package net.roscraft.bridge.event;

import java.util.function.Consumer;

/**
 * Type-safe local messages between addons that share a classpath.
 *
 * <p>No byte[] serialization — full compile-time type safety. For loosely-coupled
 * addons that don't share types, use {@link AddonSignalBus} instead.
 */
public interface LocalBus {

  <T> Subscription on(Class<T> type, Consumer<T> handler);

  <T> void emit(T message);
}
