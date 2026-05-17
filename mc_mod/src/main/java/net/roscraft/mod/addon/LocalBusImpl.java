package net.roscraft.mod.addon;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import net.roscraft.bridge.event.LocalBus;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;

final class LocalBusImpl implements LocalBus {

  private final ConcurrentHashMap<Class<?>, List<Consumer<?>>> subscribers =
      new ConcurrentHashMap<>();

  @Override
  @SuppressWarnings("unchecked")
  public <T> Subscription on(Class<T> type, Consumer<T> handler) {
    Objects.requireNonNull(type, "type must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    subscribers.computeIfAbsent(type, k -> new CopyOnWriteArrayList<>()).add(handler);
    return () -> {
      var subs = subscribers.get(type);
      if (subs != null) subs.remove(handler);
    };
  }

  @Override
  @SuppressWarnings("unchecked")
  public <T> void emit(T message) {
    Objects.requireNonNull(message, "message must not be null");
    var subs = subscribers.get(message.getClass());
    if (subs != null) {
      for (var handler : subs) {
        try {
          ((Consumer<T>) handler).accept(message);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in local event subscriber for {}: {}",
              message.getClass().getSimpleName(),
              ex.getMessage());
        }
      }
    }
  }

  void clear() {
    subscribers.clear();
  }
}
