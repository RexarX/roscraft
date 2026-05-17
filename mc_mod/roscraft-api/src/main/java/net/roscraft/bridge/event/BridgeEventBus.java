package net.roscraft.bridge.event;

import java.util.function.Consumer;

/**
 * React to bridge events you did not originate — genuinely global broadcast.
 *
 * <p>Unlike {@code onBridgeEvent()} which receives only your own tracked response
 * events, subscriptions on this bus fire for every event of the given type from
 * any source. Use with care — a {@code TopicPayload} subscription fires for every
 * addon's topic subscriptions, not just your own.
 */
public interface BridgeEventBus {

  <T extends BridgeEvent> Subscription onAny(Class<T> type, Consumer<T> handler);
}
