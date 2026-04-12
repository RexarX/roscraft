package net.roscraft.bridge;

import com.google.flatbuffers.FlatBufferBuilder;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.InetSocketAddress;
import java.net.PortUnreachableException;
import java.net.StandardProtocolFamily;
import java.net.StandardSocketOptions;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.TopicPayload;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import roscraft.bridge.fbs.BridgePacket;
import roscraft.bridge.fbs.GraphSnapshotPacket;
import roscraft.bridge.fbs.PacketPayload;
import roscraft.bridge.fbs.PlayerEntry;
import roscraft.bridge.fbs.PlayerListPacket;
import roscraft.bridge.fbs.PublishMessagePacket;
import roscraft.bridge.fbs.QueryGraphPacket;
import roscraft.bridge.fbs.QueryPlayersPacket;
import roscraft.bridge.fbs.SubscribeTopicPacket;
import roscraft.bridge.fbs.TopicPayloadPacket;

/**
 * UDP network-backed {@link RoscraftBridge} implementation.
 *
 * <p>Communicates with the standalone {@code roscraft_bridge_server} process
 * over UDP. Every datagram is a FlatBuffers {@code BridgePacket} as defined in
 * {@code roscraft/schemas/bridge_packets.fbs}. The Java sources for the
 * {@code roscraft.bridge.fbs} package are generated automatically during the
 * Gradle build by the {@code generateFlatBuffers} task.
 *
 * <p>This implementation does <em>not</em> require the native library; the ROS2
 * stack runs in a separate server process.
 *
 * <p><b>Lifecycle:</b>
 * <pre>{@code
 * var config = new NetworkBridge.Config("127.0.0.1", 7741);
 * try (var bridge = new NetworkBridge(config)) {
 *     bridge.registerCallback(myCallback);
 *     // per server tick:
 *     bridge.tick();
 * }
 * }</pre>
 *
 * <p><b>Threading:</b> {@link #tick()} must be called from a single owner
 * thread. The channel is non-blocking; {@code tick()} returns immediately if no
 * datagrams are queued.
 */
public final class NetworkBridge extends RoscraftBridge {

