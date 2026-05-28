package net.roscraft.bridge.event;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Sealed interface for all bridge response events.
 *
 * <p>Each record below corresponds to one response packet type. Addons receive
 * these via {@code onBridgeEvent(BridgeEvent)} and can filter by type using
 * pattern matching or {@link BridgeEventBus} subscriptions.
 *
 * <p>Adding a new response packet type requires adding exactly one record to
 * this interface — nothing else.
 */
public sealed interface BridgeEvent {

  /** Opaque identifier echoed from the originating request. */
  long requestId();

  // ── Helper types ──────────────────────────────────────────────────────────

  record NodeEntry(String name) {
    public NodeEntry {
      Objects.requireNonNull(name, "name must not be null");
    }
  }

  record TopicEntry(String name, String type) {
    public TopicEntry {
      Objects.requireNonNull(name, "name must not be null");
      Objects.requireNonNull(type, "type must not be null");
    }
  }

  record ServiceEntry(String name, String type) {
    public ServiceEntry {
      Objects.requireNonNull(name, "name must not be null");
      Objects.requireNonNull(type, "type must not be null");
    }
  }

  record ActionEntry(String name, String type) {
    public ActionEntry {
      Objects.requireNonNull(name, "name must not be null");
      Objects.requireNonNull(type, "type must not be null");
    }
  }

  record Player(String name, float x, float y, float z) {
    public Player {
      Objects.requireNonNull(name, "name must not be null");
    }
  }

  // ── Graph & introspection ─────────────────────────────────────────────────

  record GraphSnapshot(
      long requestId,
      List<NodeEntry> nodes,
      List<TopicEntry> topics,
      List<ServiceEntry> services,
      List<ActionEntry> actions)
      implements BridgeEvent {
    public GraphSnapshot {
      Objects.requireNonNull(nodes, "nodes must not be null");
      Objects.requireNonNull(topics, "topics must not be null");
      Objects.requireNonNull(services, "services must not be null");
      Objects.requireNonNull(actions, "actions must not be null");
      nodes = List.copyOf(nodes);
      topics = List.copyOf(topics);
      services = List.copyOf(services);
      actions = List.copyOf(actions);
    }

    public boolean isEmpty() {
      return nodes.isEmpty() && topics.isEmpty() && services.isEmpty() && actions.isEmpty();
    }
  }

