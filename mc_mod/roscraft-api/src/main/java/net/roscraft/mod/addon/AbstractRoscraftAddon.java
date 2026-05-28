package net.roscraft.mod.addon;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.Subscription;

/**
 * Convenience base for addons that use typed response handlers and managed subscriptions.
 *
 * <p>Override {@link #configure()} to register handlers and subscriptions. Use
 * {@link #on(Class, Consumer)} for owner-scoped bridge responses (via
 * {@link #onBridgeEvent(BridgeEvent)}) and {@link #onGlobal(Class, Consumer)} for
 * cross-addon bridge events.
 */
public abstract class AbstractRoscraftAddon implements RoscraftAddon {

  protected AddonContext ctx;
  protected final SubscriptionBag subscriptions = new SubscriptionBag();

  private final List<TypedHandler<?>> typedHandlers = new ArrayList<>();

  @Override
  public final void init(AddonContext ctx) {
    this.ctx = ctx;
    configure();
  }

  /** Called once after {@code ctx} is set. Register handlers and subscriptions here. */
  protected void configure() {}

  /**
   * Handle a bridge response of the given type for requests this addon originated.
   *
   * <p>Composes with {@link #handleBridgeEvent(BridgeEvent)} — call {@code super} if you override both.
   */
  protected final <T extends BridgeEvent> void on(Class<T> type, Consumer<T> handler) {
    typedHandlers.add(new TypedHandler<>(type, handler));
  }

  /** Subscribe to globally broadcast bridge events of the given type. */
  protected final <T extends BridgeEvent> void onGlobal(Class<T> type, Consumer<T> handler) {
    subscriptions.add(ctx.bridgeBus().onAny(type, handler));
  }

  /** Subscribe to string-keyed inter-addon signals (same JVM, no ROS). */
  protected final Subscription onSignal(
      String signalType, Consumer<BridgeEvent.AddonEvent> handler) {
    Subscription sub = ctx.signalBus().on(signalType, handler);
    subscriptions.add(sub);
    return sub;
  }

  /**
   * Optional hook for events not handled by {@link #on(Class, Consumer)} registrations.
   * Default implementation is a no-op.
   */
  protected void handleBridgeEvent(BridgeEvent event) {}

  @Override
  public void onBridgeEvent(BridgeEvent event) {
    for (var handler : typedHandlers) {
      handler.dispatch(event);
    }
    handleBridgeEvent(event);
  }

  /** Called after subscriptions are closed. Default implementation is a no-op. */
  protected void onShutdown() {}

  @Override
  public final void shutdown() {
    subscriptions.close();
    onShutdown();
  }

  private record TypedHandler<T extends BridgeEvent>(Class<T> type, Consumer<T> handler) {
    void dispatch(BridgeEvent event) {
      if (type.isInstance(event)) {
        handler.accept(type.cast(event));
      }
    }
  }
}
