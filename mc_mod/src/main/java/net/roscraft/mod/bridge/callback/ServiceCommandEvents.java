package net.roscraft.mod.bridge.callback;

import java.util.UUID;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class ServiceCommandEvents {

  private final CommandEventContext ctx;

  ServiceCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  // ── Service info ───────────────────────────────────────────────────

  void onServiceInfoResponse(BridgeEvent.ServiceInfoResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean verbose =
        pending != null && ctx.chat().metadataFlagEnabled(pending.metadata(), "verbose");

    if (pending != null && pending.kind() != CommandRequestKind.SERVICE_INFO) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);

    if (!verbose) {
      return;
    }

    ctx.chat()
        .sendListPreview(requesterUuid, "Client nodes", response.clientNodes(), Formatting.GRAY);
    ctx.chat()
        .sendListPreview(requesterUuid, "Server nodes", response.serverNodes(), Formatting.GRAY);
  }
  // ── Service call ───────────────────────────────────────────────────

  void onServiceCallResponse(BridgeEvent.ServiceCallResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.SERVICE_CALL) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);
  }
}
