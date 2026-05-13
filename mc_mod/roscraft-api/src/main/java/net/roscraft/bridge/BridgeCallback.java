package net.roscraft.bridge;

import net.roscraft.bridge.event.BridgeEvent;

/**
 * Callback interface implemented by the mod to receive bridge events.
 *
 * <p>{@link #onEvent(BridgeEvent)} is invoked on the thread that calls
 * {@link RoscraftBridge#tick}, so implementations must not block for
 * extended periods. Heavy work should be dispatched to the mod's own
 * thread pool or Minecraft's server thread.
 */
public interface BridgeCallback {
    /**
     * Called when any bridge event arrives.
     *
     * <p>Implementations should pattern-match on the sealed
     * {@link BridgeEvent} hierarchy to handle specific event types.
     *
     * @param event the deserialized bridge event; never {@code null}
     */
    default void onEvent(BridgeEvent event) {}
}
