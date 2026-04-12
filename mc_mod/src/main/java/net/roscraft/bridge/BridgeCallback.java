package net.roscraft.bridge;

import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.TopicPayload;

/**
 * Callback interface implemented by the mod to receive outgoing ROS2 commands.
 *
 * <p>All methods are invoked on the thread that calls {@link RoscraftBridge#tick},
 * so implementations must not block for extended periods. Heavy work should be
 * dispatched to the mod's own thread pool or Minecraft's server thread.
 *
 * <p>Default no-op implementations are provided for every method so that
 * implementors only override the events they care about.
 */
public interface BridgeCallback {
    /**
     * Called when a full ROS2 graph snapshot arrives.
     *
     * @param snapshot Immutable snapshot of topics, services and actions.
     */
    default void onGraphSnapshot(GraphSnapshot snapshot) {}

    /**
     * Called when a raw CDR message is forwarded from a subscribed topic.
     *
     * @param payload Topic name, message type and raw CDR bytes.
     */
    default void onTopicPayload(TopicPayload payload) {}

    /**
     * Called when the ROS2 layer responds to a player-list query.
     *
     * @param playerList Request ID and list of current players.
     */
    default void onPlayerList(PlayerList playerList) {}
}
