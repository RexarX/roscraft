package net.roscraft.bridge;

import java.util.ArrayList;
import java.util.List;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import net.roscraft.bridge.data.ActionFeedback;
import net.roscraft.bridge.data.ActionInfoResponse;
import net.roscraft.bridge.data.ActionResult;
import net.roscraft.bridge.data.BridgeError;
import net.roscraft.bridge.data.InterfaceListResponse;
import net.roscraft.bridge.data.InterfaceShowResponse;
import net.roscraft.bridge.data.NodeInfoResponse;
import net.roscraft.bridge.data.ParamDescribeResponse;
import net.roscraft.bridge.data.ParamDumpResponse;
import net.roscraft.bridge.data.ParamGetResponse;
import net.roscraft.bridge.data.ParamListResponse;
import net.roscraft.bridge.data.ParamLoadResponse;
import net.roscraft.bridge.data.ParamSetResponse;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.ServiceCallResponse;
import net.roscraft.bridge.data.ServiceInfoResponse;
import net.roscraft.bridge.data.TopicBwResponse;
import net.roscraft.bridge.data.TopicDelayResponse;
import net.roscraft.bridge.data.TopicHzResponse;
import net.roscraft.bridge.data.TopicInfoResponse;
import net.roscraft.bridge.data.TopicPayload;
import roscraft.bridge.fbs.ActionFeedbackPacket;
import roscraft.bridge.fbs.ActionInfoResponsePacket;
import roscraft.bridge.fbs.ActionResultPacket;
import roscraft.bridge.fbs.BridgePacket;
import roscraft.bridge.fbs.ErrorPacket;
import roscraft.bridge.fbs.GraphSnapshotPacket;
import roscraft.bridge.fbs.InterfaceListResponsePacket;
import roscraft.bridge.fbs.InterfaceShowResponsePacket;
import roscraft.bridge.fbs.NodeInfoResponsePacket;
import roscraft.bridge.fbs.PacketPayload;
import roscraft.bridge.fbs.ParamDescribeResponsePacket;
import roscraft.bridge.fbs.ParamDumpResponsePacket;
import roscraft.bridge.fbs.ParamGetResponsePacket;
import roscraft.bridge.fbs.ParamListResponsePacket;
import roscraft.bridge.fbs.ParamLoadResponsePacket;
import roscraft.bridge.fbs.ParamSetResponsePacket;
import roscraft.bridge.fbs.PlayerEntry;
import roscraft.bridge.fbs.PlayerListPacket;
import roscraft.bridge.fbs.ServiceCallResponsePacket;
import roscraft.bridge.fbs.ServiceInfoResponsePacket;
import roscraft.bridge.fbs.TopicBwResponsePacket;
import roscraft.bridge.fbs.TopicDelayResponsePacket;
import roscraft.bridge.fbs.TopicHzResponsePacket;
import roscraft.bridge.fbs.TopicInfoResponsePacket;
import roscraft.bridge.fbs.TopicPayloadPacket;

final class FlatBufferPacketDispatcher {

  private final Supplier<BridgeCallback> callbackSupplier;

  FlatBufferPacketDispatcher(Supplier<BridgeCallback> callbackSupplier) {
    this.callbackSupplier = callbackSupplier;
  }

  void dispatch(BridgePacket packet) {
    switch (packet.payloadType()) {
      case PacketPayload.GraphSnapshotPacket -> handleGraphSnapshot(packet);
      case PacketPayload.NodeInfoResponsePacket -> handleNodeInfoResponse(packet);
      case PacketPayload.TopicInfoResponsePacket -> handleTopicInfoResponse(packet);
      case PacketPayload.ServiceInfoResponsePacket -> handleServiceInfoResponse(packet);
      case PacketPayload.InterfaceListResponsePacket -> handleInterfaceListResponse(packet);
      case PacketPayload.InterfaceShowResponsePacket -> handleInterfaceShowResponse(packet);
      case PacketPayload.TopicPayloadPacket -> handleTopicPayload(packet);
      case PacketPayload.PlayerListPacket -> handlePlayerList(packet);
      case PacketPayload.ErrorPacket -> handleError(packet);
      case PacketPayload.TopicHzResponsePacket -> handleTopicHzResponse(packet);
      case PacketPayload.TopicBwResponsePacket -> handleTopicBwResponse(packet);
      case PacketPayload.TopicDelayResponsePacket -> handleTopicDelayResponse(packet);
      case PacketPayload.ServiceCallResponsePacket -> handleServiceCallResponse(packet);
      case PacketPayload.ParamListResponsePacket -> handleParamListResponse(packet);
      case PacketPayload.ParamGetResponsePacket -> handleParamGetResponse(packet);
      case PacketPayload.ParamSetResponsePacket -> handleParamSetResponse(packet);
      case PacketPayload.ParamDescribeResponsePacket -> handleParamDescribeResponse(packet);
      case PacketPayload.ParamDumpResponsePacket -> handleParamDumpResponse(packet);
      case PacketPayload.ParamLoadResponsePacket -> handleParamLoadResponse(packet);
      case PacketPayload.ActionInfoResponsePacket -> handleActionInfoResponse(packet);
      case PacketPayload.ActionFeedbackPacket -> handleActionFeedback(packet);
      case PacketPayload.ActionResultPacket -> handleActionResult(packet);
      default -> {}
    }
  }