    private static final Logger LOGGER = LoggerFactory.getLogger(NetworkBridge.class);

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * Immutable configuration for {@link NetworkBridge}.
     *
     * @param host            Remote host running {@code roscraft_bridge_server}.
     * @param port            UDP port the server listens on.
     * @param maxDatagramSize Maximum receive buffer size in bytes (default
     *                        {@value Config#DEFAULT_MAX_DATAGRAM_SIZE}).
     */
    public record Config(String host, int port, int maxDatagramSize) {
        /** Maximum UDP datagram size under IPv4 (65 507 bytes). */
        public static final int DEFAULT_MAX_DATAGRAM_SIZE = 65_507;

        /**
         * Convenience constructor using the default datagram size.
         *
         * @param host Remote host.
         * @param port UDP port.
         */
        public Config(String host, int port) {
            this(host, port, DEFAULT_MAX_DATAGRAM_SIZE);
        }

        /** Canonical constructor — validates all fields. */
        public Config {
            Objects.requireNonNull(host, "host must not be null");
            if (port < 1 || port > 65_535) {
                throw new IllegalArgumentException("port must be in [1, 65535], got: " + port);
            }
            if (maxDatagramSize < 1) {
                throw new IllegalArgumentException(
                        "maxDatagramSize must be positive, got: " + maxDatagramSize);
            }
        }
    }

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    private final Config config;
    private final DatagramChannel channel;
    private final ByteBuffer recvBuffer;

    /**
     * Re-used FlatBufferBuilder for the send path.
     *
     * <p>Only accessed from the single owner thread that calls {@link #tick()}
     * and the send methods — no synchronisation required.
     */
    private final FlatBufferBuilder fbb = new FlatBufferBuilder(512);

    private final AtomicBoolean closed = new AtomicBoolean(false);
    private final AtomicBoolean seenInboundPacket = new AtomicBoolean(false);

    // -------------------------------------------------------------------------
    // Construction
    // -------------------------------------------------------------------------

    /**
     * Create and connect a {@link NetworkBridge} using the given configuration.
     *
     * @param config Connection parameters.
     * @throws UncheckedIOException if the underlying UDP channel cannot be
     *                              opened or connected.
     */
    public NetworkBridge(Config config) {
        this.config = Objects.requireNonNull(config, "config must not be null");
        this.recvBuffer = ByteBuffer.allocateDirect(config.maxDatagramSize());

        try {
            channel = DatagramChannel.open(StandardProtocolFamily.INET);
            channel.setOption(StandardSocketOptions.SO_REUSEADDR, true);
            channel.configureBlocking(false);
            // "connect" filters inbound datagrams to the server address and
            // lets write() send without specifying the target each time.
            channel.connect(new InetSocketAddress(config.host(), config.port()));
        } catch (IOException e) {
            throw new UncheckedIOException(
                    "Failed to open UDP channel to " + config.host() + ":" + config.port(), e);
        }
    }

    // -------------------------------------------------------------------------
    // RoscraftBridge implementation
    // -------------------------------------------------------------------------

    @Override
    public void tick() {
        checkOpen();
        drainInbound();
    }

    @Override
    public long queryGraph() {
        checkOpen();
        long id = nextRequestId();
        sendPacket(buildQueryGraph(id));
        return id;
    }

    @Override
    public long subscribeTopic(String topicName, String messageType) {
        Objects.requireNonNull(topicName, "topicName must not be null");
        Objects.requireNonNull(messageType, "messageType must not be null");
        checkOpen();
        long id = nextRequestId();
        sendPacket(buildSubscribeTopic(id, topicName, messageType));
        return id;
    }

    @Override
    public long publishMessage(String topicName, String messageType, byte[] payload) {
        Objects.requireNonNull(topicName, "topicName must not be null");
        Objects.requireNonNull(messageType, "messageType must not be null");
        Objects.requireNonNull(payload, "payload must not be null");
        checkOpen();
        long id = nextRequestId();
        sendPacket(buildPublishMessage(id, topicName, messageType, payload));
        return id;
    }

    @Override
    public long queryPlayers() {
        checkOpen();
        long id = nextRequestId();
        sendPacket(buildQueryPlayers(id));
        return id;
    }

    @Override
    public void close() {
        if (closed.compareAndSet(false, true)) {
            try {
                channel.close();
            } catch (IOException e) {
                LOGGER.warn("Error closing UDP channel: {}", describeException(e));
            }
        }
    }

    /** Returns the configuration this bridge was created with. */
    public Config config() {
        return config;
    }

    /** Returns whether this bridge has received at least one inbound packet. */
    public boolean hasSeenInboundTraffic() {
        return seenInboundPacket.get();
    }

    // -------------------------------------------------------------------------
    // Inbound — receive and dispatch
    // -------------------------------------------------------------------------

    /**
     * Non-blocking drain of all queued inbound datagrams.
     *
     * <p>Each datagram is verified and decoded as a FlatBuffers
     * {@link BridgePacket}, then dispatched to the matching handler.
     */
    private void drainInbound() {
        try {
            while (true) {
                recvBuffer.clear();
                if (channel.receive(recvBuffer) == null) {
                    break;
                }
                seenInboundPacket.set(true);
                recvBuffer.flip();

                // FlatBuffers needs a read-only view with position=0 and a
                // backing array, or a ByteBuffer with the correct base offset.
                // We copy into a heap buffer so the generated accessor methods
                // (which use absolute byte[] indexing) work correctly.
                byte[] bytes = new byte[recvBuffer.remaining()];
                recvBuffer.get(bytes);
                dispatch(ByteBuffer.wrap(bytes));
            }
        } catch (IOException e) {
            if (e instanceof PortUnreachableException) {
                LOGGER.warn(
                        "UDP destination unreachable at udp://{}:{}; "
                                + "verify bridge host/port and firewall/WSL networking.",
                        config.host(),
                        config.port());
                return;
            }
            LOGGER.warn(
                    "Receive error from udp://{}:{}: {}",
                    config.host(),
                    config.port(),
                    describeException(e));
        }
    }

    /**
     * Decode a single inbound datagram and invoke the matching callback.
     *
     * @param buf Heap {@link ByteBuffer} containing exactly one datagram.
     */
    private void dispatch(ByteBuffer buf) {
        // Validate the file identifier ("RCFT") and buffer integrity.
        if (!BridgePacket.BridgePacketBufferHasIdentifier(buf)) {
            LOGGER.warn("Dropping datagram with unknown file identifier.");
            return;
        }

        BridgePacket packet = BridgePacket.getRootAsBridgePacket(buf);

        switch (packet.payloadType()) {
            case PacketPayload.GraphSnapshotPacket -> handleGraphSnapshot(packet);
            case PacketPayload.TopicPayloadPacket -> handleTopicPayload(packet);
            case PacketPayload.PlayerListPacket -> handlePlayerList(packet);
                // Outgoing-only packets arriving unexpectedly are silently ignored.
            default -> {}
        }
    }

    // ---- Inbound packet handlers -------------------------------------------

    private void handleGraphSnapshot(BridgePacket packet) {
        var snap = (GraphSnapshotPacket) packet.payload(new GraphSnapshotPacket());
        if (snap == null) return;

        var topics = readStringVector(snap.topicsLength(), snap::topics);
        var services = readStringVector(snap.servicesLength(), snap::services);
        var actions = readStringVector(snap.actionsLength(), snap::actions);

        callback().onGraphSnapshot(new GraphSnapshot(snap.requestId(), topics, services, actions));
    }

    private void handleTopicPayload(BridgePacket packet) {
        var tp = (TopicPayloadPacket) packet.payload(new TopicPayloadPacket());
        if (tp == null) return;

        byte[] payload = new byte[tp.payloadLength()];
        for (int i = 0; i < tp.payloadLength(); i++) {
            payload[i] = (byte) tp.payload(i);
        }

        callback().onTopicPayload(new TopicPayload(tp.topicName(), tp.messageType(), payload));
    }

    private void handlePlayerList(BridgePacket packet) {
        var plp = (PlayerListPacket) packet.payload(new PlayerListPacket());
        if (plp == null) return;

        int count = plp.playersLength();
        List<Player> players = new ArrayList<>(count);
        var entry = new PlayerEntry();
        for (int i = 0; i < count; i++) {
            plp.players(entry, i);
            players.add(new Player(entry.name(), entry.x(), entry.y(), entry.z()));
        }

        callback().onPlayerList(new PlayerList(plp.requestId(), players));
    }

    // -------------------------------------------------------------------------
    // Outbound — FlatBuffers packet builders
    // -------------------------------------------------------------------------
    //
    // Each builder method:
    //   1. Clears and resets the shared FlatBufferBuilder.
    //   2. Builds the inner payload table.
    //   3. Wraps it in a BridgePacket root table.
    //   4. Returns the finished buffer as a ByteBuffer ready for channel.write().

    private ByteBuffer buildQueryGraph(long requestId) {
        fbb.clear();
        int payload = QueryGraphPacket.createQueryGraphPacket(fbb, requestId);
        return finishPacket(PacketPayload.QueryGraphPacket, payload);
    }

    private ByteBuffer buildSubscribeTopic(long requestId, String topicName, String messageType) {
        fbb.clear();
        int tnOffset = fbb.createString(topicName);
        int mtOffset = fbb.createString(messageType);
        int payload =
                SubscribeTopicPacket.createSubscribeTopicPacket(fbb, requestId, tnOffset, mtOffset);
        return finishPacket(PacketPayload.SubscribeTopicPacket, payload);
    }

    private ByteBuffer buildPublishMessage(
            long requestId, String topicName, String messageType, byte[] rawPayload) {
        fbb.clear();
        int tnOffset = fbb.createString(topicName);
        int mtOffset = fbb.createString(messageType);
        int payloadOffset = PublishMessagePacket.createPayloadVector(fbb, rawPayload);
        int payload =
                PublishMessagePacket.createPublishMessagePacket(
                        fbb, requestId, tnOffset, mtOffset, payloadOffset);
        return finishPacket(PacketPayload.PublishMessagePacket, payload);
    }

    private ByteBuffer buildQueryPlayers(long requestId) {
        fbb.clear();
        int payload = QueryPlayersPacket.createQueryPlayersPacket(fbb, requestId);
        return finishPacket(PacketPayload.QueryPlayersPacket, payload);
    }

    /**
     * Wrap a finished inner-table offset in a {@link BridgePacket} root table,
     * finish the buffer with the {@code "RCFT"} file identifier, and return the
     * resulting {@link ByteBuffer} positioned at byte 0.
     *
     * @param payloadType One of the {@link PacketPayload} union constants.
     * @param payloadOffset FlatBuffers offset of the inner payload table.
     * @return Finished, flipped {@link ByteBuffer} ready for {@code channel.write()}.
     */
    private ByteBuffer finishPacket(byte payloadType, int payloadOffset) {
        int root = BridgePacket.createBridgePacket(fbb, payloadType, payloadOffset);
        BridgePacket.finishBridgePacketBuffer(fbb, root);
        return fbb.dataBuffer();
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /**
     * Send a finished FlatBuffers buffer over the UDP channel.
     *
     * @param buf Buffer positioned at byte 0, limit = packet length.
     */
    private void sendPacket(ByteBuffer buf) {
        try {
            channel.write(buf);
        } catch (IOException e) {
            LOGGER.warn(
                    "Send error to udp://{}:{}: {}",
                    config.host(),
                    config.port(),
                    describeException(e));
        }
    }

    /**
     * Extract a FlatBuffers string vector into an immutable {@link List}.
     *
     * @param length   Number of elements in the vector.
     * @param accessor Method reference of the form {@code table::fieldName}.
     * @return Unmodifiable list of the string values.
     */
    private static List<String> readStringVector(
            int length, java.util.function.IntFunction<String> accessor) {
        List<String> result = new ArrayList<>(length);
        for (int i = 0; i < length; i++) {
            result.add(accessor.apply(i));
        }
        return List.copyOf(result);
    }

    private void checkOpen() {
        if (closed.get()) {
            throw new IllegalStateException("NetworkBridge has been closed.");
        }
    }

    private static String describeException(Throwable throwable) {
        String message = throwable.getMessage();
        if (message == null || message.isBlank()) {
            return throwable.getClass().getSimpleName();
        }
        return throwable.getClass().getSimpleName() + ": " + message;
    }
}
