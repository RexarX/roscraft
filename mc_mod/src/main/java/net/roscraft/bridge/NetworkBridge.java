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
import net.roscraft.bridge.data.BridgeError;
import net.roscraft.bridge.data.InterfaceListResponse;
import net.roscraft.bridge.data.InterfaceShowResponse;
import net.roscraft.bridge.data.NodeInfoResponse;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.ServiceInfoResponse;
import net.roscraft.bridge.data.TopicBwResponse;
import net.roscraft.bridge.data.TopicHzResponse;
import net.roscraft.bridge.data.TopicInfoResponse;
import net.roscraft.bridge.data.TopicPayload;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import roscraft.bridge.fbs.BridgePacket;
import roscraft.bridge.fbs.ErrorPacket;
import roscraft.bridge.fbs.GraphSnapshotPacket;
import roscraft.bridge.fbs.InterfaceListPacket;
import roscraft.bridge.fbs.InterfaceListResponsePacket;
import roscraft.bridge.fbs.InterfaceShowPacket;
import roscraft.bridge.fbs.InterfaceShowResponsePacket;
import roscraft.bridge.fbs.NodeInfoPacket;
import roscraft.bridge.fbs.NodeInfoResponsePacket;
import roscraft.bridge.fbs.PacketPayload;
import roscraft.bridge.fbs.PlayerEntry;
import roscraft.bridge.fbs.PlayerListPacket;
import roscraft.bridge.fbs.PublishMessagePacket;
import roscraft.bridge.fbs.QueryGraphPacket;
import roscraft.bridge.fbs.QueryPlayersPacket;
import roscraft.bridge.fbs.ServiceInfoPacket;
import roscraft.bridge.fbs.ServiceInfoResponsePacket;
import roscraft.bridge.fbs.SubscribeTopicPacket;
import roscraft.bridge.fbs.TopicBwPacket;
import roscraft.bridge.fbs.TopicBwResponsePacket;
import roscraft.bridge.fbs.TopicHzPacket;
import roscraft.bridge.fbs.TopicHzResponsePacket;
import roscraft.bridge.fbs.TopicInfoPacket;
import roscraft.bridge.fbs.TopicInfoResponsePacket;
import roscraft.bridge.fbs.TopicPayloadPacket;

