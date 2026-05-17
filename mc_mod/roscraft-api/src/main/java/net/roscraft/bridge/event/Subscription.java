package net.roscraft.bridge.event;

/** A handle that can be closed to unsubscribe. */
public interface Subscription extends AutoCloseable {
  @Override
  void close();
}
