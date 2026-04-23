package net.roscraft.bridge;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import roscraft.bridge.fbs.BridgePacket;

public final class JniBridge extends RoscraftBridge {

  static {
    NativeLoader.ensureLoaded();
  }

  private final AtomicBoolean closed = new AtomicBoolean(false);
  private final FlatBufferPacketBuilder packetBuilder = new FlatBufferPacketBuilder();
  private final FlatBufferPacketDispatcher packetDispatcher =
      new FlatBufferPacketDispatcher(this::callback);

  private final class NativeCallback {

    void onPacket(byte[] packetBytes) {
      if (packetBytes == null || packetBytes.length == 0) {
        return;
      }

      var buffer = ByteBuffer.wrap(packetBytes);
      if (!BridgePacket.BridgePacketBufferHasIdentifier(buffer)) {
        return;
      }

      packetDispatcher.dispatch(BridgePacket.getRootAsBridgePacket(buffer));
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
    sendPacket(packetBuilder.queryGraph(requestId));
    return requestId;
  }

  @Override
  public long nodeInfo(String nodeName, boolean includeHidden) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(packetBuilder.nodeInfo(requestId, nodeName, includeHidden));
    return requestId;
  }

  @Override
  public long topicInfo(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(packetBuilder.topicInfo(requestId, topicName));
    return requestId;
  }

  @Override
  public long serviceInfo(String serviceName) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(packetBuilder.serviceInfo(requestId, serviceName));
    return requestId;
  }

  @Override
  public long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(
        packetBuilder.interfaceList(requestId, includeMessages, includeServices, includeActions));
    return requestId;
  }

  @Override
  public long interfaceShow(String interfaceType) {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(packetBuilder.interfaceShow(requestId, interfaceType));
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
    sendPacket(
        packetBuilder.topicSubscribe(requestId, topicName, messageType, once, timeoutSeconds, raw));
    return requestId;
  }

  @Override
  public long publishMessage(String topicName, String messageType, byte[] payload) {
    return publishMessage(topicName, messageType, payload, false, 0.0, 0, "default");
  }

  @Override
  public long publishMessage(
      String topicName,
      String messageType,
      byte[] payload,
      boolean once,
      double rateHz,
      int times,
      String qosProfile) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    Objects.requireNonNull(qosProfile, "qosProfile must not be null");
    checkOpen();

    final long requestId = nextRequestId();
    sendPacket(packetBuilder.topicPublishMessage(
        requestId, topicName, messageType, payload, once, rateHz, Math.max(0, times), qosProfile));
    return requestId;
  }

  @Override
  public long queryPlayers() {
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.queryPlayers(requestId));
    return requestId;
  }

  @Override
  public long topicHz(String topicName, String messageType, int window) {
    return topicHz(topicName, messageType, window, false);
  }

  @Override
  public long topicHz(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.topicHz(requestId, topicName, messageType, window, wallTime));
    return requestId;
  }

  @Override
  public long topicBw(String topicName, String messageType, int window) {
    return topicBw(topicName, messageType, window, false);
  }

  @Override
  public long topicBw(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.topicBw(requestId, topicName, messageType, window, wallTime));
    return requestId;
  }

  @Override
  public long topicDelay(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.topicDelay(requestId, topicName, messageType, Math.max(1, window)));
    return requestId;
  }

  @Override
  public long serviceCall(
      String serviceName,
      String serviceType,
      byte[] payload,
      double timeoutSeconds,
      int repeatCount,
      double rateHz) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    Objects.requireNonNull(serviceType, "serviceType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.serviceCall(
        requestId,
        serviceName,
        serviceType,
        payload,
        Math.max(0.0, timeoutSeconds),
        Math.max(0, repeatCount),
        Math.max(0.0, rateHz)));
    return requestId;
  }

  @Override
  public long paramList(
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.paramList(
        requestId,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0, depth),
        includeTypes,
        filterRegex == null ? "" : filterRegex,
        Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public long paramGet(String nodeName, String paramName, boolean hideType, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.paramGet(
        requestId, nodeName, paramName, hideType, Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public long paramSet(String nodeName, String paramName, String valueText, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    Objects.requireNonNull(valueText, "valueText must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.paramSet(
        requestId, nodeName, paramName, valueText, Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public long paramDescribe(String nodeName, String paramName, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(
        packetBuilder.paramDescribe(requestId, nodeName, paramName, Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public long paramDump(String nodeName, String[] prefixes, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.paramDump(
        requestId,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public long paramLoad(
      String nodeName, String yamlText, double timeoutSeconds, boolean useWildcard) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(yamlText, "yamlText must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.paramLoad(
        requestId, nodeName, yamlText, Math.max(0.0, timeoutSeconds), useWildcard));
    return requestId;
  }

  @Override
  public long actionInfo(String actionName, boolean includeHidden) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.actionInfo(requestId, actionName, includeHidden));
    return requestId;
  }

  @Override
  public long actionSendGoal(
      String actionName,
      String actionType,
      byte[] goalPayload,
      boolean feedback,
      double timeoutSeconds) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    Objects.requireNonNull(actionType, "actionType must not be null");
    Objects.requireNonNull(goalPayload, "goalPayload must not be null");
    checkOpen();
    final long requestId = nextRequestId();
    sendPacket(packetBuilder.actionSendGoal(
        requestId, actionName, actionType, goalPayload, feedback, Math.max(0.0, timeoutSeconds)));
    return requestId;
  }

  @Override
  public void close() {
    if (closed.compareAndSet(false, true)) {
      nativeDestroy();
    }
  }

  private void sendPacket(ByteBuffer packet) {
    ByteBuffer buffer = packet.duplicate();
    byte[] bytes = new byte[buffer.remaining()];
    buffer.get(bytes);
    nativeSendPacket(bytes);
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