/**
 * UDP network-backed {@link RoscraftBridge} implementation.
 *
 * <p>
 * Communicates with the standalone {@code roscraft_bridge_server} process over
 * UDP. Every datagram is a FlatBuffers {@code BridgePacket} as defined in
 * {@code roscraft/schemas/bridge_packets.fbs}. The Java sources for the
 * {@code roscraft.bridge.fbs} package are generated automatically during the
 * Gradle build by the {@code generateFlatBuffers} task.
 *
 * <p>
 * This implementation does <em>not</em> require the native library; the ROS2
 * stack runs in a separate server process.
 *
 * <p>
 * <b>Lifecycle:</b>
 *
 * <pre>{@code
 * var config = new NetworkBridge.Config("127.0.0.1", 7741);
 * try (var bridge = new NetworkBridge(config)) {
 *     bridge.registerCallback(myCallback);
 *     // per server tick:
 *     bridge.tick();
 * }
 * }</pre>
 *
 * <p>
 * <b>Threading:</b> {@link #tick()} must be called from a single owner thread.
 * The channel is non-blocking; {@code tick()} returns immediately if no
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
   * @param host
   *            Remote host running {@code roscraft_bridge_server}.
   * @param port
   *            UDP port the server listens on.
   * @param maxDatagramSize
   *            Maximum receive buffer size in bytes (default
   *            {@value Config#DEFAULT_MAX_DATAGRAM_SIZE}).
   */
  public record Config(String host, int port, int maxDatagramSize) {
    /** Maximum UDP datagram size under IPv4 (65 507 bytes). */
    public static final int DEFAULT_MAX_DATAGRAM_SIZE = 65_507;

    /**
     * Convenience constructor using the default datagram size.
     *
     * @param host
     *            Remote host.
     * @param port
     *            UDP port.
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
   * <p>
   * Only accessed from the single owner thread that calls {@link #tick()} and the
   * send methods — no synchronisation required.
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
   * @param config
   *            Connection parameters.
   * @throws UncheckedIOException
   *             if the underlying UDP channel cannot be opened or connected.
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
  public long nodeInfo(String nodeName, boolean includeHidden) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildNodeInfo(id, nodeName, includeHidden));
    return id;
  }

  @Override
  public long topicInfo(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildTopicInfo(id, topicName));
    return id;
  }

  @Override
  public long serviceInfo(String serviceName) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildServiceInfo(id, serviceName));
    return id;
  }

  @Override
  public long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildInterfaceList(id, includeMessages, includeServices, includeActions));
    return id;
  }

  @Override
  public long interfaceShow(String interfaceType) {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildInterfaceShow(id, interfaceType));
    return id;
  }

  @Override
  public long subscribeTopic(String topicName, String messageType) {
    return subscribeTopic(topicName, messageType, false, 0.0, false);
  }

  @Override
  public long subscribeTopic(
      String topicName, String messageType, boolean once, double timeoutSeconds, boolean raw) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildSubscribeTopic(id, topicName, messageType, once, timeoutSeconds, raw));
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
  public long topicHz(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildTopicHz(id, topicName, messageType, window));
    return id;
  }

  @Override
  public long topicBw(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(buildTopicBw(id, topicName, messageType, window));
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
   * <p>
   * Each datagram is verified and decoded as a FlatBuffers {@link BridgePacket},
   * then dispatched to the matching handler.
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
          "Receive error from udp://{}:{}: {}", config.host(), config.port(), describeException(e));
    }
  }

  /**
   * Decode a single inbound datagram and invoke the matching callback.
   *
   * @param buf
   *            Heap {@link ByteBuffer} containing exactly one datagram.
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
      case PacketPayload.NodeInfoResponsePacket -> handleNodeInfoResponse(packet);
      case PacketPayload.TopicInfoResponsePacket -> handleTopicInfoResponse(packet);
      case PacketPayload.ServiceInfoResponsePacket -> handleServiceInfoResponse(packet);
      case PacketPayload.InterfaceListResponsePacket -> handleInterfaceListResponse(packet);
      case PacketPayload.InterfaceShowResponsePacket -> handleInterfaceShowResponse(packet);
      case PacketPayload.TopicPayloadPacket -> handleTopicPayload(packet);
      case PacketPayload.PlayerListPacket -> handlePlayerList(packet);
      case PacketPayload.ErrorPacket -> handleError(packet);
      case PacketPayload.TopicHzResponsePacket -> handleTopicHzResponse(packet);
      case PacketPayload.TopicBwResponsePacket -> handleTopicBwResponse(packet);
        // Outgoing-only packets arriving unexpectedly are silently ignored.
      default -> {}
    }
  }

  // ---- Inbound packet handlers -------------------------------------------

  private void handleGraphSnapshot(BridgePacket packet) {
    var snap = (GraphSnapshotPacket) packet.payload(new GraphSnapshotPacket());
    if (snap == null) return;

    int nodeCount = snap.nodesLength();
    List<net.roscraft.bridge.data.NodeEntry> nodes = new ArrayList<>(nodeCount);
    var nodeEntry = new roscraft.bridge.fbs.NodeEntry();
    for (int i = 0; i < nodeCount; i++) {
      snap.nodes(nodeEntry, i);
      nodes.add(new net.roscraft.bridge.data.NodeEntry(nodeEntry.name()));
    }

    int topicCount = snap.topicsLength();
    List<net.roscraft.bridge.data.TopicEntry> topics = new ArrayList<>(topicCount);
    var topicEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < topicCount; i++) {
      snap.topics(topicEntry, i);
      topics.add(new net.roscraft.bridge.data.TopicEntry(topicEntry.name(), topicEntry.type()));
    }

    int serviceCount = snap.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; i++) {
      snap.services(serviceEntry, i);
      services.add(
          new net.roscraft.bridge.data.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    int actionCount = snap.actionsLength();
    List<net.roscraft.bridge.data.ActionEntry> actions = new ArrayList<>(actionCount);
    var actionEntry = new roscraft.bridge.fbs.ActionEntry();
    for (int i = 0; i < actionCount; i++) {
      snap.actions(actionEntry, i);
      actions.add(new net.roscraft.bridge.data.ActionEntry(actionEntry.name(), actionEntry.type()));
    }

    callback()
        .onGraphSnapshot(new net.roscraft.bridge.data.GraphSnapshot(
            snap.requestId(),
            List.copyOf(nodes),
            List.copyOf(topics),
            List.copyOf(services),
            List.copyOf(actions)));
  }

  private void handleTopicPayload(BridgePacket packet) {
    var tp = (TopicPayloadPacket) packet.payload(new TopicPayloadPacket());
    if (tp == null) return;

    byte[] payload = new byte[tp.payloadLength()];
    for (int i = 0; i < tp.payloadLength(); i++) {
      payload[i] = (byte) tp.payload(i);
    }

    callback()
        .onTopicPayload(
            new TopicPayload(tp.requestId(), tp.topicName(), tp.messageType(), tp.raw(), payload));
  }

  private void handleNodeInfoResponse(BridgePacket packet) {
    var response = (NodeInfoResponsePacket) packet.payload(new NodeInfoResponsePacket());
    if (response == null) {
      return;
    }

    int publisherCount = response.publishersLength();
    List<net.roscraft.bridge.data.TopicEntry> publishers = new ArrayList<>(publisherCount);
    var publisherEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < publisherCount; i++) {
      response.publishers(publisherEntry, i);
      publishers.add(
          new net.roscraft.bridge.data.TopicEntry(publisherEntry.name(), publisherEntry.type()));
    }

    int subscriberCount = response.subscribersLength();
    List<net.roscraft.bridge.data.TopicEntry> subscribers = new ArrayList<>(subscriberCount);
    var subscriberEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < subscriberCount; i++) {
      response.subscribers(subscriberEntry, i);
      subscribers.add(
          new net.roscraft.bridge.data.TopicEntry(subscriberEntry.name(), subscriberEntry.type()));
    }

    int serviceCount = response.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; i++) {
      response.services(serviceEntry, i);
      services.add(
          new net.roscraft.bridge.data.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    callback()
        .onNodeInfoResponse(new NodeInfoResponse(
            response.requestId(),
            response.nodeName(),
            List.copyOf(publishers),
            List.copyOf(subscribers),
            List.copyOf(services),
            response.found()));
  }

  private void handleTopicInfoResponse(BridgePacket packet) {
    var response = (TopicInfoResponsePacket) packet.payload(new TopicInfoResponsePacket());
    if (response == null) {
      return;
    }

    List<String> publisherNodes =
        readStringVector(response.publisherNodesLength(), response::publisherNodes);
    List<String> subscriberNodes =
        readStringVector(response.subscriberNodesLength(), response::subscriberNodes);

    callback()
        .onTopicInfoResponse(new TopicInfoResponse(
            response.requestId(),
            response.topicName(),
            response.messageType(),
            response.publisherCount(),
            response.subscriberCount(),
            publisherNodes,
            subscriberNodes));
  }

  private void handleServiceInfoResponse(BridgePacket packet) {
    var response = (ServiceInfoResponsePacket) packet.payload(new ServiceInfoResponsePacket());
    if (response == null) {
      return;
    }

    List<String> clientNodes =
        readStringVector(response.clientNodesLength(), response::clientNodes);
    List<String> serverNodes =
        readStringVector(response.serverNodesLength(), response::serverNodes);

    callback()
        .onServiceInfoResponse(new ServiceInfoResponse(
            response.requestId(),
            response.serviceName(),
            response.serviceType(),
            response.clientCount(),
            response.serverCount(),
            clientNodes,
            serverNodes));
  }

  private void handleInterfaceListResponse(BridgePacket packet) {
    var response = (InterfaceListResponsePacket) packet.payload(new InterfaceListResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onInterfaceListResponse(new InterfaceListResponse(
            response.requestId(),
            readStringVector(response.messagesLength(), response::messages),
            readStringVector(response.servicesLength(), response::services),
            readStringVector(response.actionsLength(), response::actions)));
  }

  private void handleInterfaceShowResponse(BridgePacket packet) {
    var response = (InterfaceShowResponsePacket) packet.payload(new InterfaceShowResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onInterfaceShowResponse(new InterfaceShowResponse(
            response.requestId(),
            response.interfaceType(),
            response.definition(),
            response.found()));
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

  private void handleError(BridgePacket packet) {
    var err = (ErrorPacket) packet.payload(new ErrorPacket());
    if (err == null) return;

    callback().onError(new BridgeError(err.requestId(), err.errorCode(), err.errorMessage()));
  }

  private void handleTopicHzResponse(BridgePacket packet) {
    var response = (TopicHzResponsePacket) packet.payload(new TopicHzResponsePacket());
    if (response == null) {
      return;
    }
    callback()
        .onTopicHzResponse(new TopicHzResponse(
            response.requestId(),
            response.topicName(),
            response.frequency(),
            (int) response.window(),
            (int) response.messageCount()));
  }

  private void handleTopicBwResponse(BridgePacket packet) {
    var response = (TopicBwResponsePacket) packet.payload(new TopicBwResponsePacket());
    if (response == null) {
      return;
    }
    callback()
        .onTopicBwResponse(new TopicBwResponse(
            response.requestId(),
            response.topicName(),
            response.bytesPerSecond(),
            (int) response.window(),
            (int) response.messageCount(),
            response.totalBytes()));
  }

  // -------------------------------------------------------------------------
  // Outbound — FlatBuffers packet builders
  // -------------------------------------------------------------------------
  //
  // Each builder method:
  // 1. Clears and resets the shared FlatBufferBuilder.
  // 2. Builds the inner payload table.
  // 3. Wraps it in a BridgePacket root table.
  // 4. Returns the finished buffer as a ByteBuffer ready for channel.write().

  private ByteBuffer buildQueryGraph(long requestId) {
    fbb.clear();
    int payload = QueryGraphPacket.createQueryGraphPacket(fbb, requestId);
    return finishPacket(PacketPayload.QueryGraphPacket, payload);
  }

  private ByteBuffer buildNodeInfo(long requestId, String nodeName, boolean includeHidden) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int payload =
        NodeInfoPacket.createNodeInfoPacket(fbb, requestId, nodeNameOffset, includeHidden);
    return finishPacket(PacketPayload.NodeInfoPacket, payload);
  }

  private ByteBuffer buildTopicInfo(long requestId, String topicName) {
    fbb.clear();
    int topicNameOffset = fbb.createString(topicName);
    int payload = TopicInfoPacket.createTopicInfoPacket(fbb, requestId, topicNameOffset);
    return finishPacket(PacketPayload.TopicInfoPacket, payload);
  }

  private ByteBuffer buildServiceInfo(long requestId, String serviceName) {
    fbb.clear();
    int serviceNameOffset = fbb.createString(serviceName);
    int payload = ServiceInfoPacket.createServiceInfoPacket(fbb, requestId, serviceNameOffset);
    return finishPacket(PacketPayload.ServiceInfoPacket, payload);
  }

  private ByteBuffer buildInterfaceList(
      long requestId, boolean includeMessages, boolean includeServices, boolean includeActions) {
    fbb.clear();
    int payload = InterfaceListPacket.createInterfaceListPacket(
        fbb, requestId, includeMessages, includeServices, includeActions);
    return finishPacket(PacketPayload.InterfaceListPacket, payload);
  }

  private ByteBuffer buildInterfaceShow(long requestId, String interfaceType) {
    fbb.clear();
    int interfaceTypeOffset = fbb.createString(interfaceType);
    int payload =
        InterfaceShowPacket.createInterfaceShowPacket(fbb, requestId, interfaceTypeOffset);
    return finishPacket(PacketPayload.InterfaceShowPacket, payload);
  }

  private ByteBuffer buildSubscribeTopic(
      long requestId,
      String topicName,
      String messageType,
      boolean once,
      double timeoutSeconds,
      boolean raw) {
    fbb.clear();
    int tnOffset = fbb.createString(topicName);
    int mtOffset = fbb.createString(messageType);
    int payload = SubscribeTopicPacket.createSubscribeTopicPacket(
        fbb, requestId, tnOffset, mtOffset, once, timeoutSeconds, raw);
    return finishPacket(PacketPayload.SubscribeTopicPacket, payload);
  }

  private ByteBuffer buildPublishMessage(
      long requestId, String topicName, String messageType, byte[] rawPayload) {
    fbb.clear();
    int tnOffset = fbb.createString(topicName);
    int mtOffset = fbb.createString(messageType);
    int payloadOffset = PublishMessagePacket.createPayloadVector(fbb, rawPayload);
    int payload = PublishMessagePacket.createPublishMessagePacket(
        fbb, requestId, tnOffset, mtOffset, payloadOffset);
    return finishPacket(PacketPayload.PublishMessagePacket, payload);
  }

  private ByteBuffer buildQueryPlayers(long requestId) {
    fbb.clear();
    int payload = QueryPlayersPacket.createQueryPlayersPacket(fbb, requestId);
    return finishPacket(PacketPayload.QueryPlayersPacket, payload);
  }

  private ByteBuffer buildTopicHz(
      long requestId, String topicName, String messageType, int window) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload =
        TopicHzPacket.createTopicHzPacket(fbb, requestId, topicOffset, typeOffset, window);
    return finishPacket(PacketPayload.TopicHzPacket, payload);
  }

  private ByteBuffer buildTopicBw(
      long requestId, String topicName, String messageType, int window) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload =
        TopicBwPacket.createTopicBwPacket(fbb, requestId, topicOffset, typeOffset, window);
    return finishPacket(PacketPayload.TopicBwPacket, payload);
  }

  /**
   * Wrap a finished inner-table offset in a {@link BridgePacket} root table,
   * finish the buffer with the {@code "RCFT"} file identifier, and return the
   * resulting {@link ByteBuffer} positioned at byte 0.
   *
   * @param payloadType
   *            One of the {@link PacketPayload} union constants.
   * @param payloadOffset
   *            FlatBuffers offset of the inner payload table.
   * @return Finished, flipped {@link ByteBuffer} ready for
   *         {@code channel.write()}.
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
   * @param buf
   *            Buffer positioned at byte 0, limit = packet length.
   */
  private void sendPacket(ByteBuffer buf) {
    try {
      channel.write(buf);
    } catch (IOException e) {
      LOGGER.warn(
          "Send error to udp://{}:{}: {}", config.host(), config.port(), describeException(e));
    }
  }

  /**
   * Extract a FlatBuffers string vector into an immutable {@link List}.
   *
   * @param length
   *            Number of elements in the vector.
   * @param accessor
   *            Method reference of the form {@code table::fieldName}.
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
