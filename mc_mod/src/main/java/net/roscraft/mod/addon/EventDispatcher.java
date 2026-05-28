package net.roscraft.mod.addon;

import java.util.Map;
import java.util.Objects;
import net.roscraft.bridge.event.AddonSignalBus;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;

/**
 * Routes incoming bridge events to the correct addon and to typed subscribers.
 *
 * <p>Owns the {@link RequestRouter} for mapping request IDs to addon IDs.
 * Dispatches to the addon owner via {@link RoscraftAddon#onBridgeEvent(BridgeEvent)}
 * and to typed subscribers via {@link BridgeEventBusImpl}.
 */
final class EventDispatcher {

  private final RequestRouter requestRouter;
  private final BridgeEventBusImpl bridgeBus;
  private final AddonSignalBus signalBus;
  private final Map<String, RoscraftAddon> addons;

  EventDispatcher(
      Map<String, RoscraftAddon> addons, RequestRouter requestRouter, AddonSignalBus signalBus) {
    this.requestRouter = Objects.requireNonNull(requestRouter, "requestRouter must not be null");
    this.bridgeBus = new BridgeEventBusImpl();
    this.signalBus = Objects.requireNonNull(signalBus, "signalBus must not be null");
    this.addons = Objects.requireNonNull(addons, "addons must not be null");
  }

  RequestRouter requestRouter() {
    return requestRouter;
  }

  BridgeEventBusImpl bridgeBus() {
    return bridgeBus;
  }

  void clear() {
    requestRouter.clear();
    bridgeBus.clear();
  }

  /** Called by the bridge callback for every incoming bridge event. */
  void onEvent(BridgeEvent event) {
    bridgeBus.dispatch(event);
    if (event instanceof BridgeEvent.AddonEvent ae) {
      signalBus.emit(ae);
    }
    dispatchToAddonOwner(event);
  }

  private void dispatchToAddonOwner(BridgeEvent event) {
    String routedAddonId = requestRouter.consume(event.requestId());

    RoscraftAddon addon;
    if (event instanceof BridgeEvent.AddonEvent ae) {
      String target = routedAddonId != null ? routedAddonId : ae.addonId();
      addon = addons.get(target);
    } else {
      addon = routedAddonId != null ? addons.get(routedAddonId) : null;
    }

    if (addon != null) {
      try {
        addon.onBridgeEvent(event);
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn(
            "Error dispatching event to addon '{}': {}", addon.addonId(), ex.getMessage());
      }
    }
  }
}
