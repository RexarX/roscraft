package net.roscraft.mod.bridge.callback;

import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandRequestTracker;

/** Dispatches bridge events to domain-specific {@code /ros} command chat handlers. */
public final class CommandBridgeEventRouter {

  private final GraphCommandEvents graph;
  private final TopicCommandEvents topic;
  private final ServiceCommandEvents service;
  private final ParamCommandEvents param;
  private final ActionCommandEvents action;
  private final MiscCommandEvents misc;

  public CommandBridgeEventRouter(RoscraftMod mod, CommandRequestTracker requests) {
    CommandEventContext ctx = new CommandEventContext(mod, requests);
    this.graph = new GraphCommandEvents(ctx);
    this.topic = new TopicCommandEvents(ctx);
    this.service = new ServiceCommandEvents(ctx);
    this.param = new ParamCommandEvents(ctx);
    this.action = new ActionCommandEvents(ctx);
    this.misc = new MiscCommandEvents(ctx);
  }

  public void onEvent(BridgeEvent event) {
    switch (event) {
      case BridgeEvent.GraphSnapshot e -> graph.onGraphSnapshot(e);
      case BridgeEvent.NodeInfoResponse e -> graph.onNodeInfoResponse(e);
      case BridgeEvent.InterfaceListResponse e -> graph.onInterfaceListResponse(e);
      case BridgeEvent.InterfaceShowResponse e -> graph.onInterfaceShowResponse(e);
      case BridgeEvent.TopicPayload e -> topic.onTopicPayload(e);
      case BridgeEvent.TopicInfoResponse e -> topic.onTopicInfoResponse(e);
      case BridgeEvent.TopicHzResponse e -> topic.onTopicHzResponse(e);
      case BridgeEvent.TopicBwResponse e -> topic.onTopicBwResponse(e);
      case BridgeEvent.TopicDelayResponse e -> topic.onTopicDelayResponse(e);
      case BridgeEvent.ServiceInfoResponse e -> service.onServiceInfoResponse(e);
      case BridgeEvent.ServiceCallResponse e -> service.onServiceCallResponse(e);
      case BridgeEvent.ParamListResponse e -> param.onParamListResponse(e);
      case BridgeEvent.ParamGetResponse e -> param.onParamGetResponse(e);
      case BridgeEvent.ParamSetResponse e -> param.onParamSetResponse(e);
      case BridgeEvent.ParamDescribeResponse e -> param.onParamDescribeResponse(e);
      case BridgeEvent.ParamDumpResponse e -> param.onParamDumpResponse(e);
      case BridgeEvent.ParamLoadResponse e -> param.onParamLoadResponse(e);
      case BridgeEvent.ActionInfoResponse e -> action.onActionInfoResponse(e);
      case BridgeEvent.ActionFeedback e -> action.onActionFeedback(e);
      case BridgeEvent.ActionResult e -> action.onActionResult(e);
      case BridgeEvent.PlayerList e -> misc.onPlayerList(e);
      case BridgeEvent.AddonEvent e -> misc.onAddonEvent(e);
      case BridgeEvent.BridgeError e -> misc.onError(e);
      default -> {}
    }
  }
}
