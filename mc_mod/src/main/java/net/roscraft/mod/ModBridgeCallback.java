package net.roscraft.mod;

import java.util.List;
import java.util.UUID;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.BridgeCallback;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;

final class ModBridgeCallback implements BridgeCallback {

  private static final int MAX_GRAPH_ITEMS_TO_SHOW = 8;
  private static final int MAX_COMMAND_ITEMS_TO_SHOW = 64;
  private static final int MAX_PLAYERS_TO_SHOW = 10;

  private final RoscraftMod mod;

  ModBridgeCallback(RoscraftMod mod) {
    this.mod = mod;
  }

  @Override
  public void onEvent(BridgeEvent event) {
    mod.addonManager().onEvent(event);

    switch (event) {
      case BridgeEvent.GraphSnapshot e -> handleGraphSnapshot(e);
      case BridgeEvent.NodeInfoResponse e -> handleNodeInfoResponse(e);
      case BridgeEvent.TopicPayload e -> handleTopicPayload(e);
      case BridgeEvent.PlayerList e -> handlePlayerList(e);
      case BridgeEvent.TopicInfoResponse e -> handleTopicInfoResponse(e);
      case BridgeEvent.ServiceInfoResponse e -> handleServiceInfoResponse(e);
      case BridgeEvent.InterfaceListResponse e -> handleInterfaceListResponse(e);
      case BridgeEvent.InterfaceShowResponse e -> handleInterfaceShowResponse(e);
      case BridgeEvent.TopicHzResponse e -> handleTopicHzResponse(e);
      case BridgeEvent.TopicBwResponse e -> handleTopicBwResponse(e);
      case BridgeEvent.TopicDelayResponse e -> handleTopicDelayResponse(e);
      case BridgeEvent.ServiceCallResponse e -> handleServiceCallResponse(e);
      case BridgeEvent.ParamListResponse e -> handleParamListResponse(e);
      case BridgeEvent.ParamGetResponse e -> handleParamGetResponse(e);
      case BridgeEvent.ParamSetResponse e -> handleParamSetResponse(e);
      case BridgeEvent.ParamDescribeResponse e -> handleParamDescribeResponse(e);
      case BridgeEvent.ParamDumpResponse e -> handleParamDumpResponse(e);
      case BridgeEvent.ParamLoadResponse e -> handleParamLoadResponse(e);
      case BridgeEvent.ActionInfoResponse e -> handleActionInfoResponse(e);
      case BridgeEvent.ActionFeedback e -> handleActionFeedback(e);
      case BridgeEvent.ActionResult e -> handleActionResult(e);
      case BridgeEvent.AddonEvent e -> handleAddonEvent(e);
      case BridgeEvent.BridgeError e -> handleError(e);
      default -> {}
    }
  }

