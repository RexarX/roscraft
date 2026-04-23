package net.roscraft.bridge;

import net.roscraft.bridge.data.ActionFeedback;
import net.roscraft.bridge.data.ActionInfoResponse;
import net.roscraft.bridge.data.ActionResult;
import net.roscraft.bridge.data.BridgeError;
import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.InterfaceListResponse;
import net.roscraft.bridge.data.InterfaceShowResponse;
import net.roscraft.bridge.data.NodeInfoResponse;
import net.roscraft.bridge.data.ParamDescribeResponse;
import net.roscraft.bridge.data.ParamDumpResponse;
import net.roscraft.bridge.data.ParamGetResponse;
import net.roscraft.bridge.data.ParamListResponse;
import net.roscraft.bridge.data.ParamLoadResponse;
import net.roscraft.bridge.data.ParamSetResponse;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.ServiceCallResponse;
import net.roscraft.bridge.data.ServiceInfoResponse;
import net.roscraft.bridge.data.TopicBwResponse;
import net.roscraft.bridge.data.TopicDelayResponse;
import net.roscraft.bridge.data.TopicHzResponse;
import net.roscraft.bridge.data.TopicInfoResponse;
import net.roscraft.bridge.data.TopicPayload;

/**
 * Callback interface implemented by the mod to receive outgoing ROS2 commands.
 *
 * <p>
 * All methods are invoked on the thread that calls {@link RoscraftBridge#tick},
 * so implementations must not block for extended periods. Heavy work should be
 * dispatched to the mod's own thread pool or Minecraft's server thread.
 *
 * <p>
 * Default no-op implementations are provided for every method so that
 * implementors only override the events they care about.
 */
public interface BridgeCallback {
  /**
   * Called when a full ROS2 graph snapshot arrives.
   *
   * @param snapshot
   *            Immutable snapshot of topics, services and actions.
   */
  default void onGraphSnapshot(GraphSnapshot snapshot) {}

  /**
   * Called when a node-info response arrives.
   *
   * @param response
   *            node publishers/subscribers/services and found flag.
   */
  default void onNodeInfoResponse(NodeInfoResponse response) {}

  /**
   * Called when a serialized ROS2 message is forwarded from a subscribed topic.
   *
   * @param payload
   *            Topic name, message type and serialized payload bytes.
   */
  default void onTopicPayload(TopicPayload payload) {}

  /**
   * Called when the ROS2 layer responds to a player-list query.
   *
   * @param playerList
   *            Request ID and list of current players.
   */
  default void onPlayerList(PlayerList playerList) {}

  /**
   * Called when a topic-info response arrives.
   *
   * @param response
   *            Topic type and publisher/subscriber counters.
   */
  default void onTopicInfoResponse(TopicInfoResponse response) {}

  /**
   * Called when a service-info response arrives.
   *
   * @param response
   *            Service type and client/server counters.
   */
  default void onServiceInfoResponse(ServiceInfoResponse response) {}

  /**
   * Called when an interface-list response arrives.
   *
   * @param response
   *            available interfaces grouped by kind.
   */
  default void onInterfaceListResponse(InterfaceListResponse response) {}

  /**
   * Called when an interface-show response arrives.
   *
   * @param response
   *            Interface definition lookup result.
   */
  default void onInterfaceShowResponse(InterfaceShowResponse response) {}

  default void onTopicHzResponse(TopicHzResponse response) {}

  default void onTopicBwResponse(TopicBwResponse response) {}

  default void onTopicDelayResponse(TopicDelayResponse response) {}

  default void onServiceCallResponse(ServiceCallResponse response) {}

  default void onParamListResponse(ParamListResponse response) {}

  default void onParamGetResponse(ParamGetResponse response) {}

  default void onParamSetResponse(ParamSetResponse response) {}

  default void onParamDescribeResponse(ParamDescribeResponse response) {}

  default void onParamDumpResponse(ParamDumpResponse response) {}

  default void onParamLoadResponse(ParamLoadResponse response) {}

  default void onActionInfoResponse(ActionInfoResponse response) {}

  default void onActionFeedback(ActionFeedback feedback) {}

  default void onActionResult(ActionResult result) {}

  /**
   * Called when an error occurs on the ROS bridge.
   *
   * @param error
   *            Error code, message, and originating request ID.
   */
  default void onError(BridgeError error) {}
}
