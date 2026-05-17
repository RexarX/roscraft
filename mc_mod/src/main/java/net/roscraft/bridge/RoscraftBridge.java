package net.roscraft.bridge;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Abstract base class for all RoscraftBridge implementations.
 *
 * <p>Subclasses provide the transport mechanism (JNI or network). This
 * class manages the shared callback reference and request-ID generation.
 * Addons interact through the narrowed {@link BridgeOperations} interface.
 *
 * <p><b>Lifecycle:</b>
 * <ol>
 * <li>Construct the concrete subclass.</li>
 * <li>Call {@link #registerCallback} to install an event listener.</li>
 * <li>Call {@link #tick} from the game loop (server tick, etc.).</li>
 * <li>Call {@link #close} during mod shutdown.</li>
 * </ol>
 */
public abstract class RoscraftBridge
    implements BridgeOperations,
        BridgeOperations.TopicOps,
        BridgeOperations.ParamOps,
        BridgeOperations.ServiceOps,
        BridgeOperations.ActionOps,
        BridgeOperations.GraphOps,
        AutoCloseable {

  private static final AtomicLong REQUEST_COUNTER = new AtomicLong(1L);

  protected static long nextRequestId() {
    return REQUEST_COUNTER.getAndIncrement();
  }

  // ── Callback ────────────────────────────────────────────────────────

  private final AtomicReference<BridgeCallback> callback =
      new AtomicReference<>(new BridgeCallback() {});

  public final void registerCallback(BridgeCallback callback) {
    this.callback.set(Objects.requireNonNull(callback, "callback must not be null"));
  }

  protected final BridgeCallback callback() {
    return callback.get();
  }

  // ── BridgeOperations accessors ──────────────────────────────────────

  @Override
  public TopicOps topics() {
    return this;
  }

  @Override
  public ParamOps params() {
    return this;
  }

  @Override
  public ServiceOps services() {
    return this;
  }

  @Override
  public ActionOps actions() {
    return this;
  }

  @Override
  public GraphOps graph() {
    return this;
  }

  // ── Abstract transport API ──────────────────────────────────────────

  public abstract void tick();

  @Override
  public abstract void close();

  // ── BridgeOperations ────────────────────────────────────────────────

  @Override
  public abstract long queryPlayers();

  @Override
  public abstract long sendRawPacket(byte[] flatbufferPayload);

  // ── Graph operations ─────────────────────────────────────────────────

  @Override
  public abstract long snapshot();

  @Override
  public abstract long nodeInfo(String nodeName, boolean includeHidden);

  @Override
  public abstract long topicInfo(String topicName);

  @Override
  public abstract long serviceInfo(String serviceName);

  @Override
  public abstract long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions);

  @Override
  public abstract long interfaceShow(String interfaceType);

  // ── Topic operations ─────────────────────────────────────────────────

  @Override
  public abstract long subscribe(String topicName, String messageType);

  @Override
  public abstract long subscribe(
      String topicName, String messageType, TopicOps.SubscribeOptions opts);

  @Override
  public abstract long unsubscribe(String topicName);

  @Override
  public abstract long publish(String topicName, String messageType, byte[] payload);

  @Override
  public abstract long publish(
      String topicName, String messageType, byte[] payload, TopicOps.PublishOptions opts);

  @Override
  public abstract long hz(String topicName, String messageType, int window);

  @Override
  public abstract long hz(
      String topicName, String messageType, int window, TopicOps.HzOptions opts);

  @Override
  public abstract long bw(String topicName, String messageType, int window);

  @Override
  public abstract long bw(
      String topicName, String messageType, int window, TopicOps.BwOptions opts);

  @Override
  public abstract long delay(String topicName, String messageType, int window);

  // ── Service operations ───────────────────────────────────────────────

  @Override
  public abstract long call(
      String serviceName, String serviceType, byte[] payload, ServiceOps.ServiceCallOptions opts);

  // ── Param operations ─────────────────────────────────────────────────

  @Override
  public abstract long list(String nodeName, ParamOps.ParamListOptions opts);

  @Override
  public abstract long get(String nodeName, String paramName, ParamOps.ParamGetOptions opts);

  @Override
  public abstract long set(
      String nodeName, String paramName, String valueText, double timeoutSeconds);

  @Override
  public abstract long describe(String nodeName, String paramName, double timeoutSeconds);

  @Override
  public abstract long dump(String nodeName, String[] prefixes, double timeoutSeconds);

  @Override
  public abstract long load(String nodeName, String yamlText, ParamOps.ParamLoadOptions opts);

  // ── Action operations ────────────────────────────────────────────────

  @Override
  public abstract long info(String actionName, boolean includeHidden);

  @Override
  public abstract long sendGoal(
      String actionName, String actionType, byte[] goalPayload, ActionOps.ActionGoalOptions opts);

  // ── Addon event (internal — only AddonManager calls this) ────────────

  public long sendAddonEvent(
      String addonId, String eventType, String encoding, byte[] payload, boolean response) {
    // Default: serialize and sendRawPacket. Subclasses override for efficiency.
    byte[] packet = buildAddonEventPacket(addonId, eventType, encoding, payload, response);
    return sendRawPacket(packet);
  }

  protected byte[] buildAddonEventPacket(
      String addonId, String eventType, String encoding, byte[] payload, boolean response) {
    throw new UnsupportedOperationException(
        "buildAddonEventPacket must be overridden or sendAddonEvent must be overridden");
  }
}
