package net.roscraft.mod.bridge.callback;

import java.util.UUID;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class MiscCommandEvents {

  private final CommandEventContext ctx;

  MiscCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  // ── Player list ────────────────────────────────────────────────────

  void onPlayerList(BridgeEvent.PlayerList playerList) {
    RoscraftMod.LOGGER.debug("Player list received: {} players", playerList.size());

    CommandPendingRequest pending = ctx.requests().complete(playerList.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    if (pending != null && pending.kind() == CommandRequestKind.CONNECTION_CHECK) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              RoscraftMod.prefix()
                  .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
                  .append(Text.literal(String.valueOf(playerList.requestId()))
                      .formatted(Formatting.YELLOW))
                  .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                  .append(Text.literal("Bridge replied with " + playerList.size() + " players.")
                      .formatted(Formatting.GRAY)));
      return;
    }

    if (pending != null && pending.kind() != CommandRequestKind.PLAYERS) {
      return;
    }

    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Player list reply #" + playerList.requestId() + " ")
                    .formatted(Formatting.GOLD))
                .append(Text.literal("count=" + playerList.size()).formatted(Formatting.GREEN)));

    int count = Math.min(playerList.players().size(), BridgeEventChatSupport.MAX_PLAYERS);
    for (int i = 0; i < count; i++) {
      BridgeEvent.Player player = playerList.players().get(i);
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              Text.literal(" - ")
                  .formatted(Formatting.DARK_GRAY)
                  .append(Text.literal(player.name()).formatted(Formatting.YELLOW))
                  .append(Text.literal(
                          String.format(" (%.1f, %.1f, %.1f)", player.x(), player.y(), player.z()))
                      .formatted(Formatting.GRAY)));
    }

    if (playerList.players().size() > BridgeEventChatSupport.MAX_PLAYERS) {
      ctx.mod()
          .sendToRequesterOrOperators(
              requesterUuid,
              Text.literal(" - ... and "
                      + (playerList.players().size() - BridgeEventChatSupport.MAX_PLAYERS)
                      + " more")
                  .formatted(Formatting.GRAY));
    }
  }
  // ── Error ──────────────────────────────────────────────────────────

  void onError(BridgeEvent.BridgeError error) {
    RoscraftMod.LOGGER.error("Bridge error: [{}] {}", error.errorCode(), error.errorMessage());

    CommandPendingRequest pending = ctx.requests().complete(error.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String userMessage =
        switch (error.errorCode()) {
          case "SUBSCRIBE_FAILED" -> "Failed to subscribe: " + error.errorMessage();
          case "SUBSCRIBE_TIMEOUT" -> "Topic echo timed out: " + error.errorMessage();
          case "PUBLISH_FAILED" -> "Failed to publish: " + error.errorMessage();
          default -> "Error: " + error.errorMessage();
        };
    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("[").formatted(Formatting.DARK_RED))
                .append(Text.literal(error.errorCode()).formatted(Formatting.RED))
                .append(Text.literal("] ").formatted(Formatting.DARK_RED))
                .append(Text.literal(userMessage).formatted(Formatting.RED)));
  }
  // ── Addon event ────────────────────────────────────────────────────

  void onAddonEvent(BridgeEvent.AddonEvent event) {
    RoscraftMod.LOGGER.debug(
        "Addon event: addonId={} eventType={} requestId={} response={}",
        event.addonId(),
        event.eventType(),
        event.requestId(),
        event.response());
  }
}
