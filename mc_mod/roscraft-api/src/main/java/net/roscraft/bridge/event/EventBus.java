package net.roscraft.bridge.event;

import java.util.function.Consumer;

/**
 * Local event bus for addon-to-addon communication within the Minecraft process.
 *
 * <p>Addons subscribe to bridge events by type for ROS responses, or to
 * addon events by event type string for inter-addon coordination. Events
 * published via {@link #publish(BridgeEvent.AddonEvent)} stay local and do
 * not transit the ROS bridge — use an addon's
 * {@link net.roscraft.mod.addon.AddonContext#sendEvent(String, byte[], boolean)}
 * to send events to ROS.
 *
 * <h3>Typed local messages</h3>
 * Addons that share a classpath (same JAR, known dependency) can use
 * {@link #subscribeLocal(Class, Consumer)} and {@link #publishLocal(Object)}
 * for fully type-safe inter-addon communication without byte[] serialization.
 * For loosely-coupled addons that don't share types, use
 * {@link #subscribeAddonEvent(String, Consumer)} and {@link #publish(BridgeEvent.AddonEvent)}.
 */
public interface EventBus {

  /**
   * Subscribe to bridge events of a specific type.
   *
   * @param type the event record class to filter on
   * @param handler receives matching events
   * @param <T> the event subtype
   * @return a subscription that can be closed to unsubscribe
   */
  <T extends BridgeEvent> Subscription subscribe(Class<T> type, Consumer<T> handler);

  /**
   * Subscribe to addon events by event type string (local delivery, no ROS).
   *
   * @param eventType the event type string to match
   * @param handler receives matching addon events
   * @return a subscription that can be closed to unsubscribe
   */
  Subscription subscribeAddonEvent(String eventType, Consumer<BridgeEvent.AddonEvent> handler);

  /**
   * Publish an addon event locally. The event is delivered to all addons
   * subscribed to {@code event.eventType()} via {@link #subscribeAddonEvent}.
   * Does not transit the ROS bridge.
   *
   * @param event the addon event to publish
   */
  void publish(BridgeEvent.AddonEvent event);

  /**
   * Subscribe to typed local messages. For addons that share the same
   * classpath — no byte[] serialization, full compile-time type safety.
   *
   * @param type the message class to filter on
   * @param handler receives matching messages
   * @param <T> the message type
   * @return a subscription that can be closed to unsubscribe
   */
  <T> Subscription subscribeLocal(Class<T> type, Consumer<T> handler);

  /**
   * Publish a typed local message. Delivered to all addons subscribed
   * to the message's runtime class via {@link #subscribeLocal(Class, Consumer)}.
   *
   * @param event the message to publish; must not be null
   * @param <T> the message type
   */
  <T> void publishLocal(T event);

  /** A handle that can be closed to unsubscribe. */
  interface Subscription extends AutoCloseable {
    @Override
    void close();
  }
}
