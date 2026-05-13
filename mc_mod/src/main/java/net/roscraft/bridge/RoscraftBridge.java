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
public abstract class RoscraftBridge implements BridgeOperations, AutoCloseable {

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

  // ── Abstract transport API ──────────────────────────────────────────

  public abstract void tick();

  @Override
  public abstract void close();

  // ── Core ROS operations (abstract) ──────────────────────────────────

  @Override
  public abstract long queryGraph();

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

  @Override
  public abstract long subscribeTopic(String topicName, String messageType);

  @Override
  public abstract long subscribeTopic(
      String topicName, String messageType, boolean once, double timeoutSeconds, boolean raw);

  @Override
  public abstract long unsubscribeTopic(String topicName);

  @Override
  public abstract long publishMessage(String topicName, String messageType, byte[] payload);

  @Override
  public abstract long publishMessage(
      String topicName,
      String messageType,
      byte[] payload,
      boolean once,
      double rateHz,
      int times,
      String qosProfile);

  @Override
  public abstract long topicHz(String topicName, String messageType, int window);

  @Override
  public abstract long topicHz(String topicName, String messageType, int window, boolean wallTime);

  @Override
  public abstract long topicBw(String topicName, String messageType, int window);

  @Override
  public abstract long topicBw(String topicName, String messageType, int window, boolean wallTime);

  @Override
  public abstract long topicDelay(String topicName, String messageType, int window);

  @Override
  public abstract long serviceCall(
      String serviceName,
      String serviceType,
      byte[] payload,
      double timeoutSeconds,
      int repeatCount,
      double rateHz);

  @Override
  public abstract long paramList(
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds);

  @Override
  public abstract long paramGet(
      String nodeName, String paramName, boolean hideType, double timeoutSeconds);

  @Override
  public abstract long paramSet(
      String nodeName, String paramName, String valueText, double timeoutSeconds);

  @Override
  public abstract long paramDescribe(String nodeName, String paramName, double timeoutSeconds);

  @Override
  public abstract long paramDump(String nodeName, String[] prefixes, double timeoutSeconds);

  @Override
  public abstract long paramLoad(
      String nodeName, String yamlText, double timeoutSeconds, boolean useWildcard);

  @Override
  public abstract long actionInfo(String actionName, boolean includeHidden);

  @Override
  public abstract long actionSendGoal(
      String actionName,
      String actionType,
      byte[] goalPayload,
      boolean feedback,
      double timeoutSeconds);

  @Override
  public abstract long queryPlayers();

  @Override
  public abstract long sendAddonEvent(
      String addonId, String eventType, String encoding, byte[] payload, boolean response);
}
