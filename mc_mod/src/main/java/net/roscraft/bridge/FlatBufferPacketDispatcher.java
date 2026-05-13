package net.roscraft.bridge;

import java.util.ArrayList;
import java.util.List;
import java.util.function.IntFunction;
import java.util.function.Supplier;
import net.roscraft.bridge.event.BridgeEvent;
import roscraft.bridge.fbs.ActionFeedbackPacket;
import roscraft.bridge.fbs.ActionInfoResponsePacket;
import roscraft.bridge.fbs.ActionResultPacket;
import roscraft.bridge.fbs.AddonEventPacket;
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
    BridgeEvent event = dispatchEvent(packet);
    if (event != null) {
      callback().onEvent(event);
    }
  }

  private BridgeEvent dispatchEvent(BridgePacket packet) {
    return switch (packet.payloadType()) {
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
      case PacketPayload.AddonEventPacket -> handleAddonEvent(packet);
      default -> null;
    };
  }

  private BridgeEvent.GraphSnapshot handleGraphSnapshot(BridgePacket packet) {
    var snapshotPacket = (GraphSnapshotPacket) packet.payload(new GraphSnapshotPacket());
    if (snapshotPacket == null) {
      return null;
    }

    int nodeCount = snapshotPacket.nodesLength();
    List<BridgeEvent.NodeEntry> nodes = new ArrayList<>(nodeCount);
    var nodeEntry = new roscraft.bridge.fbs.NodeEntry();
    for (int i = 0; i < nodeCount; ++i) {
      snapshotPacket.nodes(nodeEntry, i);
      nodes.add(new BridgeEvent.NodeEntry(nodeEntry.name()));
    }

    int topicCount = snapshotPacket.topicsLength();
    List<BridgeEvent.TopicEntry> topics = new ArrayList<>(topicCount);
    var topicEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < topicCount; ++i) {
      snapshotPacket.topics(topicEntry, i);
      topics.add(new BridgeEvent.TopicEntry(topicEntry.name(), topicEntry.type()));
    }

    int serviceCount = snapshotPacket.servicesLength();
    List<BridgeEvent.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
      snapshotPacket.services(serviceEntry, i);
      services.add(new BridgeEvent.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    int actionCount = snapshotPacket.actionsLength();
    List<BridgeEvent.ActionEntry> actions = new ArrayList<>(actionCount);
    var actionEntry = new roscraft.bridge.fbs.ActionEntry();
    for (int i = 0; i < actionCount; ++i) {
      snapshotPacket.actions(actionEntry, i);
      actions.add(new BridgeEvent.ActionEntry(actionEntry.name(), actionEntry.type()));
    }

    return new BridgeEvent.GraphSnapshot(
        snapshotPacket.requestId(),
        List.copyOf(nodes),
        List.copyOf(topics),
        List.copyOf(services),
        List.copyOf(actions));
  }

  private BridgeEvent.TopicPayload handleTopicPayload(BridgePacket packet) {
    var payloadPacket = (TopicPayloadPacket) packet.payload(new TopicPayloadPacket());
    if (payloadPacket == null) {
      return null;
    }

    byte[] payload = readByteVector(payloadPacket.payloadLength(), payloadPacket::payload);
    return new BridgeEvent.TopicPayload(
        payloadPacket.requestId(),
        payloadPacket.topicName(),
        payloadPacket.messageType(),
        payloadPacket.raw(),
        payload);
  }

  private BridgeEvent.NodeInfoResponse handleNodeInfoResponse(BridgePacket packet) {
    var response = (NodeInfoResponsePacket) packet.payload(new NodeInfoResponsePacket());
    if (response == null) {
      return null;
    }

    int publisherCount = response.publishersLength();
    List<BridgeEvent.TopicEntry> publishers = new ArrayList<>(publisherCount);
    var publisherEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < publisherCount; ++i) {
      response.publishers(publisherEntry, i);
      publishers.add(new BridgeEvent.TopicEntry(publisherEntry.name(), publisherEntry.type()));
    }

    int subscriberCount = response.subscribersLength();
    List<BridgeEvent.TopicEntry> subscribers = new ArrayList<>(subscriberCount);
    var subscriberEntry = new roscraft.bridge.fbs.TopicEntry();
    for (int i = 0; i < subscriberCount; ++i) {
      response.subscribers(subscriberEntry, i);
      subscribers.add(new BridgeEvent.TopicEntry(subscriberEntry.name(), subscriberEntry.type()));
    }

    int serviceCount = response.servicesLength();
    List<BridgeEvent.ServiceEntry> services = new ArrayList<>(serviceCount);
    var serviceEntry = new roscraft.bridge.fbs.ServiceEntry();
    for (int i = 0; i < serviceCount; ++i) {
      response.services(serviceEntry, i);
      services.add(new BridgeEvent.ServiceEntry(serviceEntry.name(), serviceEntry.type()));
    }

    return new BridgeEvent.NodeInfoResponse(
        response.requestId(),
        response.nodeName(),
        List.copyOf(publishers),
        List.copyOf(subscribers),
        List.copyOf(services),
        response.found());
  }

  private BridgeEvent.TopicInfoResponse handleTopicInfoResponse(BridgePacket packet) {
    var response = (TopicInfoResponsePacket) packet.payload(new TopicInfoResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.TopicInfoResponse(
        response.requestId(),
        response.topicName(),
        response.messageType(),
        response.publisherCount(),
        response.subscriberCount(),
        readStringVector(response.publisherNodesLength(), response::publisherNodes),
        readStringVector(response.subscriberNodesLength(), response::subscriberNodes));
  }

  private BridgeEvent.ServiceInfoResponse handleServiceInfoResponse(BridgePacket packet) {
    var response = (ServiceInfoResponsePacket) packet.payload(new ServiceInfoResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ServiceInfoResponse(
        response.requestId(),
        response.serviceName(),
        response.serviceType(),
        response.clientCount(),
        response.serverCount(),
        readStringVector(response.clientNodesLength(), response::clientNodes),
        readStringVector(response.serverNodesLength(), response::serverNodes));
  }

  private BridgeEvent.InterfaceListResponse handleInterfaceListResponse(BridgePacket packet) {
    var response = (InterfaceListResponsePacket) packet.payload(new InterfaceListResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.InterfaceListResponse(
        response.requestId(),
        readStringVector(response.messagesLength(), response::messages),
        readStringVector(response.servicesLength(), response::services),
        readStringVector(response.actionsLength(), response::actions));
  }

  private BridgeEvent.InterfaceShowResponse handleInterfaceShowResponse(BridgePacket packet) {
    var response = (InterfaceShowResponsePacket) packet.payload(new InterfaceShowResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.InterfaceShowResponse(
        response.requestId(), response.interfaceType(), response.definition(), response.found());
  }

  private BridgeEvent.PlayerList handlePlayerList(BridgePacket packet) {
    var playerListPacket = (PlayerListPacket) packet.payload(new PlayerListPacket());
    if (playerListPacket == null) {
      return null;
    }

    int count = playerListPacket.playersLength();
    List<BridgeEvent.Player> players = new ArrayList<>(count);
    var entry = new PlayerEntry();
    for (int i = 0; i < count; ++i) {
      playerListPacket.players(entry, i);
      players.add(new BridgeEvent.Player(entry.name(), entry.x(), entry.y(), entry.z()));
    }

    return new BridgeEvent.PlayerList(playerListPacket.requestId(), players);
  }

  private BridgeEvent.BridgeError handleError(BridgePacket packet) {
    var errorPacket = (ErrorPacket) packet.payload(new ErrorPacket());
    if (errorPacket == null) {
      return null;
    }

    return new BridgeEvent.BridgeError(
        errorPacket.requestId(), errorPacket.errorCode(), errorPacket.errorMessage());
  }

  private BridgeEvent.TopicHzResponse handleTopicHzResponse(BridgePacket packet) {
    var response = (TopicHzResponsePacket) packet.payload(new TopicHzResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.TopicHzResponse(
        response.requestId(),
        response.topicName(),
        response.frequency(),
        (int) response.window(),
        (int) response.messageCount());
  }

  private BridgeEvent.TopicBwResponse handleTopicBwResponse(BridgePacket packet) {
    var response = (TopicBwResponsePacket) packet.payload(new TopicBwResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.TopicBwResponse(
        response.requestId(),
        response.topicName(),
        response.bytesPerSecond(),
        (int) response.window(),
        (int) response.messageCount(),
        response.totalBytes());
  }

  private BridgeEvent.TopicDelayResponse handleTopicDelayResponse(BridgePacket packet) {
    var response = (TopicDelayResponsePacket) packet.payload(new TopicDelayResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.TopicDelayResponse(
        response.requestId(),
        response.topicName(),
        response.averageDelay(),
        response.minDelay(),
        response.maxDelay(),
        (int) response.window(),
        (int) response.messageCount());
  }

  private BridgeEvent.ServiceCallResponse handleServiceCallResponse(BridgePacket packet) {
    var response = (ServiceCallResponsePacket) packet.payload(new ServiceCallResponsePacket());
    if (response == null) {
      return null;
    }

    byte[] payload = readByteVector(response.responsePayloadLength(), response::responsePayload);
    return new BridgeEvent.ServiceCallResponse(
        response.requestId(),
        response.serviceName(),
        response.serviceType(),
        response.success(),
        payload,
        response.resultText());
  }

  private BridgeEvent.ParamListResponse handleParamListResponse(BridgePacket packet) {
    var response = (ParamListResponsePacket) packet.payload(new ParamListResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamListResponse(
        response.requestId(),
        response.nodeName(),
        readStringVector(response.namesLength(), response::names),
        readStringVector(response.prefixesLength(), response::prefixes),
        readStringVector(response.typesLength(), response::types));
  }

  private BridgeEvent.ParamGetResponse handleParamGetResponse(BridgePacket packet) {
    var response = (ParamGetResponsePacket) packet.payload(new ParamGetResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamGetResponse(
        response.requestId(),
        response.nodeName(),
        response.paramName(),
        response.found(),
        response.paramType(),
        response.valueText(),
        response.typeHidden());
  }

  private BridgeEvent.ParamSetResponse handleParamSetResponse(BridgePacket packet) {
    var response = (ParamSetResponsePacket) packet.payload(new ParamSetResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamSetResponse(
        response.requestId(),
        response.nodeName(),
        response.paramName(),
        response.success(),
        response.reason(),
        response.paramType(),
        response.valueText());
  }

  private BridgeEvent.ParamDescribeResponse handleParamDescribeResponse(BridgePacket packet) {
    var response = (ParamDescribeResponsePacket) packet.payload(new ParamDescribeResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamDescribeResponse(
        response.requestId(),
        response.nodeName(),
        response.paramName(),
        response.found(),
        response.paramType(),
        response.description(),
        response.readOnly(),
        response.constraints());
  }

  private BridgeEvent.ParamDumpResponse handleParamDumpResponse(BridgePacket packet) {
    var response = (ParamDumpResponsePacket) packet.payload(new ParamDumpResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamDumpResponse(
        response.requestId(), response.nodeName(), response.yamlText());
  }

  private BridgeEvent.ParamLoadResponse handleParamLoadResponse(BridgePacket packet) {
    var response = (ParamLoadResponsePacket) packet.payload(new ParamLoadResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ParamLoadResponse(
        response.requestId(),
        response.nodeName(),
        response.success(),
        response.reason() == null ? "" : response.reason(),
        response.paramsLoaded());
  }

  private BridgeEvent.ActionInfoResponse handleActionInfoResponse(BridgePacket packet) {
    var response = (ActionInfoResponsePacket) packet.payload(new ActionInfoResponsePacket());
    if (response == null) {
      return null;
    }

    return new BridgeEvent.ActionInfoResponse(
        response.requestId(),
        response.actionName(),
        response.actionType(),
        response.clientCount(),
        response.serverCount(),
        response.feedbackPublisherCount(),
        response.feedbackSubscriberCount(),
        response.statusPublisherCount(),
        response.statusSubscriberCount());
  }

  private BridgeEvent.ActionFeedback handleActionFeedback(BridgePacket packet) {
    var response = (ActionFeedbackPacket) packet.payload(new ActionFeedbackPacket());
    if (response == null) {
      return null;
    }

    byte[] payload = readByteVector(response.feedbackPayloadLength(), response::feedbackPayload);
    return new BridgeEvent.ActionFeedback(
        response.requestId(),
        response.actionName(),
        response.actionType(),
        payload,
        response.feedbackText());
  }

  private BridgeEvent.ActionResult handleActionResult(BridgePacket packet) {
    var response = (ActionResultPacket) packet.payload(new ActionResultPacket());
    if (response == null) {
      return null;
    }

    byte[] payload = readByteVector(response.resultPayloadLength(), response::resultPayload);
    return new BridgeEvent.ActionResult(
        response.requestId(),
        response.actionName(),
        response.actionType(),
        response.success(),
        payload,
        response.resultText());
  }

  private BridgeEvent.AddonEvent handleAddonEvent(BridgePacket packet) {
    var eventPacket = (AddonEventPacket) packet.payload(new AddonEventPacket());
    if (eventPacket == null) {
      return null;
    }

    byte[] payload = readByteVector(eventPacket.payloadLength(), eventPacket::payload);
    return new BridgeEvent.AddonEvent(
        eventPacket.requestId(),
        eventPacket.addonId(),
        eventPacket.eventType(),
        eventPacket.encoding(),
        payload,
        eventPacket.response());
  }

  private static byte[] readByteVector(int length, IntFunction<Integer> accessor) {
    byte[] out = new byte[length];
    for (int i = 0; i < length; ++i) {
      out[i] = accessor.apply(i).byteValue();
    }
    return out;
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