  private void handleGraphSnapshot(BridgePacket packet) {
    var snapshotPacket = (GraphSnapshotPacket) packet.payload(new GraphSnapshotPacket());
    if (snapshotPacket == null) {
      return;
    }

    int nodeCount = snapshotPacket.nodesLength();
    List<net.roscraft.bridge.data.NodeEntry> nodes = new ArrayList<>(nodeCount);
    var nodeEntry = new roscraft.bridge.fbs.NodeEntry();
    for (int i = 0; i < nodeCount; ++i) {
      snapshotPacket.nodes(nodeEntry, i);
      nodes.add(new net.roscraft.bridge.data.NodeEntry(nodeEntry.name()));
    }

    int topicCount = snapshotPacket.topicsLength();
    List<net.roscraft.bridge.data.TopicEntry> topics = new ArrayList<>(topicCount);
    var topicEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < topicCount; ++i) {
      snapshotPacket.topics(topicEntry, i);
      topics.add(new net.roscraft.bridge.data.TopicEntry(topicEntry.name(), topicEntry.type()));
    }

    int serviceCount = snapshotPacket.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
      snapshotPacket.services(serviceEntry, i);
      services.add(
          new net.roscraft.bridge.data.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    int actionCount = snapshotPacket.actionsLength();
    List<net.roscraft.bridge.data.ActionEntry> actions = new ArrayList<>(actionCount);
    var actionEntry = new roscraft.bridge.fbs.ActionEntry();
    for (int i = 0; i < actionCount; ++i) {
      snapshotPacket.actions(actionEntry, i);
      actions.add(new net.roscraft.bridge.data.ActionEntry(actionEntry.name(), actionEntry.type()));
    }

    callback()
        .onGraphSnapshot(new net.roscraft.bridge.data.GraphSnapshot(
            snapshotPacket.requestId(),
            List.copyOf(nodes),
            List.copyOf(topics),
            List.copyOf(services),
            List.copyOf(actions)));
  }

  private void handleTopicPayload(BridgePacket packet) {
    var payloadPacket = (TopicPayloadPacket) packet.payload(new TopicPayloadPacket());
    if (payloadPacket == null) {
      return;
    }

    byte[] payload = new byte[payloadPacket.payloadLength()];
    for (int i = 0; i < payloadPacket.payloadLength(); ++i) {
      payload[i] = (byte) payloadPacket.payload(i);
    }

    callback()
        .onTopicPayload(new TopicPayload(
            payloadPacket.requestId(),
            payloadPacket.topicName(),
            payloadPacket.messageType(),
            payloadPacket.raw(),
            payload));
  }

  private void handleNodeInfoResponse(BridgePacket packet) {
    var response = (NodeInfoResponsePacket) packet.payload(new NodeInfoResponsePacket());
    if (response == null) {
      return;
    }

    int publisherCount = response.publishersLength();
    List<net.roscraft.bridge.data.TopicEntry> publishers = new ArrayList<>(publisherCount);
    var publisherEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < publisherCount; ++i) {
      response.publishers(publisherEntry, i);
      publishers.add(
          new net.roscraft.bridge.data.TopicEntry(publisherEntry.name(), publisherEntry.type()));
    }

