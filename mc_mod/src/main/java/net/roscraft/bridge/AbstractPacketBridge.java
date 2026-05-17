package net.roscraft.bridge;

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Shared base class for bridge transport implementations.
 *
 * <p>Implements every {@link RoscraftBridge} operation method exactly
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

  // ── BridgeOperations ────────────────────────────────────────────────

  @Override
  public long queryPlayers() {
    long id = nextRequestId();
    sendPacket(packetBuilder.queryPlayers(id));
    return id;
  }

  @Override
  public long sendRawPacket(byte[] flatbufferPayload) {
    long id = nextRequestId();
    // The raw packet already has a request ID embedded — we need to use it.
    // For now, just send the raw bytes directly.
    sendPacket(ByteBuffer.wrap(flatbufferPayload));
    return id;
  }

  // ── Graph operations ─────────────────────────────────────────────────

  @Override
  public long snapshot() {
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

  // ── Topic operations ─────────────────────────────────────────────────

  @Override
  public long subscribe(String topicName, String messageType) {
    return subscribe(topicName, messageType, TopicOps.SubscribeOptions.defaults());
  }

  @Override
  public long subscribe(String topicName, String messageType, TopicOps.SubscribeOptions opts) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicSubscribe(
        id, topicName, messageType, opts.once(), opts.timeoutSeconds(), opts.raw()));
    return id;
  }

  @Override
  public long unsubscribe(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicUnsubscribe(id, topicName));
    return id;
  }

  @Override
  public long publish(String topicName, String messageType, byte[] payload) {
    return publish(topicName, messageType, payload, TopicOps.PublishOptions.defaults());
  }

  @Override
  public long publish(
      String topicName, String messageType, byte[] payload, TopicOps.PublishOptions opts) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicPublishMessage(
        id,
        topicName,
        messageType,
        payload,
        opts.once(),
        opts.rateHz(),
        opts.times(),
        Objects.requireNonNull(opts.qosProfile(), "qosProfile must not be null")));
    return id;
  }

  @Override
  public long hz(String topicName, String messageType, int window) {
    return hz(topicName, messageType, window, TopicOps.HzOptions.defaults());
  }

  @Override
  public long hz(String topicName, String messageType, int window, TopicOps.HzOptions opts) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicHz(id, topicName, messageType, window, opts.wallTime()));
    return id;
  }

  @Override
  public long bw(String topicName, String messageType, int window) {
    return bw(topicName, messageType, window, TopicOps.BwOptions.defaults());
  }

  @Override
  public long bw(String topicName, String messageType, int window, TopicOps.BwOptions opts) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicBw(id, topicName, messageType, window, opts.wallTime()));
    return id;
  }

  @Override
  public long delay(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.topicDelay(id, topicName, messageType, window));
    return id;
  }

  // ── Service operations ───────────────────────────────────────────────

  @Override
  public long call(
      String serviceName, String serviceType, byte[] payload, ServiceOps.ServiceCallOptions opts) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    Objects.requireNonNull(serviceType, "serviceType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.serviceCall(
        id,
        serviceName,
        serviceType,
        payload,
        Math.max(0.0, opts.timeoutSeconds()),
        Math.max(0, opts.repeatCount()),
        Math.max(0.0, opts.rateHz())));
    return id;
  }

  // ── Param operations ─────────────────────────────────────────────────

  @Override
  public long list(String nodeName, ParamOps.ParamListOptions opts) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramList(
        id,
        nodeName,
        opts.prefixes() == null
            ? new String[0]
            : Arrays.copyOf(opts.prefixes(), opts.prefixes().length),
        Math.max(0, opts.depth()),
        opts.includeTypes(),
        opts.filterRegex() == null ? "" : opts.filterRegex(),
        Math.max(0.0, opts.timeoutSeconds())));
    return id;
  }

  @Override
  public long get(String nodeName, String paramName, ParamOps.ParamGetOptions opts) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramGet(
        id, nodeName, paramName, opts.hideType(), Math.max(0.0, opts.timeoutSeconds())));
    return id;
  }

  @Override
  public long set(String nodeName, String paramName, String valueText, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    Objects.requireNonNull(valueText, "valueText must not be null");
    long id = nextRequestId();
    sendPacket(
        packetBuilder.paramSet(id, nodeName, paramName, valueText, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long describe(String nodeName, String paramName, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramDescribe(id, nodeName, paramName, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long dump(String nodeName, String[] prefixes, double timeoutSeconds) {
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
  public long load(String nodeName, String yamlText, ParamOps.ParamLoadOptions opts) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(yamlText, "yamlText must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.paramLoad(
        id, nodeName, yamlText, Math.max(0.0, opts.timeoutSeconds()), opts.useWildcard()));
    return id;
  }

  // ── Action operations ────────────────────────────────────────────────

  @Override
  public long info(String actionName, boolean includeHidden) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.actionInfo(id, actionName, includeHidden));
    return id;
  }

  @Override
  public long sendGoal(
      String actionName, String actionType, byte[] goalPayload, ActionOps.ActionGoalOptions opts) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    Objects.requireNonNull(actionType, "actionType must not be null");
    Objects.requireNonNull(goalPayload, "goalPayload must not be null");
    long id = nextRequestId();
    sendPacket(packetBuilder.actionSendGoal(
        id,
        actionName,
        actionType,
        goalPayload,
        opts.feedback(),
        Math.max(0.0, opts.timeoutSeconds())));
    return id;
  }

  // ── Addon events (internal) ──────────────────────────────────────────

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

  @Override
  protected byte[] buildAddonEventPacket(
      String addonId, String eventType, String encoding, byte[] payload, boolean response) {
    long id = nextRequestId();
    ByteBuffer buf = packetBuilder.addonEvent(id, addonId, eventType, encoding, payload, response);
    byte[] bytes = new byte[buf.remaining()];
    buf.get(bytes);
    return bytes;
  }
}
