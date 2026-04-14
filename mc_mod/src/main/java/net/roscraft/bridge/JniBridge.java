package net.roscraft.bridge;

import com.google.flatbuffers.FlatBufferBuilder;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.TopicPayload;
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

public final class JniBridge extends RoscraftBridge {

    static {
        NativeLoader.ensureLoaded();
    }

    private final FlatBufferBuilder fbb = new FlatBufferBuilder(512);
    private final AtomicBoolean closed = new AtomicBoolean(false);

    private final class NativeCallback {

        void onPacket(byte[] packetBytes) {
            if (packetBytes == null || packetBytes.length == 0) {
                return;
            }

            var buffer = ByteBuffer.wrap(packetBytes);
            if (!BridgePacket.BridgePacketBufferHasIdentifier(buffer)) {
                return;
            }

            var packet = BridgePacket.getRootAsBridgePacket(buffer);
            switch (packet.payloadType()) {
                case PacketPayload.GraphSnapshotPacket -> handleGraphSnapshot(packet);
                case PacketPayload.TopicPayloadPacket -> handleTopicPayload(packet);
                case PacketPayload.PlayerListPacket -> handlePlayerList(packet);
                default -> {}
            }
        }
    }

    private JniBridge() {}

    public static JniBridge create() {
        if (!nativeCreate()) {
            throw new IllegalStateException(
                    "Failed to initialise native ROS2 JNI bridge. "
                            + "Check that ROS2 is installed and sourced correctly.");
        }
        var bridge = new JniBridge();
        nativeRegisterCallback(bridge.new NativeCallback());
        return bridge;
    }

    @Override
    public void tick() {
        checkOpen();
        nativeTick();
    }

    @Override
    public long queryGraph() {
        checkOpen();
        final long requestId = nextRequestId();
        sendPacket(buildQueryGraph(requestId));
        return requestId;
    }

    @Override
    public long subscribeTopic(String topicName, String messageType) {
        Objects.requireNonNull(topicName, "topicName must not be null");
        Objects.requireNonNull(messageType, "messageType must not be null");
        checkOpen();

        final long requestId = nextRequestId();
        sendPacket(buildSubscribeTopic(requestId, topicName, messageType));
        return requestId;
    }

    @Override
    public long publishMessage(String topicName, String messageType, byte[] payload) {
        Objects.requireNonNull(topicName, "topicName must not be null");
        Objects.requireNonNull(messageType, "messageType must not be null");
        Objects.requireNonNull(payload, "payload must not be null");
        checkOpen();

        final long requestId = nextRequestId();
        sendPacket(buildPublishMessage(requestId, topicName, messageType, payload));
        return requestId;
    }

    @Override
    public long queryPlayers() {
        checkOpen();
        final long requestId = nextRequestId();
        sendPacket(buildQueryPlayers(requestId));
        return requestId;
    }

    @Override
    public void close() {
        if (closed.compareAndSet(false, true)) {
            nativeDestroy();
        }
    }

    private void handleGraphSnapshot(BridgePacket packet) {
        var snapshotPacket = (GraphSnapshotPacket) packet.payload(new GraphSnapshotPacket());
        if (snapshotPacket == null) {
            return;
        }

        var topics = readStringVector(snapshotPacket.topicsLength(), snapshotPacket::topics);
        var services = readStringVector(snapshotPacket.servicesLength(), snapshotPacket::services);
        var actions = readStringVector(snapshotPacket.actionsLength(), snapshotPacket::actions);

        callback()
                .onGraphSnapshot(
                        new GraphSnapshot(snapshotPacket.requestId(), topics, services, actions));
    }

    private void handleTopicPayload(BridgePacket packet) {
        var payloadPacket = (TopicPayloadPacket) packet.payload(new TopicPayloadPacket());
        if (payloadPacket == null) {
            return;
        }

        byte[] payload = new byte[payloadPacket.payloadLength()];
        for (int i = 0; i < payloadPacket.payloadLength(); ++i) {
            payload[i] = (byte) payloadPacket.payload(i);
        }

        callback()
                .onTopicPayload(
                        new TopicPayload(
                                payloadPacket.topicName(), payloadPacket.messageType(), payload));
    }

    private void handlePlayerList(BridgePacket packet) {
        var playerListPacket = (PlayerListPacket) packet.payload(new PlayerListPacket());
        if (playerListPacket == null) {
            return;
        }

        int count = playerListPacket.playersLength();
        List<Player> players = new ArrayList<>(count);
        var entry = new PlayerEntry();
        for (int i = 0; i < count; ++i) {
            playerListPacket.players(entry, i);
            players.add(new Player(entry.name(), entry.x(), entry.y(), entry.z()));
        }

        callback().onPlayerList(new PlayerList(playerListPacket.requestId(), players));
    }

    private ByteBuffer buildQueryGraph(long requestId) {
        fbb.clear();
        int payload = QueryGraphPacket.createQueryGraphPacket(fbb, requestId);
        return finishPacket(PacketPayload.QueryGraphPacket, payload);
    }

    private ByteBuffer buildSubscribeTopic(long requestId, String topicName, String messageType) {
        fbb.clear();
        int topicOffset = fbb.createString(topicName);
        int typeOffset = fbb.createString(messageType);
        int payload =
                SubscribeTopicPacket.createSubscribeTopicPacket(
                        fbb, requestId, topicOffset, typeOffset);
        return finishPacket(PacketPayload.SubscribeTopicPacket, payload);
    }

    private ByteBuffer buildPublishMessage(
            long requestId, String topicName, String messageType, byte[] rawPayload) {
        fbb.clear();
        int topicOffset = fbb.createString(topicName);
        int typeOffset = fbb.createString(messageType);
        int payloadOffset = PublishMessagePacket.createPayloadVector(fbb, rawPayload);
        int payload =
                PublishMessagePacket.createPublishMessagePacket(
                        fbb, requestId, topicOffset, typeOffset, payloadOffset);
        return finishPacket(PacketPayload.PublishMessagePacket, payload);
    }

    private ByteBuffer buildQueryPlayers(long requestId) {
        fbb.clear();
        int payload = QueryPlayersPacket.createQueryPlayersPacket(fbb, requestId);
        return finishPacket(PacketPayload.QueryPlayersPacket, payload);
    }

    private ByteBuffer finishPacket(byte payloadType, int payloadOffset) {
        int root = BridgePacket.createBridgePacket(fbb, payloadType, payloadOffset);
        BridgePacket.finishBridgePacketBuffer(fbb, root);
        return fbb.dataBuffer();
    }

    private void sendPacket(ByteBuffer packet) {
        ByteBuffer buffer = packet.duplicate();
        byte[] bytes = new byte[buffer.remaining()];
        buffer.get(bytes);
        nativeSendPacket(bytes);
    }

    private static List<String> readStringVector(
            int length, java.util.function.IntFunction<String> accessor) {
        List<String> values = new ArrayList<>(length);
        for (int i = 0; i < length; ++i) {
            values.add(accessor.apply(i));
        }
        return List.copyOf(values);
    }

    private void checkOpen() {
        if (closed.get()) {
            throw new IllegalStateException("JniBridge has been closed.");
        }
    }

    private static native boolean nativeCreate();

    private static native void nativeDestroy();

    private static native void nativeRegisterCallback(Object callback);

    private static native void nativeTick();

    private static native void nativeSendPacket(byte[] packet);
}