  record NodeInfoResponse(
      long requestId,
      String nodeName,
      List<TopicEntry> publishers,
      List<TopicEntry> subscribers,
      List<ServiceEntry> services,
      boolean found)
      implements BridgeEvent {
    public NodeInfoResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(publishers, "publishers must not be null");
      Objects.requireNonNull(subscribers, "subscribers must not be null");
      Objects.requireNonNull(services, "services must not be null");
      publishers = List.copyOf(publishers);
      subscribers = List.copyOf(subscribers);
      services = List.copyOf(services);
    }
  }

  record TopicInfoResponse(
      long requestId,
      String topicName,
      String messageType,
      long publisherCount,
      long subscriberCount,
      List<String> publisherNodes,
      List<String> subscriberNodes)
      implements BridgeEvent {
    public TopicInfoResponse {
      Objects.requireNonNull(topicName, "topicName must not be null");
      Objects.requireNonNull(publisherNodes, "publisherNodes must not be null");
      Objects.requireNonNull(subscriberNodes, "subscriberNodes must not be null");
      messageType = messageType == null ? "" : messageType;
      publisherNodes = List.copyOf(publisherNodes);
      subscriberNodes = List.copyOf(subscriberNodes);
    }

    public boolean hasMessageType() {
      return !messageType.isBlank();
    }
  }

  record ServiceInfoResponse(
      long requestId,
      String serviceName,
      String serviceType,
      long clientCount,
      long serverCount,
      List<String> clientNodes,
      List<String> serverNodes)
      implements BridgeEvent {
    public ServiceInfoResponse {
      Objects.requireNonNull(serviceName, "serviceName must not be null");
      Objects.requireNonNull(clientNodes, "clientNodes must not be null");
      Objects.requireNonNull(serverNodes, "serverNodes must not be null");
      serviceType = serviceType == null ? "" : serviceType;
      clientNodes = List.copyOf(clientNodes);
      serverNodes = List.copyOf(serverNodes);
    }

    public boolean hasServiceType() {
      return !serviceType.isBlank();
    }
  }

  record InterfaceListResponse(
      long requestId, List<String> messages, List<String> services, List<String> actions)
      implements BridgeEvent {
    public InterfaceListResponse {
      Objects.requireNonNull(messages, "messages must not be null");
      Objects.requireNonNull(services, "services must not be null");
      Objects.requireNonNull(actions, "actions must not be null");
      messages = List.copyOf(messages);
      services = List.copyOf(services);
      actions = List.copyOf(actions);
    }
  }

  record InterfaceShowResponse(
      long requestId, String interfaceType, String definition, boolean found)
      implements BridgeEvent {
    public InterfaceShowResponse {
      Objects.requireNonNull(interfaceType, "interfaceType must not be null");
      definition = definition == null ? "" : definition;
    }
  }

  // ── Topic pub/sub ─────────────────────────────────────────────────────────

  record TopicPayload(
      long requestId, String topicName, String messageType, boolean raw, byte[] payload)
      implements BridgeEvent {
    public TopicPayload {
      Objects.requireNonNull(topicName, "topicName must not be null");
      Objects.requireNonNull(messageType, "messageType must not be null");
      Objects.requireNonNull(payload, "payload must not be null");
      payload = Arrays.copyOf(payload, payload.length);
    }

    public int payloadLength() {
      return payload.length;
    }
  }

  // ── Topic statistics ──────────────────────────────────────────────────────

  record TopicHzResponse(
      long requestId, String topicName, double frequency, int window, int messageCount)
      implements BridgeEvent {
    public TopicHzResponse {
      topicName = topicName == null ? "" : topicName;
    }
  }

  record TopicBwResponse(
      long requestId,
      String topicName,
      double bytesPerSecond,
      int window,
      int messageCount,
      long totalBytes)
      implements BridgeEvent {
    public TopicBwResponse {
      topicName = topicName == null ? "" : topicName;
    }
  }

  record TopicDelayResponse(
      long requestId,
      String topicName,
      double averageDelay,
      double minDelay,
      double maxDelay,
      int window,
      int messageCount)
      implements BridgeEvent {
    public TopicDelayResponse {
      topicName = topicName == null ? "" : topicName;
    }
  }

  // ── Services ──────────────────────────────────────────────────────────────

  record ServiceCallResponse(
      long requestId,
      String serviceName,
      String serviceType,
      boolean success,
      byte[] responsePayload,
      String resultText)
      implements BridgeEvent {
    public ServiceCallResponse {
      Objects.requireNonNull(serviceName, "serviceName must not be null");
      Objects.requireNonNull(serviceType, "serviceType must not be null");
      responsePayload = responsePayload == null
          ? new byte[0]
          : Arrays.copyOf(responsePayload, responsePayload.length);
      resultText = resultText == null ? "" : resultText;
    }

    public int payloadLength() {
      return responsePayload.length;
    }
  }

  // ── Parameters ────────────────────────────────────────────────────────────

  record ParamListResponse(
      long requestId,
      String nodeName,
      List<String> names,
      List<String> prefixes,
      List<String> types)
      implements BridgeEvent {
    public ParamListResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(names, "names must not be null");
      Objects.requireNonNull(prefixes, "prefixes must not be null");
      Objects.requireNonNull(types, "types must not be null");
      names = List.copyOf(names);
      prefixes = List.copyOf(prefixes);
      types = List.copyOf(types);
    }
  }

  record ParamGetResponse(
      long requestId,
      String nodeName,
      String paramName,
      boolean found,
      String paramType,
      String valueText,
      boolean typeHidden)
      implements BridgeEvent {
    public ParamGetResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
      paramType = paramType == null ? "" : paramType;
      valueText = valueText == null ? "" : valueText;
    }
  }

  record ParamSetResponse(
      long requestId,
      String nodeName,
      String paramName,
      boolean success,
      String reason,
      String paramType,
      String valueText)
      implements BridgeEvent {
    public ParamSetResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
      reason = reason == null ? "" : reason;
      paramType = paramType == null ? "" : paramType;
      valueText = valueText == null ? "" : valueText;
    }
  }

  record ParamDescribeResponse(
      long requestId,
      String nodeName,
      String paramName,
      boolean found,
      String paramType,
      String description,
      boolean readOnly,
      String constraints)
      implements BridgeEvent {
    public ParamDescribeResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
      paramType = paramType == null ? "" : paramType;
      description = description == null ? "" : description;
      constraints = constraints == null ? "" : constraints;
    }
  }

  record ParamDumpResponse(long requestId, String nodeName, String yamlText)
      implements BridgeEvent {
    public ParamDumpResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      yamlText = yamlText == null ? "" : yamlText;
    }
  }

  record ParamLoadResponse(
      long requestId, String nodeName, boolean success, String reason, long paramsLoaded)
      implements BridgeEvent {
    public ParamLoadResponse {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      reason = reason == null ? "" : reason;
    }
  }

  // ── Actions ───────────────────────────────────────────────────────────────

  record ActionInfoResponse(
      long requestId,
      String actionName,
      String actionType,
      long clientCount,
      long serverCount,
      long feedbackPublisherCount,
      long feedbackSubscriberCount,
      long statusPublisherCount,
      long statusSubscriberCount)
      implements BridgeEvent {
    public ActionInfoResponse {
      Objects.requireNonNull(actionName, "actionName must not be null");
      actionType = actionType == null ? "" : actionType;
    }

    public boolean hasActionType() {
      return !actionType.isBlank();
    }
  }

  record ActionFeedback(
      long requestId,
      String actionName,
      String actionType,
      byte[] feedbackPayload,
      String feedbackText)
      implements BridgeEvent {
    public ActionFeedback {
      Objects.requireNonNull(actionName, "actionName must not be null");
      Objects.requireNonNull(actionType, "actionType must not be null");
      feedbackPayload = feedbackPayload == null
          ? new byte[0]
          : Arrays.copyOf(feedbackPayload, feedbackPayload.length);
      feedbackText = feedbackText == null ? "" : feedbackText;
    }

    public int payloadLength() {
      return feedbackPayload.length;
    }
  }

  record ActionResult(
      long requestId,
      String actionName,
      String actionType,
      boolean success,
      byte[] resultPayload,
      String resultText)
      implements BridgeEvent {
    public ActionResult {
      Objects.requireNonNull(actionName, "actionName must not be null");
      Objects.requireNonNull(actionType, "actionType must not be null");
      resultPayload =
          resultPayload == null ? new byte[0] : Arrays.copyOf(resultPayload, resultPayload.length);
      resultText = resultText == null ? "" : resultText;
    }

    public int payloadLength() {
      return resultPayload.length;
    }
  }

  // ── Players ───────────────────────────────────────────────────────────────

  record PlayerList(long requestId, List<Player> players) implements BridgeEvent {
    public PlayerList {
      Objects.requireNonNull(players, "players must not be null");
      players = List.copyOf(players);
    }

    public int size() {
      return players.size();
    }

    public boolean isEmpty() {
      return players.isEmpty();
    }
  }

  // ── Addon events ──────────────────────────────────────────────────────────

  record AddonEvent(
      long requestId,
      String addonId,
      String eventType,
      String encoding,
      byte[] payload,
      boolean response)
      implements BridgeEvent {
    public AddonEvent {
      Objects.requireNonNull(addonId, "addonId must not be null");
      Objects.requireNonNull(eventType, "eventType must not be null");
      encoding = encoding != null ? encoding : "";
      payload = payload != null ? Arrays.copyOf(payload, payload.length) : new byte[0];
    }

    public int payloadLength() {
      return payload.length;
    }
  }

  // ── Errors ────────────────────────────────────────────────────────────────

  record BridgeError(long requestId, String errorCode, String errorMessage)
      implements BridgeEvent {}
}