  private void handleGraphSnapshot(BridgeEvent.GraphSnapshot snapshot) {
    RoscraftMod.LOGGER.debug(
        "Graph snapshot received: {} nodes, {} topics, {} services, {} actions",
        snapshot.nodes().size(),
        snapshot.topics().size(),
        snapshot.services().size(),
        snapshot.actions().size());

    RoscraftMod.PendingRequest pending = mod.completeRequest(snapshot.requestId());
    if (pending == null) {
      sendGraphSnapshotPreview(snapshot, null);
      return;
    }

    UUID requesterUuid = pending.requesterUuid();
    switch (pending.kind()) {
      case CONNECTION_CHECK -> {
        mod.sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
                .append(
                    Text.literal(String.valueOf(snapshot.requestId())).formatted(Formatting.YELLOW))
                .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                .append(
                    Text.literal("Bridge replied with nodes=" + snapshot.nodes().size()
                            + ", topics="
                            + snapshot.topics().size()
                            + ", services="
                            + snapshot.services().size()
                            + ", actions="
                            + snapshot.actions().size())
                        .formatted(Formatting.GRAY)));
      }
      case NODE_LIST -> sendNodeList(snapshot, requesterUuid, pending.metadata());
      case NODE_INFO, INTERFACE_LIST -> {
        return;
      }
      case TOPIC_LIST -> sendTopicList(snapshot, requesterUuid, pending.metadata());
      case TOPIC_TYPE -> sendTopicType(snapshot, requesterUuid, pending.metadata());
      case TOPIC_FIND -> sendTopicFind(snapshot, requesterUuid, pending.metadata());
      case TOPIC_ECHO, TOPIC_ECHO_STOP -> {
        return;
      }
      case PLAYERS,
          TOPIC_INFO,
          SERVICE_INFO,
          INTERFACE_SHOW,
          TOPIC_HZ,
          TOPIC_HZ_STOP,
          TOPIC_BW,
          TOPIC_BW_STOP,
          TOPIC_DELAY,
          TOPIC_DELAY_STOP -> {
        return;
      }
    }
  }

  // ── Node info ──────────────────────────────────────────────────────

  private void handleNodeInfoResponse(BridgeEvent.NodeInfoResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() != PendingRequestKind.NODE_INFO) {
      return;
    }

    if (!response.found()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Node not found: ").formatted(Formatting.RED))
              .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Node info for ").formatted(Formatting.GOLD))
            .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));

    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Publishers",
        response.publishers(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.AQUA);
    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Subscribers",
        response.subscribers(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.GREEN);
    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Services",
        response.services(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.BLUE);
  }

  // ── Interface list ─────────────────────────────────────────────────

  private void handleInterfaceListResponse(BridgeEvent.InterfaceListResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() != PendingRequestKind.INTERFACE_LIST) {
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Interface list #").formatted(Formatting.GOLD))
            .append(
                Text.literal(String.valueOf(response.requestId())).formatted(Formatting.YELLOW)));

    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Messages", response.messages(), Formatting.GRAY);
    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Services", response.services(), Formatting.GRAY);
    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Actions", response.actions(), Formatting.GRAY);
  }

  // ── Graph snapshot formatting helpers ──────────────────────────────

  private void sendGraphSnapshotPreview(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid) {
    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Graph reply #" + snapshot.requestId() + " ")
                .formatted(Formatting.GOLD))
            .append(Text.literal("nodes=" + snapshot.nodes().size()
                    + ", topics="
                    + snapshot.topics().size()
                    + ", services="
                    + snapshot.services().size()
                    + ", actions="
                    + snapshot.actions().size())
                .formatted(Formatting.GREEN)));

    sendListPreviewToRequesterOrOperators(
        requesterUuid,
        "Nodes",
        snapshot.nodes().stream().map(BridgeEvent.NodeEntry::name).toList(),
        Formatting.LIGHT_PURPLE);
    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Topics",
        snapshot.topics(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.AQUA);
    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Services",
        snapshot.services(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.BLUE);
    sendEntryListPreviewToRequesterOrOperators(
        requesterUuid,
        "Actions",
        snapshot.actions(),
        entry -> entry.name() + " [" + entry.type() + "]",
        Formatting.DARK_AQUA);
  }

  private void sendNodeList(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");
    boolean countOnly = metadataFlagEnabled(metadata, "count_only");

    List<String> names = snapshot.nodes().stream()
        .map(BridgeEvent.NodeEntry::name)
        .filter(name -> includeHidden || !isHiddenName(name))
        .sorted()
        .toList();

    if (countOnly) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Node count: ").formatted(Formatting.GOLD))
              .append(Text.literal(String.valueOf(names.size())).formatted(Formatting.GREEN)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Nodes [" + names.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.LIGHT_PURPLE);
  }

  private void sendTopicList(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = metadataFlagEnabled(metadata, "show_types");
    boolean countOnly = metadataFlagEnabled(metadata, "count_only");
    boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");

    List<String> values;
    if (showTypes) {
      values = snapshot.topics().stream()
          .filter(entry -> includeHidden || !isHiddenName(entry.name()))
          .map(entry -> entry.name() + " [" + entry.type() + "]")
          .sorted()
          .toList();
    } else {
      values = snapshot.topics().stream()
          .map(BridgeEvent.TopicEntry::name)
          .filter(name -> includeHidden || !isHiddenName(name))
          .distinct()
          .sorted()
          .toList();
    }

    if (countOnly) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Topic count: ").formatted(Formatting.GOLD))
              .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topics [" + values.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.AQUA);
  }

  private void sendTopicType(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String topicName) {
    if (topicName == null || topicName.isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Topic name is missing.").formatted(Formatting.RED)));
      return;
    }

    List<String> types = snapshot.topics().stream()
        .filter(entry -> entry.name().equals(topicName))
        .map(BridgeEvent.TopicEntry::type)
        .filter(type -> !type.isBlank())
        .distinct()
        .sorted()
        .toList();

    if (types.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Topic not found: ").formatted(Formatting.RED))
              .append(Text.literal(topicName).formatted(Formatting.YELLOW)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topic type for ").formatted(Formatting.GOLD))
            .append(Text.literal(topicName).formatted(Formatting.YELLOW))
            .append(Text.literal(":").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
  }

  private void sendTopicFind(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String topicType) {
    if (topicType == null || topicType.isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Topic type is missing.").formatted(Formatting.RED)));
      return;
    }

    List<String> names = snapshot.topics().stream()
        .filter(entry -> entry.type().equals(topicType))
        .map(BridgeEvent.TopicEntry::name)
        .distinct()
        .sorted()
        .toList();

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topics of type ").formatted(Formatting.GOLD))
            .append(Text.literal(topicType).formatted(Formatting.YELLOW))
            .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.AQUA);
  }

  private void sendServiceList(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = metadataFlagEnabled(metadata, "show_types");
    boolean countOnly = metadataFlagEnabled(metadata, "count_only");
    boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");

    List<String> values;
    if (showTypes) {
      values = snapshot.services().stream()
          .filter(entry -> includeHidden || !isHiddenName(entry.name()))
          .map(entry -> entry.name() + " [" + entry.type() + "]")
          .sorted()
          .toList();
    } else {
      values = snapshot.services().stream()
          .map(BridgeEvent.ServiceEntry::name)
          .filter(name -> includeHidden || !isHiddenName(name))
          .distinct()
          .sorted()
          .toList();
    }

    if (countOnly) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Service count: ").formatted(Formatting.GOLD))
              .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Services [" + values.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.BLUE);
  }

  private void sendServiceType(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String serviceName) {
    if (serviceName == null || serviceName.isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Service name is missing.").formatted(Formatting.RED)));
      return;
    }

    List<String> types = snapshot.services().stream()
        .filter(entry -> entry.name().equals(serviceName))
        .map(BridgeEvent.ServiceEntry::type)
        .filter(type -> !type.isBlank())
        .distinct()
        .sorted()
        .toList();

    if (types.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Service not found: ").formatted(Formatting.RED))
              .append(Text.literal(serviceName).formatted(Formatting.YELLOW)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Service type for ").formatted(Formatting.GOLD))
            .append(Text.literal(serviceName).formatted(Formatting.YELLOW))
            .append(Text.literal(":").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
  }

  private void sendServiceFind(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String serviceType) {
    if (serviceType == null || serviceType.isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Service type is missing.").formatted(Formatting.RED)));
      return;
    }

    List<String> names = snapshot.services().stream()
        .filter(entry -> entry.type().equals(serviceType))
        .map(BridgeEvent.ServiceEntry::name)
        .distinct()
        .sorted()
        .toList();

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Services of type ").formatted(Formatting.GOLD))
            .append(Text.literal(serviceType).formatted(Formatting.YELLOW))
            .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.BLUE);
  }

  private void sendActionList(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = metadataFlagEnabled(metadata, "show_types");

    List<String> values;
    if (showTypes) {
      values = snapshot.actions().stream()
          .map(entry -> entry.name() + " [" + entry.type() + "]")
          .sorted()
          .toList();
    } else {
      values = snapshot.actions().stream()
          .map(BridgeEvent.ActionEntry::name)
          .distinct()
          .sorted()
          .toList();
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Actions [" + values.size() + "]").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.DARK_AQUA);
  }

  private void sendActionType(
      BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String actionName) {
    if (actionName == null || actionName.isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Action name is missing.").formatted(Formatting.RED)));
      return;
    }

    List<String> types = snapshot.actions().stream()
        .filter(entry -> entry.name().equals(actionName))
        .map(BridgeEvent.ActionEntry::type)
        .filter(type -> !type.isBlank())
        .distinct()
        .sorted()
        .toList();

    if (types.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Action not found: ").formatted(Formatting.RED))
              .append(Text.literal(actionName).formatted(Formatting.YELLOW)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Action type for ").formatted(Formatting.GOLD))
            .append(Text.literal(actionName).formatted(Formatting.YELLOW))
            .append(Text.literal(":").formatted(Formatting.GOLD)));
    sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
  }

  // ── Topic payload ──────────────────────────────────────────────────

  private void handleTopicPayload(BridgeEvent.TopicPayload payload) {
    RoscraftMod.LOGGER.trace(
        "Topic payload: {} ({} bytes, requestId={}, raw={})",
        payload.topicName(),
        payload.payloadLength(),
        payload.requestId(),
        payload.raw());

    RoscraftMod.PendingRequest pending = mod.pendingRequest(payload.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.TOPIC_ECHO) {
      mod.completeRequest(payload.requestId());
      return;
    }

    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topic echo ").formatted(Formatting.GOLD))
            .append(Text.literal(payload.topicName()).formatted(Formatting.YELLOW))
            .append(Text.literal(" (" + payload.messageType() + ") ").formatted(Formatting.GRAY))
            .append(Text.literal("bytes=").formatted(Formatting.DARK_GRAY))
            .append(
                Text.literal(String.valueOf(payload.payloadLength())).formatted(Formatting.GREEN))
            .append(Text.literal(" raw=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(String.valueOf(payload.raw())).formatted(Formatting.AQUA)));

    if (pending != null && metadataFlagEnabled(pending.metadata(), "once")) {
      mod.completeRequest(payload.requestId());
    }
  }

  // ── Player list ────────────────────────────────────────────────────

  private void handlePlayerList(BridgeEvent.PlayerList playerList) {
    RoscraftMod.LOGGER.debug("Player list received: {} players", playerList.size());

    RoscraftMod.PendingRequest pending = mod.completeRequest(playerList.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() == PendingRequestKind.CONNECTION_CHECK) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
              .append(
                  Text.literal(String.valueOf(playerList.requestId())).formatted(Formatting.YELLOW))
              .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
              .append(Text.literal("Bridge replied with " + playerList.size() + " players.")
                  .formatted(Formatting.GRAY)));
      return;
    }

    if (pending != null && pending.kind() != PendingRequestKind.PLAYERS) {
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Player list reply #" + playerList.requestId() + " ")
                .formatted(Formatting.GOLD))
            .append(Text.literal("count=" + playerList.size()).formatted(Formatting.GREEN)));

    int count = Math.min(playerList.players().size(), MAX_PLAYERS_TO_SHOW);
    for (int i = 0; i < count; i++) {
      BridgeEvent.Player player = playerList.players().get(i);
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - ")
              .formatted(Formatting.DARK_GRAY)
              .append(Text.literal(player.name()).formatted(Formatting.YELLOW))
              .append(Text.literal(
                      String.format(" (%.1f, %.1f, %.1f)", player.x(), player.y(), player.z()))
                  .formatted(Formatting.GRAY)));
    }

    if (playerList.players().size() > MAX_PLAYERS_TO_SHOW) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(
                  " - ... and " + (playerList.players().size() - MAX_PLAYERS_TO_SHOW) + " more")
              .formatted(Formatting.GRAY));
    }
  }

  // ── Topic info ─────────────────────────────────────────────────────

  private void handleTopicInfoResponse(BridgeEvent.TopicInfoResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean verbose = pending != null && metadataFlagEnabled(pending.metadata(), "verbose");

    if (pending != null && pending.kind() != PendingRequestKind.TOPIC_INFO) {
      return;
    }

    MutableText line = RoscraftMod.prefix()
        .append(
            Text.literal("Topic info #" + response.requestId() + " ").formatted(Formatting.GOLD))
        .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW));
    if (response.hasMessageType()) {
      line.append(Text.literal(" [" + response.messageType() + "]").formatted(Formatting.AQUA));
    } else {
      line.append(Text.literal(" [unknown]").formatted(Formatting.GRAY));
    }
    line.append(Text.literal(" publishers=" + response.publisherCount()
            + ", subscribers="
            + response.subscriberCount())
        .formatted(Formatting.GREEN));
    mod.sendToRequesterOrOperators(requesterUuid, line);

    if (!verbose) {
      return;
    }

    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Publisher nodes", response.publisherNodes(), Formatting.GRAY);
    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Subscriber nodes", response.subscriberNodes(), Formatting.GRAY);
  }

  // ── Service info ───────────────────────────────────────────────────

  private void handleServiceInfoResponse(BridgeEvent.ServiceInfoResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean verbose = pending != null && metadataFlagEnabled(pending.metadata(), "verbose");

    if (pending != null && pending.kind() != PendingRequestKind.SERVICE_INFO) {
      return;
    }

    MutableText line = RoscraftMod.prefix()
        .append(
            Text.literal("Service info #" + response.requestId() + " ").formatted(Formatting.GOLD))
        .append(Text.literal(response.serviceName()).formatted(Formatting.YELLOW));
    if (response.hasServiceType()) {
      line.append(Text.literal(" [" + response.serviceType() + "]").formatted(Formatting.AQUA));
    } else {
      line.append(Text.literal(" [unknown]").formatted(Formatting.GRAY));
    }
    line.append(
        Text.literal(" clients=" + response.clientCount() + ", servers=" + response.serverCount())
            .formatted(Formatting.GREEN));
    mod.sendToRequesterOrOperators(requesterUuid, line);

    if (!verbose) {
      return;
    }

    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Client nodes", response.clientNodes(), Formatting.GRAY);
    sendListPreviewToRequesterOrOperators(
        requesterUuid, "Server nodes", response.serverNodes(), Formatting.GRAY);
  }

  // ── Interface show ─────────────────────────────────────────────────

  private void handleInterfaceShowResponse(BridgeEvent.InterfaceShowResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean noComments = pending != null && metadataFlagEnabled(pending.metadata(), "no_comments");

    if (pending != null && pending.kind() != PendingRequestKind.INTERFACE_SHOW) {
      return;
    }

    if (!response.found()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Interface " + response.interfaceType() + " not found")
                  .formatted(Formatting.RED)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Interface ").formatted(Formatting.GOLD))
            .append(Text.literal(response.interfaceType()).formatted(Formatting.YELLOW))
            .append(Text.literal(":").formatted(Formatting.GOLD)));

    for (String line : response.definition().split("\\R", -1)) {
      if (noComments && line.stripLeading().startsWith("#")) {
        continue;
      }
      if (line.isEmpty()) {
        continue;
      }
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
    }
  }

  // ── Error ──────────────────────────────────────────────────────────

  private void handleError(BridgeEvent.BridgeError error) {
    RoscraftMod.LOGGER.error("Bridge error: [{}] {}", error.errorCode(), error.errorMessage());

    RoscraftMod.PendingRequest pending = mod.completeRequest(error.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String userMessage =
        switch (error.errorCode()) {
          case "SUBSCRIBE_FAILED" -> "Failed to subscribe: " + error.errorMessage();
          case "SUBSCRIBE_TIMEOUT" -> "Topic echo timed out: " + error.errorMessage();
          case "PUBLISH_FAILED" -> "Failed to publish: " + error.errorMessage();
          default -> "Error: " + error.errorMessage();
        };
    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("[").formatted(Formatting.DARK_RED))
            .append(Text.literal(error.errorCode()).formatted(Formatting.RED))
            .append(Text.literal("] ").formatted(Formatting.DARK_RED))
            .append(Text.literal(userMessage).formatted(Formatting.RED)));
  }

  // ── Topic Hz ───────────────────────────────────────────────────────

  private void handleTopicHzResponse(BridgeEvent.TopicHzResponse response) {
    RoscraftMod.PendingRequest pending = mod.pendingRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.TOPIC_HZ) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String formattedFreq = String.format("%.2f", response.frequency());
    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topic hz ").formatted(Formatting.GOLD))
            .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
            .append(Text.literal(" average rate: ").formatted(Formatting.GOLD))
            .append(Text.literal(formattedFreq).formatted(Formatting.GREEN))
            .append(Text.literal(" Hz").formatted(Formatting.GREEN))
            .append(
                Text.literal("  min: 0.00 Hz  max: " + String.format("%.2f", response.frequency())
                        + " Hz"
                        + "  window: "
                        + response.window()
                        + "  samples: "
                        + response.messageCount())
                    .formatted(Formatting.GRAY)));
  }

  // ── Topic Bw ───────────────────────────────────────────────────────

  private void handleTopicBwResponse(BridgeEvent.TopicBwResponse response) {
    RoscraftMod.PendingRequest pending = mod.pendingRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.TOPIC_BW) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String formattedBw = String.format("%.2f", response.bytesPerSecond());
    String formattedTotal = String.format("%.2f KB", response.totalBytes() / 1024.0);
    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topic bw ").formatted(Formatting.GOLD))
            .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
            .append(Text.literal(" average: ").formatted(Formatting.GOLD))
            .append(Text.literal(formattedBw + " B/s").formatted(Formatting.GREEN))
            .append(Text.literal("  total: " + formattedTotal
                    + "  window: "
                    + response.window()
                    + "  messages: "
                    + response.messageCount())
                .formatted(Formatting.GRAY)));
  }

  // ── Topic delay ────────────────────────────────────────────────────

  private void handleTopicDelayResponse(BridgeEvent.TopicDelayResponse response) {
    RoscraftMod.PendingRequest pending = mod.pendingRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.TOPIC_DELAY) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Topic delay ").formatted(Formatting.GOLD))
            .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
            .append(Text.literal(" average: ").formatted(Formatting.GOLD))
            .append(Text.literal(String.format("%.4f s", response.averageDelay()))
                .formatted(Formatting.GREEN))
            .append(Text.literal("  min: ").formatted(Formatting.GRAY))
            .append(Text.literal(String.format("%.4f s", response.minDelay()))
                .formatted(Formatting.GRAY))
            .append(Text.literal("  max: ").formatted(Formatting.GRAY))
            .append(Text.literal(String.format("%.4f s", response.maxDelay()))
                .formatted(Formatting.GRAY))
            .append(Text.literal(
                    "  window: " + response.window() + "  samples: " + response.messageCount())
                .formatted(Formatting.GRAY)));
  }

  // ── Service call ───────────────────────────────────────────────────

  private void handleServiceCallResponse(BridgeEvent.ServiceCallResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.SERVICE_CALL) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(
            Text.literal("Service call #" + response.requestId() + " ").formatted(Formatting.GOLD))
        .append(Text.literal(response.serviceName()).formatted(Formatting.YELLOW))
        .append(Text.literal(" [" + response.serviceType() + "] ").formatted(Formatting.AQUA))
        .append(Text.literal(response.success() ? "succeeded" : "failed")
            .formatted(response.success() ? Formatting.GREEN : Formatting.RED));
    if (!response.resultText().isBlank()) {
      line.append(Text.literal(" - " + response.resultText()).formatted(Formatting.GRAY));
    }
    line.append(Text.literal(" (payload=" + response.payloadLength() + " bytes)")
        .formatted(Formatting.DARK_GRAY));
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Param list ─────────────────────────────────────────────────────

  private void handleParamListResponse(BridgeEvent.ParamListResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_LIST) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean includeTypes =
        pending != null && metadataFlagEnabled(pending.metadata(), "include_types");

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Param list #" + response.requestId() + " ")
                .formatted(Formatting.GOLD))
            .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW))
            .append(Text.literal(" names=" + response.names().size()).formatted(Formatting.GREEN))
            .append(Text.literal(" prefixes=" + response.prefixes().size())
                .formatted(Formatting.GRAY)));

    if (response.names().isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(response.names().size(), MAX_COMMAND_ITEMS_TO_SHOW);
    for (int i = 0; i < count; i++) {
      String name = response.names().get(i);
      String type = i < response.types().size() ? response.types().get(i) : "";
      String line = includeTypes && !type.isBlank() ? name + " [" + type + "]" : name;
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
    }
    if (response.names().size() > MAX_COMMAND_ITEMS_TO_SHOW) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(
                  "  ... and " + (response.names().size() - MAX_COMMAND_ITEMS_TO_SHOW) + " more")
              .formatted(Formatting.GRAY));
    }
  }

  // ── Param get ──────────────────────────────────────────────────────

  private void handleParamGetResponse(BridgeEvent.ParamGetResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_GET) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (!response.found()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Param not found: ").formatted(Formatting.RED))
              .append(Text.literal(response.nodeName() + "/" + response.paramName())
                  .formatted(Formatting.YELLOW)));
      return;
    }

    MutableText line = RoscraftMod.prefix()
        .append(Text.literal("Param get ").formatted(Formatting.GOLD))
        .append(Text.literal(response.nodeName() + "/" + response.paramName())
            .formatted(Formatting.YELLOW))
        .append(Text.literal(" = ").formatted(Formatting.GOLD))
        .append(Text.literal(response.valueText()).formatted(Formatting.GREEN));
    if (!response.typeHidden() && !response.paramType().isBlank()) {
      line.append(Text.literal(" [" + response.paramType() + "]").formatted(Formatting.AQUA));
    }
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Param set ──────────────────────────────────────────────────────

  private void handleParamSetResponse(BridgeEvent.ParamSetResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_SET) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(Text.literal("Param set ").formatted(Formatting.GOLD))
        .append(Text.literal(response.nodeName() + "/" + response.paramName())
            .formatted(Formatting.YELLOW))
        .append(Text.literal(response.success() ? " succeeded" : " failed")
            .formatted(response.success() ? Formatting.GREEN : Formatting.RED));
    if (!response.valueText().isBlank()) {
      line.append(Text.literal(" value=" + response.valueText()).formatted(Formatting.GRAY));
    }
    if (!response.paramType().isBlank()) {
      line.append(Text.literal(" [" + response.paramType() + "]").formatted(Formatting.AQUA));
    }
    if (!response.reason().isBlank()) {
      line.append(Text.literal(" - " + response.reason()).formatted(Formatting.GRAY));
    }
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Param describe ─────────────────────────────────────────────────

  private void handleParamDescribeResponse(BridgeEvent.ParamDescribeResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_DESCRIBE) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (!response.found()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          RoscraftMod.prefix()
              .append(Text.literal("Param not found: ").formatted(Formatting.RED))
              .append(Text.literal(response.nodeName() + "/" + response.paramName())
                  .formatted(Formatting.YELLOW)));
      return;
    }

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Param describe ").formatted(Formatting.GOLD))
            .append(Text.literal(response.nodeName() + "/" + response.paramName())
                .formatted(Formatting.YELLOW))
            .append(Text.literal(" [" + response.paramType() + "]").formatted(Formatting.AQUA))
            .append(Text.literal(" read_only=" + response.readOnly()).formatted(Formatting.GRAY)));
    if (!response.description().isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  description: " + response.description()).formatted(Formatting.GRAY));
    }
    if (!response.constraints().isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  constraints: " + response.constraints()).formatted(Formatting.GRAY));
    }
  }

  // ── Param dump ─────────────────────────────────────────────────────

  private void handleParamDumpResponse(BridgeEvent.ParamDumpResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_DUMP) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    mod.sendToRequesterOrOperators(
        requesterUuid,
        RoscraftMod.prefix()
            .append(Text.literal("Param dump #" + response.requestId() + " ")
                .formatted(Formatting.GOLD))
            .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));

    if (response.yamlText().isBlank()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  (empty)").formatted(Formatting.DARK_GRAY));
      return;
    }

    String[] lines = response.yamlText().split("\\R", -1);
    int shown = 0;
    for (String line : lines) {
      if (line.isEmpty()) {
        continue;
      }
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
      shown++;
      if (shown >= MAX_COMMAND_ITEMS_TO_SHOW) {
        break;
      }
    }
    if (lines.length > shown) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  ... truncated").formatted(Formatting.DARK_GRAY));
    }
  }

  // ── Param load ─────────────────────────────────────────────────────

  private void handleParamLoadResponse(BridgeEvent.ParamLoadResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.PARAM_LOAD) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(Text.literal("Param load ").formatted(Formatting.GOLD))
        .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW))
        .append(Text.literal(response.success() ? " succeeded" : " failed")
            .formatted(response.success() ? Formatting.GREEN : Formatting.RED))
        .append(
            Text.literal(" params_loaded=" + response.paramsLoaded()).formatted(Formatting.GRAY));
    if (!response.reason().isBlank()) {
      line.append(Text.literal(" - " + response.reason()).formatted(Formatting.GRAY));
    }
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Action info ────────────────────────────────────────────────────

  private void handleActionInfoResponse(BridgeEvent.ActionInfoResponse response) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(response.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.ACTION_INFO) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(
            Text.literal("Action info #" + response.requestId() + " ").formatted(Formatting.GOLD))
        .append(Text.literal(response.actionName()).formatted(Formatting.YELLOW));
    if (response.hasActionType()) {
      line.append(Text.literal(" [" + response.actionType() + "]").formatted(Formatting.AQUA));
    }
    line.append(
        Text.literal(" clients=" + response.clientCount() + ", servers=" + response.serverCount())
            .formatted(Formatting.GREEN));
    mod.sendToRequesterOrOperators(requesterUuid, line);

    mod.sendToRequesterOrOperators(
        requesterUuid,
        Text.literal("  feedback pubs/subs="
                + response.feedbackPublisherCount()
                + "/"
                + response.feedbackSubscriberCount()
                + ", status pubs/subs="
                + response.statusPublisherCount()
                + "/"
                + response.statusSubscriberCount())
            .formatted(Formatting.GRAY));
  }

  // ── Action feedback ────────────────────────────────────────────────

  private void handleActionFeedback(BridgeEvent.ActionFeedback feedback) {
    RoscraftMod.PendingRequest pending = mod.pendingRequest(feedback.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.ACTION_SEND_GOAL) {
      mod.completeRequest(feedback.requestId());
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(Text.literal("Action feedback ").formatted(Formatting.GOLD))
        .append(Text.literal(feedback.actionName()).formatted(Formatting.YELLOW))
        .append(Text.literal(" [" + feedback.actionType() + "] ").formatted(Formatting.AQUA));
    if (!feedback.feedbackText().isBlank()) {
      line.append(Text.literal(feedback.feedbackText()).formatted(Formatting.GRAY));
    } else {
      line.append(Text.literal("payload=" + feedback.payloadLength() + " bytes")
          .formatted(Formatting.GRAY));
    }
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Action result ──────────────────────────────────────────────────

  private void handleActionResult(BridgeEvent.ActionResult result) {
    RoscraftMod.PendingRequest pending = mod.completeRequest(result.requestId());
    if (pending != null && pending.kind() != PendingRequestKind.ACTION_SEND_GOAL) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    MutableText line = RoscraftMod.prefix()
        .append(Text.literal("Action result ").formatted(Formatting.GOLD))
        .append(Text.literal(result.actionName()).formatted(Formatting.YELLOW))
        .append(Text.literal(" [" + result.actionType() + "] ").formatted(Formatting.AQUA))
        .append(Text.literal(result.success() ? "succeeded" : "failed")
            .formatted(result.success() ? Formatting.GREEN : Formatting.RED));
    if (!result.resultText().isBlank()) {
      line.append(Text.literal(" - " + result.resultText()).formatted(Formatting.GRAY));
    }
    line.append(Text.literal(" (payload=" + result.payloadLength() + " bytes)")
        .formatted(Formatting.DARK_GRAY));
    mod.sendToRequesterOrOperators(requesterUuid, line);
  }

  // ── Addon event ────────────────────────────────────────────────────

  private void handleAddonEvent(BridgeEvent.AddonEvent event) {
    RoscraftMod.LOGGER.debug(
        "Addon event: addonId={} eventType={} requestId={} response={}",
        event.addonId(),
        event.eventType(),
        event.requestId(),
        event.response());
  }

  // ── Display helpers ────────────────────────────────────────────────

  private void sendValuesToRequesterOrOperators(
      UUID requesterUuid, List<String> values, Formatting color) {
    if (values.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(values.size(), MAX_COMMAND_ITEMS_TO_SHOW);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  " + values.get(i)).formatted(color));
    }

    if (values.size() > MAX_COMMAND_ITEMS_TO_SHOW) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  ... and " + (values.size() - MAX_COMMAND_ITEMS_TO_SHOW) + " more")
              .formatted(Formatting.GRAY));
    }
  }

  private boolean metadataFlagEnabled(String metadata, String key) {
    String value = metadataValue(metadata, key);
    return "1".equals(value) || "true".equalsIgnoreCase(value);
  }

  private String metadataValue(String metadata, String key) {
    if (metadata == null || metadata.isBlank()) {
      return null;
    }

    for (String token : metadata.split(";")) {
      String trimmed = token.trim();
      int separatorIndex = trimmed.indexOf('=');
      if (separatorIndex <= 0 || separatorIndex + 1 >= trimmed.length()) {
        continue;
      }
      if (!trimmed.substring(0, separatorIndex).equals(key)) {
        continue;
      }
      return trimmed.substring(separatorIndex + 1);
    }

    return null;
  }

  private boolean isHiddenName(String name) {
    for (String segment : name.split("/")) {
      if (!segment.isEmpty() && segment.startsWith("_")) {
        return true;
      }
    }
    return false;
  }

  private void sendListPreviewToRequesterOrOperators(
      UUID requesterUuid, String label, List<String> values, Formatting color) {
    if (values.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(values.size(), MAX_GRAPH_ITEMS_TO_SHOW);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label + ": ")
              .formatted(Formatting.DARK_GRAY)
              .append(Text.literal(values.get(i)).formatted(color)));
    }

    if (values.size() > MAX_GRAPH_ITEMS_TO_SHOW) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label
                  + ": ... and "
                  + (values.size() - MAX_GRAPH_ITEMS_TO_SHOW)
                  + " more")
              .formatted(Formatting.GRAY));
    }
  }

  private <T> void sendEntryListPreviewToRequesterOrOperators(
      UUID requesterUuid,
      String label,
      List<T> entries,
      java.util.function.Function<T, String> formatter,
      Formatting color) {
    if (entries.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(entries.size(), MAX_GRAPH_ITEMS_TO_SHOW);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label + ": ")
              .formatted(Formatting.DARK_GRAY)
              .append(Text.literal(formatter.apply(entries.get(i))).formatted(color)));
    }

    if (entries.size() > MAX_GRAPH_ITEMS_TO_SHOW) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label
                  + ": ... and "
                  + (entries.size() - MAX_GRAPH_ITEMS_TO_SHOW)
                  + " more")
              .formatted(Formatting.GRAY));
    }
  }
}
