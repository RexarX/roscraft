package net.roscraft.mod.addon;

import java.util.Objects;
import java.util.Optional;
import java.util.function.Supplier;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.event.EventBus;

/**
 * Context passed to {@link RoscraftAddon#init(AddonContext)} providing
 * access to the bridge, event bus, event sending, and request tracking.
 *
 * <h3>Bridge access</h3>
 * Use {@link #bridgeIfConnected()} to get an {@code Optional<BridgeOperations>},
 * or {@link #isBridgeConnected()} for a quick check. When disconnected, bridge
 * calls return nothing — addons don't need null checks.
 *
 * <h3>Request tracking</h3>
 * Any bridge method that returns a request ID can be tracked so the
 * response is routed to the addon's {@link RoscraftAddon#onBridgeEvent}:
 * <pre>{@code
 * ctx.bridgeIfConnected().ifPresent(bridge -> {
 *     ctx.track(bridge.queryGraph());
 *     ctx.track(bridge.subscribeTopic("/foo", "std_msgs/msg/String"));
 * });
 * }</pre>
 * {@link #track(long)} returns the request ID for chaining:
 * <pre>{@code
 * long rid = ctx.bridgeIfConnected()
 *     .map(bridge -> ctx.track(bridge.queryGraph()))
 *     .orElse(0L);
 * }</pre>
 *
 * <h3>Inter-addon events</h3>
 * {@link #sendEvent(String, byte[], boolean)} sends events to ROS via
 * the bridge. When disconnected, it returns 0 silently.
 */
public final class AddonContext {

  private final String addonId;
  private final Supplier<BridgeOperations> bridgeSupplier;
  private final RequestTracker requestTracker;
  private final EventBus eventBus;

  public AddonContext(
      String addonId,
      Supplier<BridgeOperations> bridgeSupplier,
      RequestTracker requestTracker,
      EventBus eventBus) {
    this.addonId = Objects.requireNonNull(addonId, "addonId must not be null");
    this.bridgeSupplier = Objects.requireNonNull(bridgeSupplier, "bridgeSupplier must not be null");
    this.requestTracker = Objects.requireNonNull(requestTracker, "requestTracker must not be null");
    this.eventBus = Objects.requireNonNull(eventBus, "eventBus must not be null");
  }

  public String addonId() {
    return addonId;
  }

  public boolean isBridgeConnected() {
    return bridgeSupplier.get() != null;
  }

  /**
   * The active ROS bridge, or {@code Optional.empty()} if not connected.
   * This is a live lookup — it reflects the current connection state.
   */
  public Optional<BridgeOperations> bridgeIfConnected() {
    return Optional.ofNullable(bridgeSupplier.get());
  }

  /** Local event bus for addon-to-addon communication. */
  public EventBus eventBus() {
    return eventBus;
  }

  /**
   * Track a request so its response is routed to this addon's
   * {@link RoscraftAddon#onBridgeEvent}. Returns the request ID for chaining.
   */
  public long track(long requestId) {
    requestTracker.track(addonId, requestId);
    return requestId;
  }

  public void untrackRequest(long requestId) {
    requestTracker.untrack(addonId, requestId);
  }

  /**
   * Send an addon event to ROS via the bridge.
   *
   * @param eventType event type identifier
   * @param payload serialized payload bytes (may be empty)
   * @param response whether this is a response to a prior incoming event
   * @return request ID for correlation, or 0 if bridge is disconnected
   */
  public long sendEvent(String eventType, byte[] payload, boolean response) {
    return sendEvent(eventType, "", payload, response);
  }

  /**
   * Send an addon event to ROS with encoding.
   *
   * @param eventType event type identifier
   * @param encoding payload encoding (e.g. "json")
   * @param payload serialized payload bytes
   * @param response whether this is a response
   * @return request ID for correlation, or 0 if bridge is disconnected
   */
  public long sendEvent(String eventType, String encoding, byte[] payload, boolean response) {
    BridgeOperations bridge = bridgeSupplier.get();
    if (bridge == null) {
      return 0;
    }
    return bridge.sendAddonEvent(addonId, eventType, encoding, payload, response);
  }

  public interface RequestTracker {
    void track(String addonId, long requestId);

    void untrack(String addonId, long requestId);
  }
}