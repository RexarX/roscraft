package net.roscraft.bridge.event;

import java.util.function.Consumer;

/**
 * @deprecated Replaced by three purpose-specific buses:
 * <ul>
 * <li>{@link BridgeEventBus} — subscribe to globally-broadcast bridge events</li>
 * <li>{@link LocalBus} — emit/receive typed local messages</li>
 * <li>{@link AddonSignalBus} — emit/receive string-keyed inter-addon signals</li>
 * </ul>
 * Obtain them via {@code AddonContext.bridgeBus()}, {@code .localBus()}, {@code .signalBus()}.
 */
@Deprecated(forRemoval = true)
public interface EventBus extends BridgeEventBus, LocalBus, AddonSignalBus {

  @Deprecated
  <T extends BridgeEvent> Subscription subscribe(Class<T> type, Consumer<T> handler);

  @Deprecated
  Subscription subscribeAddonEvent(String eventType, Consumer<BridgeEvent.AddonEvent> handler);

  @Deprecated
  void publish(BridgeEvent.AddonEvent event);

  @Deprecated
  <T> Subscription subscribeLocal(Class<T> type, Consumer<T> handler);

  @Deprecated
  <T> void publishLocal(T event);
}