    int subscriberCount = response.subscribersLength();
    List<net.roscraft.bridge.data.TopicEntry> subscribers = new ArrayList<>(subscriberCount);
    var subscriberEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < subscriberCount; ++i) {
      response.subscribers(subscriberEntry, i);
      subscribers.add(
          new net.roscraft.bridge.data.TopicEntry(subscriberEntry.name(), subscriberEntry.type()));
    }

    int serviceCount = response.servicesLength();
    List<net.roscraft.bridge.data.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
      response.services(serviceEntry, i);
      services.add(
          new net.roscraft.bridge.data.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    callback()
        .onNodeInfoResponse(new NodeInfoResponse(
            response.requestId(),
            response.nodeName(),
            List.copyOf(publishers),
            List.copyOf(subscribers),
            List.copyOf(services),
            response.found()));
  }

  private void handleTopicInfoResponse(BridgePacket packet) {
    var response = (TopicInfoResponsePacket) packet.payload(new TopicInfoResponsePacket());
    if (response == null) {
      return;
    }

    List<String> publisherNodes =
        readStringVector(response.publisherNodesLength(), response::publisherNodes);
    List<String> subscriberNodes =
        readStringVector(response.subscriberNodesLength(), response::subscriberNodes);

    callback()
        .onTopicInfoResponse(new TopicInfoResponse(
            response.requestId(),
            response.topicName(),
            response.messageType(),
            response.publisherCount(),
            response.subscriberCount(),
            publisherNodes,
            subscriberNodes));
  }

  private void handleServiceInfoResponse(BridgePacket packet) {
    var response = (ServiceInfoResponsePacket) packet.payload(new ServiceInfoResponsePacket());
    if (response == null) {
      return;
    }

    List<String> clientNodes =
        readStringVector(response.clientNodesLength(), response::clientNodes);
    List<String> serverNodes =
        readStringVector(response.serverNodesLength(), response::serverNodes);

    callback()
        .onServiceInfoResponse(new ServiceInfoResponse(
            response.requestId(),
            response.serviceName(),
            response.serviceType(),
            response.clientCount(),
            response.serverCount(),
            clientNodes,
            serverNodes));
  }

  private void handleInterfaceListResponse(BridgePacket packet) {
    var response = (InterfaceListResponsePacket) packet.payload(new InterfaceListResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onInterfaceListResponse(new InterfaceListResponse(
            response.requestId(),
            readStringVector(response.messagesLength(), response::messages),
            readStringVector(response.servicesLength(), response::services),
            readStringVector(response.actionsLength(), response::actions)));
  }

  private void handleInterfaceShowResponse(BridgePacket packet) {
    var response = (InterfaceShowResponsePacket) packet.payload(new InterfaceShowResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onInterfaceShowResponse(new InterfaceShowResponse(
            response.requestId(),
            response.interfaceType(),
            response.definition(),
            response.found()));
  }

  private void handlePlayerList(BridgePacket packet) {
    var playerListPacket = (PlayerListPacket) packet.payload(new PlayerListPacket());
    if (playerListPacket == null) {
      return;
    }

    int count = playerListPacket.playersLength();
    List<Player> players = new ArrayList<>(count);
    var entry = new PlayerEntry();
    for (int i = 0; i < count; ++i) {
      playerListPacket.players(entry, i);
      players.add(new Player(entry.name(), entry.x(), entry.y(), entry.z()));
    }

    callback().onPlayerList(new PlayerList(playerListPacket.requestId(), players));
  }

  private void handleError(BridgePacket packet) {
    var errorPacket = (ErrorPacket) packet.payload(new ErrorPacket());
    if (errorPacket == null) {
      return;
    }

    callback()
        .onError(new BridgeError(
            errorPacket.requestId(), errorPacket.errorCode(), errorPacket.errorMessage()));
  }

  private void handleTopicHzResponse(BridgePacket packet) {
    var response = (TopicHzResponsePacket) packet.payload(new TopicHzResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onTopicHzResponse(new TopicHzResponse(
            response.requestId(),
            response.topicName(),
            response.frequency(),
            (int) response.window(),
            (int) response.messageCount()));
  }

  private void handleTopicBwResponse(BridgePacket packet) {
    var response = (TopicBwResponsePacket) packet.payload(new TopicBwResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onTopicBwResponse(new TopicBwResponse(
            response.requestId(),
            response.topicName(),
            response.bytesPerSecond(),
            (int) response.window(),
            (int) response.messageCount(),
            response.totalBytes()));
  }

  private void handleTopicDelayResponse(BridgePacket packet) {
    var response = (TopicDelayResponsePacket) packet.payload(new TopicDelayResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onTopicDelayResponse(new TopicDelayResponse(
            response.requestId(),
            response.topicName(),
            response.averageDelay(),
            response.minDelay(),
            response.maxDelay(),
            (int) response.window(),
            (int) response.messageCount()));
  }

  private void handleServiceCallResponse(BridgePacket packet) {
    var response = (ServiceCallResponsePacket) packet.payload(new ServiceCallResponsePacket());
    if (response == null) {
      return;
    }

    byte[] payload = new byte[response.responsePayloadLength()];
    for (int i = 0; i < response.responsePayloadLength(); i++) {
      payload[i] = (byte) response.responsePayload(i);
    }

    callback()
        .onServiceCallResponse(new ServiceCallResponse(
            response.requestId(),
            response.serviceName(),
            response.serviceType(),
            response.success(),
            payload,
            response.resultText()));
  }

  private void handleParamListResponse(BridgePacket packet) {
    var response = (ParamListResponsePacket) packet.payload(new ParamListResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamListResponse(new ParamListResponse(
            response.requestId(),
            response.nodeName(),
            readStringVector(response.namesLength(), response::names),
            readStringVector(response.prefixesLength(), response::prefixes),
            readStringVector(response.typesLength(), response::types)));
  }

  private void handleParamGetResponse(BridgePacket packet) {
    var response = (ParamGetResponsePacket) packet.payload(new ParamGetResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamGetResponse(new ParamGetResponse(
            response.requestId(),
            response.nodeName(),
            response.paramName(),
            response.found(),
            response.paramType(),
            response.valueText(),
            response.typeHidden()));
  }

  private void handleParamSetResponse(BridgePacket packet) {
    var response = (ParamSetResponsePacket) packet.payload(new ParamSetResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamSetResponse(new ParamSetResponse(
            response.requestId(),
            response.nodeName(),
            response.paramName(),
            response.success(),
            response.reason(),
            response.paramType(),
            response.valueText()));
  }

  private void handleParamDescribeResponse(BridgePacket packet) {
    var response = (ParamDescribeResponsePacket) packet.payload(new ParamDescribeResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamDescribeResponse(new ParamDescribeResponse(
            response.requestId(),
            response.nodeName(),
            response.paramName(),
            response.found(),
            response.paramType(),
            response.description(),
            response.readOnly(),
            response.constraints()));
  }

  private void handleParamDumpResponse(BridgePacket packet) {
    var response = (ParamDumpResponsePacket) packet.payload(new ParamDumpResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamDumpResponse(
            new ParamDumpResponse(response.requestId(), response.nodeName(), response.yamlText()));
  }

  private void handleParamLoadResponse(BridgePacket packet) {
    var response = (ParamLoadResponsePacket) packet.payload(new ParamLoadResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onParamLoadResponse(new ParamLoadResponse(
            response.requestId(),
            response.nodeName(),
            response.success(),
            response.reason() == null ? "" : response.reason(),
            response.paramsLoaded()));
  }

  private void handleActionInfoResponse(BridgePacket packet) {
    var response = (ActionInfoResponsePacket) packet.payload(new ActionInfoResponsePacket());
    if (response == null) {
      return;
    }

    callback()
        .onActionInfoResponse(new ActionInfoResponse(
            response.requestId(),
            response.actionName(),
            response.actionType(),
            response.clientCount(),
            response.serverCount(),
            response.feedbackPublisherCount(),
            response.feedbackSubscriberCount(),
            response.statusPublisherCount(),
            response.statusSubscriberCount()));
  }

  private void handleActionFeedback(BridgePacket packet) {
    var response = (ActionFeedbackPacket) packet.payload(new ActionFeedbackPacket());
    if (response == null) {
      return;
    }

    byte[] payload = new byte[response.feedbackPayloadLength()];
    for (int i = 0; i < response.feedbackPayloadLength(); i++) {
      payload[i] = (byte) response.feedbackPayload(i);
    }

    callback()
        .onActionFeedback(new ActionFeedback(
            response.requestId(),
            response.actionName(),
            response.actionType(),
            payload,
            response.feedbackText()));
  }

  private void handleActionResult(BridgePacket packet) {
    var response = (ActionResultPacket) packet.payload(new ActionResultPacket());
    if (response == null) {
      return;
    }

    byte[] payload = new byte[response.resultPayloadLength()];
    for (int i = 0; i < response.resultPayloadLength(); i++) {
      payload[i] = (byte) response.resultPayload(i);
    }

    callback()
        .onActionResult(new ActionResult(
            response.requestId(),
            response.actionName(),
            response.actionType(),
            response.success(),
            payload,
            response.resultText()));
  }

  private static List<String> readStringVector(int length, IntFunction<String> accessor) {
    List<String> values = new ArrayList<>(length);
    for (int i = 0; i < length; ++i) {
      values.add(accessor.apply(i));
    }
    return List.copyOf(values);
  }

  private BridgeCallback callback() {
    return callbackSupplier.get();
  }
}
