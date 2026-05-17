package net.roscraft.mod.addon;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.BridgeEventBus;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;

final class BridgeEventBusImpl implements BridgeEventBus {

  private final ConcurrentHashMap<Class<?>, List<Consumer<?>>> subscribers =
      new ConcurrentHashMap<>();

  @Override
  @SuppressWarnings("unchecked")
  public <T extends BridgeEvent> Subscription onAny(Class<T> type, Consumer<T> handler) {
    Objects.requireNonNull(type, "type must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    if (type == BridgeEvent.class) {
      throw new IllegalArgumentException(
          "Subscribe to a concrete BridgeEvent record type, not BridgeEvent itself. "
              + "Use onBridgeEvent() if you need all events.");
    }
    subscribers.computeIfAbsent(type, k -> new CopyOnWriteArrayList<>()).add(handler);
    return () -> {
      var subs = subscribers.get(type);
      if (subs != null) subs.remove(handler);
    };
  }

  @SuppressWarnings("unchecked")
  <T extends BridgeEvent> void dispatch(T event) {
    var subs = subscribers.get(event.getClass());
    if (subs != null) {
      for (var handler : subs) {
        try {
          ((Consumer<T>) handler).accept(event);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in typed event subscriber for {}: {}",
              event.getClass().getSimpleName(),
              ex.getMessage());
        }
      }
    }
  }

  void clear() {
    subscribers.clear();
  }
}
