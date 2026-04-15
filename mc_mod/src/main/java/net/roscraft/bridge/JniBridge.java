package net.roscraft.bridge;

import com.google.flatbuffers.FlatBufferBuilder;
import java.nio.ByteBuffer;
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
        default -> {}
      }
    }
  }

  private JniBridge() {}

  public static JniBridge create() {
    if (!nativeCreate()) {
      throw new IllegalStateException("Failed to initialise native ROS2 JNI bridge. "
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
  public long nodeInfo(String nodeName, boolean includeHidden) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(buildNodeInfo(requestId, nodeName, includeHidden));
    return requestId;
  }

  @Override
  public long topicInfo(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(buildTopicInfo(requestId, topicName));
    return requestId;
  }

  @Override
  public long serviceInfo(String serviceName) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(buildServiceInfo(requestId, serviceName));
    return requestId;
  }

  @Override
  public long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(buildInterfaceList(requestId, includeMessages, includeServices, includeActions));
    return requestId;
  }

  @Override
  public long interfaceShow(String interfaceType) {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(buildInterfaceShow(requestId, interfaceType));
    return requestId;
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

    final long requestId = nextRequestId();
    sendPacket(buildSubscribeTopic(requestId, topicName, messageType, once, timeoutSeconds, raw));
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
  public long topicHz(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(buildTopicHz(requestId, topicName, messageType, window));
    return requestId;
  }

  @Override
  public long topicBw(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(buildTopicBw(requestId, topicName, messageType, window));
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

    int nodeCount = snapshotPacket.nodesLength();
    List<net.roscraft.bridge.data.NodeEntry> nodes = new ArrayList<>(nodeCount);
    var nodeEntry = new roscraft.bridge.fbs.NodeEntry();
    for (int i = 0; i < nodeCount; ++i) {
      snapshotPacket.nodes(nodeEntry, i);
      nodes.add(new net.roscraft.bridge.data.NodeEntry(nodeEntry.name()));
    }

    int topicCount = snapshotPacket.topicsLength();
    List<net.roscraft.bridge.data.TopicEntry> topics = new ArrayList<>(topicCount);
    var topicEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < topicCount; ++i) {
      snapshotPacket.topics(topicEntry, i);
      topics.add(new net.roscraft.bridge.data.TopicEntry(topicEntry.name(), topicEntry.type()));
    }

    int serviceCount = snapshotPacket.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
      snapshotPacket.services(serviceEntry, i);
      services.add(
          new net.roscraft.bridge.data.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    int actionCount = snapshotPacket.actionsLength();
    List<net.roscraft.bridge.data.ActionEntry> actions = new ArrayList<>(actionCount);
    var actionEntry = new roscraft.bridge.fbs.ActionEntry();
    for (int i = 0; i < actionCount; ++i) {
      snapshotPacket.actions(actionEntry, i);
      actions.add(new net.roscraft.bridge.data.ActionEntry(actionEntry.name(), actionEntry.type()));
    }

    callback()
        .onGraphSnapshot(new net.roscraft.bridge.data.GraphSnapshot(
            snapshotPacket.requestId(),
            List.copyOf(nodes),
            List.copyOf(topics),
            List.copyOf(services),
            List.copyOf(actions)));
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
        .onTopicPayload(new TopicPayload(
            payloadPacket.requestId(),
            payloadPacket.topicName(),
            payloadPacket.messageType(),
            payloadPacket.raw(),
            payload));
  }

  private void handleNodeInfoResponse(BridgePacket packet) {
    var response = (NodeInfoResponsePacket) packet.payload(new NodeInfoResponsePacket());
    if (response == null) {
      return;
    }

    int publisherCount = response.publishersLength();
    List<net.roscraft.bridge.data.TopicEntry> publishers = new ArrayList<>(publisherCount);
    var publisherEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < publisherCount; ++i) {
      response.publishers(publisherEntry, i);
      publishers.add(
          new net.roscraft.bridge.data.TopicEntry(publisherEntry.name(), publisherEntry.type()));
    }

    int subscriberCount = response.subscribersLength();
    List<net.roscraft.bridge.data.TopicEntry> subscribers = new ArrayList<>(subscriberCount);
    var subscriberEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < subscriberCount; ++i) {
      response.subscribers(subscriberEntry, i);
      subscribers.add(
          new net.roscraft.bridge.data.TopicEntry(subscriberEntry.name(), subscriberEntry.type()));
    }

    int serviceCount = response.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
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

  private void handleError(BridgePacket packet) {
    var errorPacket = (ErrorPacket) packet.payload(new ErrorPacket());
    if (errorPacket == null) {
      return;
    }

    callback()
        .onError(new BridgeError(
            errorPacket.requestId(), errorPacket.errorCode(), errorPacket.errorMessage()));
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
    int topicOffset = fbb.createString(topicName);
    int payload = TopicInfoPacket.createTopicInfoPacket(fbb, requestId, topicOffset);
    return finishPacket(PacketPayload.TopicInfoPacket, payload);
  }

  private ByteBuffer buildServiceInfo(long requestId, String serviceName) {
    fbb.clear();
    int serviceOffset = fbb.createString(serviceName);
    int payload = ServiceInfoPacket.createServiceInfoPacket(fbb, requestId, serviceOffset);
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
    int typeOffset = fbb.createString(interfaceType);
    int payload = InterfaceShowPacket.createInterfaceShowPacket(fbb, requestId, typeOffset);
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
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload = SubscribeTopicPacket.createSubscribeTopicPacket(
        fbb, requestId, topicOffset, typeOffset, once, timeoutSeconds, raw);
    return finishPacket(PacketPayload.SubscribeTopicPacket, payload);
  }

  private ByteBuffer buildPublishMessage(
      long requestId, String topicName, String messageType, byte[] rawPayload) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payloadOffset = PublishMessagePacket.createPayloadVector(fbb, rawPayload);
    int payload = PublishMessagePacket.createPublishMessagePacket(
        fbb, requestId, topicOffset, typeOffset, payloadOffset);
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
