package net.roscraft.bridge;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Shared base class for bridge transport implementations.
 *
 * <p>Implements every {@link RoscraftBridge} ROS-operation method exactly
 * once by delegating packet building to {@link FlatBufferPacketBuilder}.
 * Subclasses only need to implement {@link #sendPacket(ByteBuffer)} and
 * {@link #tick()} (and optionally override {@link #pollInbound()}).
 */
public abstract class AbstractPacketBridge extends RoscraftBridge {

  protected final FlatBufferPacketBuilder packetBuilder = new FlatBufferPacketBuilder();
  protected final FlatBufferPacketDispatcher packetDispatcher =
      new FlatBufferPacketDispatcher(this::callback);
  protected final AtomicBoolean closed = new AtomicBoolean(false);

  protected abstract void sendPacket(ByteBuffer buf);

  @Override
  public abstract void tick();

  protected void pollInbound() {}

  protected void checkOpen(String name) {
    if (closed.get()) {
      throw new IllegalStateException(name + " has been closed.");
    }
  }

  // ── Core operations ─────────────────────────────────────────────────

  @Override
  public long queryGraph() {
    long id = nextRequestId();
    sendPacket(packetBuilder.queryGraph(id));
    return id;
  }

  @Override
  public long nodeInfo(String nodeName, boolean includeHidden) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.nodeInfo(id, nodeName, includeHidden));
    return id;
  }

  @Override
  public long topicInfo(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicInfo(id, topicName));
    return id;
  }

  @Override
  public long serviceInfo(String serviceName) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.serviceInfo(id, serviceName));
    return id;
  }

  @Override
  public long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    long id = nextRequestId();
    sendPacket(packetBuilder.interfaceList(id, includeMessages, includeServices, includeActions));
    return id;
  }

  @Override
  public long interfaceShow(String interfaceType) {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.interfaceShow(id, interfaceType));
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
    long id = nextRequestId();
    sendPacket(packetBuilder.topicSubscribe(id, topicName, messageType, once, timeoutSeconds, raw));
    return id;
  }

  @Override
  public long unsubscribeTopic(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicUnsubscribe(id, topicName));
    return id;
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
    long id = nextRequestId();
    sendPacket(packetBuilder.topicPublishMessage(
        id, topicName, messageType, payload, once, rateHz, Math.max(0, times), qosProfile));
    return id;
  }

  @Override
  public long queryPlayers() {
    long id = nextRequestId();
    sendPacket(packetBuilder.queryPlayers(id));
    return id;
  }

  // ── Topic statistics ────────────────────────────────────────────────

  @Override
  public long topicHz(String topicName, String messageType, int window) {
    return topicHz(topicName, messageType, window, false);
  }

  @Override
  public long topicHz(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicHz(id, topicName, messageType, window, wallTime));
    return id;
  }

  @Override
  public long topicBw(String topicName, String messageType, int window) {
    return topicBw(topicName, messageType, window, false);
  }

  @Override
  public long topicBw(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicBw(id, topicName, messageType, window, wallTime));
    return id;
  }

  @Override
  public long topicDelay(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicDelay(id, topicName, messageType, Math.max(1, window)));
    return id;
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
    long id = nextRequestId();
    sendPacket(packetBuilder.serviceCall(
        id,
        serviceName,
        serviceType,
        payload,
        Math.max(0.0, timeoutSeconds),
        Math.max(0, repeatCount),
        Math.max(0.0, rateHz)));
    return id;
  }

  // ── Parameters ──────────────────────────────────────────────────────

  @Override
  public long paramList(
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramList(
        id,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0, depth),
        includeTypes,
        filterRegex == null ? "" : filterRegex,
        Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramGet(String nodeName, String paramName, boolean hideType, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    long id = nextRequestId();
    sendPacket(
        packetBuilder.paramGet(id, nodeName, paramName, hideType, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramSet(String nodeName, String paramName, String valueText, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    Objects.requireNonNull(valueText, "valueText must not be null");
    long id = nextRequestId();
    sendPacket(
        packetBuilder.paramSet(id, nodeName, paramName, valueText, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramDescribe(String nodeName, String paramName, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramDescribe(id, nodeName, paramName, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramDump(String nodeName, String[] prefixes, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramDump(
        id,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramLoad(
      String nodeName, String yamlText, double timeoutSeconds, boolean useWildcard) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(yamlText, "yamlText must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramLoad(
        id, nodeName, yamlText, Math.max(0.0, timeoutSeconds), useWildcard));
    return id;
  }

  // ── Actions ─────────────────────────────────────────────────────────

  @Override
  public long actionInfo(String actionName, boolean includeHidden) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.actionInfo(id, actionName, includeHidden));
    return id;
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
    long id = nextRequestId();
    sendPacket(packetBuilder.actionSendGoal(
        id, actionName, actionType, goalPayload, feedback, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long sendAddonEvent(
      String addonId, String eventType, String encoding, byte[] payload, boolean response) {
    Objects.requireNonNull(addonId, "addonId must not be null");
    Objects.requireNonNull(eventType, "eventType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.addonEvent(id, addonId, eventType, encoding, payload, response));
    return id;
  }
}
