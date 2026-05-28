package net.roscraft.mod.addon;

import java.util.ArrayList;
import java.util.List;
import net.roscraft.bridge.event.Subscription;

/**
 * Collects {@link Subscription} handles and closes them together on shutdown.
 *
 * <p>Typical usage in {@link AbstractRoscraftAddon#configure()}:
 * <pre>{@code
 * subscriptions.add(ctx.subscribeTopic("/cmd_vel", "geometry_msgs/msg/Twist"));
 * subscriptions.add(ctx.signalBus().on("reload", this::onReload));
 * }</pre>
 */
public final class SubscriptionBag implements AutoCloseable {

  private final List<Subscription> subscriptions = new ArrayList<>();
  private boolean closed;

  public void add(Subscription subscription) {
    if (subscription == null) {
      return;
    }
    if (closed) {
      subscription.close();
      return;
    }
    subscriptions.add(subscription);
  }

  public void add(java.util.Optional<Subscription> subscription) {
    subscription.ifPresent(this::add);
  }

  @Override
  public void close() {
    if (closed) {
      return;
    }
    closed = true;
    for (int i = subscriptions.size() - 1; i >= 0; i--) {
      try {
        subscriptions.get(i).close();
      } catch (Exception ignored) {
        // Best-effort cleanup
      }
    }
    subscriptions.clear();
  }
}
