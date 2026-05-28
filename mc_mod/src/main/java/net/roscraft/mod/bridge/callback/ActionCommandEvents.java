package net.roscraft.mod.bridge.callback;

import java.util.UUID;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class ActionCommandEvents {

  private final CommandEventContext ctx;

  ActionCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  // ── Action info ────────────────────────────────────────────────────

  void onActionInfoResponse(BridgeEvent.ActionInfoResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.ACTION_INFO) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);

    ctx.mod()
        .sendToRequesterOrOperators(
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

  void onActionFeedback(BridgeEvent.ActionFeedback feedback) {
    CommandPendingRequest pending = ctx.requests().peek(feedback.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.ACTION_SEND_GOAL) {
      ctx.requests().complete(feedback.requestId());
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
  // ── Action result ──────────────────────────────────────────────────

  void onActionResult(BridgeEvent.ActionResult result) {
    CommandPendingRequest pending = ctx.requests().complete(result.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.ACTION_SEND_GOAL) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
}
