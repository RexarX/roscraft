package net.roscraft.mod.addon;

import java.util.Objects;
import java.util.Optional;
import java.util.function.Supplier;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.event.AddonSignalBus;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.BridgeEventBus;
import net.roscraft.bridge.event.LocalBus;
import net.roscraft.bridge.event.Subscription;

/**
 * Context passed to {@link RoscraftAddon#init(AddonContext)} providing
 * access to the bridge, event buses, event sending, and request tracking.
 *
 * <h3>Bridge access</h3>
 * Use {@link #bridgeIfConnected()} to get an {@code Optional<BridgeOperations>}.
 * Every bridge operation invoked through the returned wrapper is automatically
 * tracked — responses route to {@link RoscraftAddon#onBridgeEvent(BridgeEvent)}
 * without any manual tracking step.
 *
 * <h3>Inter-addon events</h3>
 * {@link #sendEvent(String, byte[], boolean)} sends events to ROS via the bridge.
 * When disconnected, it returns {@link #DISCONNECTED} silently.
 */
public final class AddonContext {

  /** Returned by {@link #sendEvent} when the bridge is disconnected. Never a real request ID. */
  public static final long DISCONNECTED = 0L;

  private final String addonId;
  private final Supplier<BridgeOperations> bridgeSupplier;
  private final RequestTracker requestTracker;
  private final BridgeEventBus bridgeBus;
  private final LocalBus localBus;
  private final AddonSignalBus signalBus;
  private final SendEventFunction sendEventFn;

  public AddonContext(
      String addonId,
      Supplier<BridgeOperations> bridgeSupplier,
      RequestTracker requestTracker,
      BridgeEventBus bridgeBus,
      LocalBus localBus,
      AddonSignalBus signalBus,
      SendEventFunction sendEventFn) {
    this.addonId = Objects.requireNonNull(addonId, "addonId must not be null");
    this.bridgeSupplier = Objects.requireNonNull(bridgeSupplier, "bridgeSupplier must not be null");
    this.requestTracker = Objects.requireNonNull(requestTracker, "requestTracker must not be null");
    this.bridgeBus = Objects.requireNonNull(bridgeBus, "bridgeBus must not be null");
    this.localBus = Objects.requireNonNull(localBus, "localBus must not be null");
    this.signalBus = Objects.requireNonNull(signalBus, "signalBus must not be null");
    this.sendEventFn = Objects.requireNonNull(sendEventFn, "sendEventFn must not be null");
  }

  public String addonId() {
    return addonId;
  }

  public boolean isBridgeConnected() {
    return bridgeSupplier.get() != null;
  }

  /**
   * The active ROS bridge with auto-tracking, or {@code Optional.empty()} if not connected.
   *
   * <p>Every bridge operation invoked through the returned wrapper is automatically
   * tracked. One-shot operations (queries, params, etc.) use {@code track()};
   * persistent operations (subscribe, sendGoal) use {@code trackPersistent()}.
   * This is a live lookup — it reflects the current connection state.
   */
  public Optional<BridgeOperations> bridgeIfConnected() {
    return Optional.ofNullable(bridgeSupplier.get());
  }

  /** Subscribe to globally-broadcast bridge events (all addons' responses, not just your own). */
  public BridgeEventBus bridgeBus() {
    return bridgeBus;
  }

  /** Emit/receive typed local messages between addons that share a classpath. */
  public LocalBus localBus() {
    return localBus;
  }

  /** Emit/receive string-keyed inter-addon signals (loosely coupled). */
  public AddonSignalBus signalBus() {
    return signalBus;
  }

  // ── Convenience: tracked subscriptions ────────────────────────────────

  /**
   * Subscribe to a ROS topic with auto-tracking.
   *
   * @return a handle that unsubscribes and untracks when closed
   */
  public Optional<Subscription> subscribeTopic(String topic, String type) {
    return subscribeTopic(topic, type, BridgeOperations.TopicOps.SubscribeOptions.defaults());
  }

  /**
   * Subscribe to a ROS topic with auto-tracking and options.
   *
   * @return a handle that unsubscribes and untracks when closed
   */
  public Optional<Subscription> subscribeTopic(
      String topic, String type, BridgeOperations.TopicOps.SubscribeOptions opts) {
    BridgeOperations bridge = bridgeSupplier.get();
    if (bridge == null) return Optional.empty();
    long id = bridge.topics().subscribe(topic, type, opts);
    requestTracker.trackPersistent(addonId, id);
    return Optional.of(() -> {
      requestTracker.untrack(addonId, id);
      bridge.topics().unsubscribe(topic);
    });
  }

  /**
   * Send an action goal with auto-tracking.
   *
   * @return a handle that untracks when closed
   */
  public Optional<Subscription> sendGoal(
      String name, String type, byte[] goalPayload,
      BridgeOperations.ActionOps.ActionGoalOptions opts) {
    BridgeOperations bridge = bridgeSupplier.get();
    if (bridge == null) return Optional.empty();
    long id = bridge.actions().sendGoal(name, type, goalPayload, opts);
    requestTracker.trackPersistent(addonId, id);
    return Optional.of(() -> requestTracker.untrack(addonId, id));
  }

  // ── Inter-addon events ────────────────────────────────────────────────

  /**
   * Send an addon event to ROS via the bridge.
   *
   * @param eventType event type identifier
   * @param payload serialized payload bytes (may be empty)
   * @param response whether this is a response to a prior incoming event
   * @return request ID for correlation, or {@link #DISCONNECTED} if bridge is disconnected
   */
  public long sendEvent(String eventType, byte[] payload, boolean response) {
    return sendEventFn.send(addonId, eventType, "", payload, response);
  }

  // ── Types ─────────────────────────────────────────────────────────────

  /** Functional interface for sending addon events to ROS. Implemented by the bridge layer. */
  @FunctionalInterface
  public interface SendEventFunction {
    long send(String addonId, String eventType, String encoding, byte[] payload, boolean response);
  }

  public interface RequestTracker {
    void track(String addonId, long requestId);

    void trackPersistent(String addonId, long requestId);

    void untrack(String addonId, long requestId);
  }
}
