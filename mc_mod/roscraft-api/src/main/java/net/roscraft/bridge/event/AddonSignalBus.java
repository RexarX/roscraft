package net.roscraft.bridge.event;

import java.util.function.Consumer;

/**
 * Loosely-coupled string-keyed signals for addons that don't share types.
 *
 * <p>Addons that share a classpath and want type safety should use
 * {@link LocalBus} instead. To send events to ROS (across the bridge),
 * use {@link net.roscraft.mod.addon.AddonContext#sendEvent(String, byte[], boolean)}.
 */
public interface AddonSignalBus {

  Subscription on(String signalType, Consumer<BridgeEvent.AddonEvent> handler);

  void emit(BridgeEvent.AddonEvent event);
}
