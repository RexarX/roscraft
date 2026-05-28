package net.roscraft.mod.bridge.callback;

import java.util.UUID;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class ParamCommandEvents {

  private final CommandEventContext ctx;

  ParamCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  // ── Param list ─────────────────────────────────────────────────────

  void onParamListResponse(BridgeEvent.ParamListResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_LIST) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean includeTypes =
        pending != null && ctx.chat().metadataFlagEnabled(pending.metadata(), "include_types");

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Param list #" + response.requestId() + " ")
                    .formatted(Formatting.GOLD))
                .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW))
                .append(
                    Text.literal(" names=" + response.names().size()).formatted(Formatting.GREEN))
                .append(Text.literal(" prefixes=" + response.prefixes().size())
                    .formatted(Formatting.GRAY)));

    if (response.names().isEmpty()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(response.names().size(), BridgeEventChatSupport.MAX_COMMAND_ITEMS);
    for (int i = 0; i < count; i++) {
      String name = response.names().get(i);
      String type = i < response.types().size() ? response.types().get(i) : "";
      String line = includeTypes && !type.isBlank() ? name + " [" + type + "]" : name;
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
    }
    if (response.names().size() > BridgeEventChatSupport.MAX_COMMAND_ITEMS) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              Text.literal("  ... and "
                      + (response.names().size() - BridgeEventChatSupport.MAX_COMMAND_ITEMS)
                      + " more")
                  .formatted(Formatting.GRAY));
    }
  }
  // ── Param get ──────────────────────────────────────────────────────

  void onParamGetResponse(BridgeEvent.ParamGetResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_GET) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (!response.found()) {
      ctx.mod()
          .sendToRequesterOrOperators(
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
  // ── Param set ──────────────────────────────────────────────────────

  void onParamSetResponse(BridgeEvent.ParamSetResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_SET) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
  // ── Param describe ─────────────────────────────────────────────────

  void onParamDescribeResponse(BridgeEvent.ParamDescribeResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_DESCRIBE) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (!response.found()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Param not found: ").formatted(Formatting.RED))
                  .append(Text.literal(response.nodeName() + "/" + response.paramName())
                      .formatted(Formatting.YELLOW)));
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Param describe ").formatted(Formatting.GOLD))
                .append(Text.literal(response.nodeName() + "/" + response.paramName())
                    .formatted(Formatting.YELLOW))
                .append(Text.literal(" [" + response.paramType() + "]").formatted(Formatting.AQUA))
                .append(
                    Text.literal(" read_only=" + response.readOnly()).formatted(Formatting.GRAY)));
    if (!response.description().isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              Text.literal("  description: " + response.description()).formatted(Formatting.GRAY));
    }
    if (!response.constraints().isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              Text.literal("  constraints: " + response.constraints()).formatted(Formatting.GRAY));
    }
  }
  // ── Param dump ─────────────────────────────────────────────────────

  void onParamDumpResponse(BridgeEvent.ParamDumpResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_DUMP) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Param dump #" + response.requestId() + " ")
                    .formatted(Formatting.GOLD))
                .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));

    if (response.yamlText().isBlank()) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  (empty)").formatted(Formatting.DARK_GRAY));
      return;
    }

    String[] lines = response.yamlText().split("\\R", -1);
    int shown = 0;
    for (String line : lines) {
      if (line.isEmpty()) {
        continue;
      }
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
      shown++;
      if (shown >= BridgeEventChatSupport.MAX_COMMAND_ITEMS) {
        break;
      }
    }
    if (lines.length > shown) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid, Text.literal("  ... truncated").formatted(Formatting.DARK_GRAY));
    }
  }
  // ── Param load ─────────────────────────────────────────────────────

  void onParamLoadResponse(BridgeEvent.ParamLoadResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.PARAM_LOAD) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
}
