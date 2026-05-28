package net.roscraft.mod.bridge.callback;

import java.util.List;
import java.util.UUID;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class GraphCommandEvents {

  private final CommandEventContext ctx;

  GraphCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  void onGraphSnapshot(BridgeEvent.GraphSnapshot snapshot) {
    RoscraftMod.LOGGER.debug(
        "Graph snapshot received: {} nodes, {} topics, {} services, {} actions",
        snapshot.nodes().size(),
        snapshot.topics().size(),
        snapshot.services().size(),
        snapshot.actions().size());

    CommandPendingRequest pending = ctx.requests().complete(snapshot.requestId());
    if (pending == null) {
      sendGraphSnapshotPreview(snapshot, null);
      return;
    }

    UUID requesterUuid = pending.requesterUuid();
    switch (pending.kind()) {
      case CONNECTION_CHECK -> {
        ctx.mod()
            .sendToRequesterOrOperators(
                requesterUuid,
                RoscraftMod.prefix()
                    .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
                    .append(Text.literal(String.valueOf(snapshot.requestId()))
                        .formatted(Formatting.YELLOW))
                    .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                    .append(Text.literal(
                            "Bridge replied with nodes=" + snapshot.nodes().size()
                                + ", topics="
                                + snapshot.topics().size()
                                + ", services="
                                + snapshot.services().size()
                                + ", actions="
                                + snapshot.actions().size())
                        .formatted(Formatting.GRAY)));
      }
      case NODE_LIST -> sendNodeList(snapshot, requesterUuid, pending.metadata());
      case NODE_INFO, INTERFACE_LIST -> {}
      case TOPIC_LIST -> sendTopicList(snapshot, requesterUuid, pending.metadata());
      case TOPIC_TYPE -> sendTopicType(snapshot, requesterUuid, pending.metadata());
      case TOPIC_FIND -> sendTopicFind(snapshot, requesterUuid, pending.metadata());
      case SERVICE_LIST -> sendServiceList(snapshot, requesterUuid, pending.metadata());
      case SERVICE_TYPE -> sendServiceType(snapshot, requesterUuid, pending.metadata());
      case SERVICE_FIND -> sendServiceFind(snapshot, requesterUuid, pending.metadata());
      case ACTION_LIST -> sendActionList(snapshot, requesterUuid, pending.metadata());
      case ACTION_TYPE -> sendActionType(snapshot, requesterUuid, pending.metadata());
      case TOPIC_ECHO, TOPIC_ECHO_STOP -> {}
      case PLAYERS,
          TOPIC_INFO,
          SERVICE_INFO,
          INTERFACE_SHOW,
          TOPIC_HZ,
          TOPIC_HZ_STOP,
          TOPIC_BW,
          TOPIC_BW_STOP,
          TOPIC_DELAY,
          TOPIC_DELAY_STOP -> {}
    }
  }

  // ── Node info ──────────────────────────────────────────────────────

  void onNodeInfoResponse(BridgeEvent.NodeInfoResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() != CommandRequestKind.NODE_INFO) {
      return;
    }

    if (!response.found()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Node not found: ").formatted(Formatting.RED))
                  .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Node info for ").formatted(Formatting.GOLD))
                .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));

    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Publishers",
            response.publishers(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.AQUA);
    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Subscribers",
            response.subscribers(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.GREEN);
    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Services",
            response.services(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.BLUE);
  }
  // ── Interface list ─────────────────────────────────────────────────

  void onInterfaceListResponse(BridgeEvent.InterfaceListResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() != CommandRequestKind.INTERFACE_LIST) {
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Interface list #").formatted(Formatting.GOLD))
                .append(Text.literal(String.valueOf(response.requestId()))
                    .formatted(Formatting.YELLOW)));

    ctx.chat().sendListPreview(requesterUuid, "Messages", response.messages(), Formatting.GRAY);
    ctx.chat().sendListPreview(requesterUuid, "Services", response.services(), Formatting.GRAY);
    ctx.chat().sendListPreview(requesterUuid, "Actions", response.actions(), Formatting.GRAY);
  }
  // ── Graph snapshot formatting helpers ──────────────────────────────

  void sendGraphSnapshotPreview(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid) {
    ctx.mod()
        .sendToRequesterOrOperators(
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

    ctx.chat()
        .sendListPreview(
            requesterUuid,
            "Nodes",
            snapshot.nodes().stream().map(BridgeEvent.NodeEntry::name).toList(),
            Formatting.LIGHT_PURPLE);
    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Topics",
            snapshot.topics(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.AQUA);
    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Services",
            snapshot.services(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.BLUE);
    ctx.chat()
        .sendEntryListPreview(
            requesterUuid,
            "Actions",
            snapshot.actions(),
            entry -> entry.name() + " [" + entry.type() + "]",
            Formatting.DARK_AQUA);
  }

  void sendNodeList(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean includeHidden = ctx.chat().metadataFlagEnabled(metadata, "include_hidden");
    boolean countOnly = ctx.chat().metadataFlagEnabled(metadata, "count_only");

    List<String> names = snapshot.nodes().stream()
        .map(BridgeEvent.NodeEntry::name)
        .filter(name -> includeHidden || !ctx.chat().isHiddenName(name))
        .sorted()
        .toList();

    if (countOnly) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Node count: ").formatted(Formatting.GOLD))
                  .append(Text.literal(String.valueOf(names.size())).formatted(Formatting.GREEN)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Nodes [" + names.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, names, Formatting.LIGHT_PURPLE);
  }

  void sendTopicList(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = ctx.chat().metadataFlagEnabled(metadata, "show_types");
    boolean countOnly = ctx.chat().metadataFlagEnabled(metadata, "count_only");
    boolean includeHidden = ctx.chat().metadataFlagEnabled(metadata, "include_hidden");

    List<String> values;
    if (showTypes) {
      values = snapshot.topics().stream()
          .filter(entry -> includeHidden || !ctx.chat().isHiddenName(entry.name()))
          .map(entry -> entry.name() + " [" + entry.type() + "]")
          .sorted()
          .toList();
    } else {
      values = snapshot.topics().stream()
          .map(BridgeEvent.TopicEntry::name)
          .filter(name -> includeHidden || !ctx.chat().isHiddenName(name))
          .distinct()
          .sorted()
          .toList();
    }

    if (countOnly) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Topic count: ").formatted(Formatting.GOLD))
                  .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Topics [" + values.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, values, Formatting.AQUA);
  }

  void sendTopicType(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String topicName) {
    if (topicName == null || topicName.isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Topic not found: ").formatted(Formatting.RED))
                  .append(Text.literal(topicName).formatted(Formatting.YELLOW)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Topic type for ").formatted(Formatting.GOLD))
                .append(Text.literal(topicName).formatted(Formatting.YELLOW))
                .append(Text.literal(":").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, types, Formatting.AQUA);
  }

  void sendTopicFind(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String topicType) {
    if (topicType == null || topicType.isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Topics of type ").formatted(Formatting.GOLD))
                .append(Text.literal(topicType).formatted(Formatting.YELLOW))
                .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, names, Formatting.AQUA);
  }

  void sendServiceList(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = ctx.chat().metadataFlagEnabled(metadata, "show_types");
    boolean countOnly = ctx.chat().metadataFlagEnabled(metadata, "count_only");
    boolean includeHidden = ctx.chat().metadataFlagEnabled(metadata, "include_hidden");

    List<String> values;
    if (showTypes) {
      values = snapshot.services().stream()
          .filter(entry -> includeHidden || !ctx.chat().isHiddenName(entry.name()))
          .map(entry -> entry.name() + " [" + entry.type() + "]")
          .sorted()
          .toList();
    } else {
      values = snapshot.services().stream()
          .map(BridgeEvent.ServiceEntry::name)
          .filter(name -> includeHidden || !ctx.chat().isHiddenName(name))
          .distinct()
          .sorted()
          .toList();
    }

    if (countOnly) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Service count: ").formatted(Formatting.GOLD))
                  .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(
                    Text.literal("Services [" + values.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, values, Formatting.BLUE);
  }

  void sendServiceType(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String serviceName) {
    if (serviceName == null || serviceName.isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Service not found: ").formatted(Formatting.RED))
                  .append(Text.literal(serviceName).formatted(Formatting.YELLOW)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Service type for ").formatted(Formatting.GOLD))
                .append(Text.literal(serviceName).formatted(Formatting.YELLOW))
                .append(Text.literal(":").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, types, Formatting.AQUA);
  }

  void sendServiceFind(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String serviceType) {
    if (serviceType == null || serviceType.isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Services of type ").formatted(Formatting.GOLD))
                .append(Text.literal(serviceType).formatted(Formatting.YELLOW))
                .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, names, Formatting.BLUE);
  }

  void sendActionList(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
    boolean showTypes = ctx.chat().metadataFlagEnabled(metadata, "show_types");

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

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(
                    Text.literal("Actions [" + values.size() + "]").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, values, Formatting.DARK_AQUA);
  }

  void sendActionType(BridgeEvent.GraphSnapshot snapshot, UUID requesterUuid, String actionName) {
    if (actionName == null || actionName.isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Action not found: ").formatted(Formatting.RED))
                  .append(Text.literal(actionName).formatted(Formatting.YELLOW)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Action type for ").formatted(Formatting.GOLD))
                .append(Text.literal(actionName).formatted(Formatting.YELLOW))
                .append(Text.literal(":").formatted(Formatting.GOLD)));
    ctx.chat().sendValues(requesterUuid, types, Formatting.AQUA);
  }
  // ── Interface show ─────────────────────────────────────────────────

  void onInterfaceShowResponse(BridgeEvent.InterfaceShowResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean noComments =
        pending != null && ctx.chat().metadataFlagEnabled(pending.metadata(), "no_comments");

    if (pending != null && pending.kind() != CommandRequestKind.INTERFACE_SHOW) {
      return;
    }

    if (!response.found()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Interface " + response.interfaceType() + " not found")
                      .formatted(Formatting.RED)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
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
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
    }
  }
}
