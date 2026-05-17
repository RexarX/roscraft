package net.roscraft.mod.addon;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import net.roscraft.bridge.event.AddonSignalBus;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;

final class AddonSignalBusImpl implements AddonSignalBus {

  private final ConcurrentHashMap<String, List<Consumer<BridgeEvent.AddonEvent>>> subscribers =
      new ConcurrentHashMap<>();

  @Override
  public Subscription on(String signalType, Consumer<BridgeEvent.AddonEvent> handler) {
    Objects.requireNonNull(signalType, "signalType must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    subscribers.computeIfAbsent(signalType, k -> new CopyOnWriteArrayList<>()).add(handler);
    return () -> {
      var subs = subscribers.get(signalType);
      if (subs != null) subs.remove(handler);
    };
  }

  @Override
  public void emit(BridgeEvent.AddonEvent event) {
    Objects.requireNonNull(event, "event must not be null");
    var subs = subscribers.get(event.eventType());
    if (subs != null) {
      for (var handler : subs) {
        try {
          handler.accept(event);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in addon event subscriber for '{}': {}", event.eventType(), ex.getMessage());
        }
      }
    }
  }

  void clear() {
    subscribers.clear();
  }
}
