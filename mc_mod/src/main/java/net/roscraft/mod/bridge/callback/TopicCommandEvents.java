package net.roscraft.mod.bridge.callback;

import java.util.UUID;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandPendingRequest;
import net.roscraft.mod.command.request.CommandRequestKind;

final class TopicCommandEvents {

  private final CommandEventContext ctx;

  TopicCommandEvents(CommandEventContext ctx) {
    this.ctx = ctx;
  }

  // ── Topic payload ──────────────────────────────────────────────────

  void onTopicPayload(BridgeEvent.TopicPayload payload) {
    RoscraftMod.LOGGER.trace(
        "Topic payload: {} ({} bytes, requestId={}, raw={})",
        payload.topicName(),
        payload.payloadLength(),
        payload.requestId(),
        payload.raw());

    CommandPendingRequest pending = ctx.requests().peek(payload.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.TOPIC_ECHO) {
      ctx.requests().complete(payload.requestId());
      return;
    }

    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Topic echo ").formatted(Formatting.GOLD))
                .append(Text.literal(payload.topicName()).formatted(Formatting.YELLOW))
                .append(
                    Text.literal(" (" + payload.messageType() + ") ").formatted(Formatting.GRAY))
                .append(Text.literal("bytes=").formatted(Formatting.DARK_GRAY))
                .append(Text.literal(String.valueOf(payload.payloadLength()))
                    .formatted(Formatting.GREEN))
                .append(Text.literal(" raw=").formatted(Formatting.DARK_GRAY))
                .append(Text.literal(String.valueOf(payload.raw())).formatted(Formatting.AQUA)));

    if (pending != null && ctx.chat().metadataFlagEnabled(pending.metadata(), "once")) {
      ctx.requests().complete(payload.requestId());
    }
  }
  // ── Topic info ─────────────────────────────────────────────────────

  void onTopicInfoResponse(BridgeEvent.TopicInfoResponse response) {
    CommandPendingRequest pending = ctx.requests().complete(response.requestId());
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();
    boolean verbose =
        pending != null && ctx.chat().metadataFlagEnabled(pending.metadata(), "verbose");

    if (pending != null && pending.kind() != CommandRequestKind.TOPIC_INFO) {
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
    ctx.mod().sendToRequesterOrOperators(requesterUuid, line);

    if (!verbose) {
      return;
    }

    ctx.chat()
        .sendListPreview(
            requesterUuid, "Publisher nodes", response.publisherNodes(), Formatting.GRAY);
    ctx.chat()
        .sendListPreview(
            requesterUuid, "Subscriber nodes", response.subscriberNodes(), Formatting.GRAY);
  }
  // ── Topic Hz ───────────────────────────────────────────────────────

  void onTopicHzResponse(BridgeEvent.TopicHzResponse response) {
    CommandPendingRequest pending = ctx.requests().peek(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.TOPIC_HZ) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String formattedFreq = String.format("%.2f", response.frequency());
    ctx.mod()
        .sendToRequesterOrOperators(
            requesterUuid,
            RoscraftMod.prefix()
                .append(Text.literal("Topic hz ").formatted(Formatting.GOLD))
                .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
                .append(Text.literal(" average rate: ").formatted(Formatting.GOLD))
                .append(Text.literal(formattedFreq).formatted(Formatting.GREEN))
                .append(Text.literal(" Hz").formatted(Formatting.GREEN))
                .append(Text.literal(
                        "  min: 0.00 Hz  max: " + String.format("%.2f", response.frequency())
                            + " Hz"
                            + "  window: "
                            + response.window()
                            + "  samples: "
                            + response.messageCount())
                    .formatted(Formatting.GRAY)));
  }
  // ── Topic Bw ───────────────────────────────────────────────────────

  void onTopicBwResponse(BridgeEvent.TopicBwResponse response) {
    CommandPendingRequest pending = ctx.requests().peek(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.TOPIC_BW) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    String formattedBw = String.format("%.2f", response.bytesPerSecond());
    String formattedTotal = String.format("%.2f KB", response.totalBytes() / 1024.0);
    ctx.mod()
        .sendToRequesterOrOperators(
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

  void onTopicDelayResponse(BridgeEvent.TopicDelayResponse response) {
    CommandPendingRequest pending = ctx.requests().peek(response.requestId());
    if (pending != null && pending.kind() != CommandRequestKind.TOPIC_DELAY) {
      return;
    }
    UUID requesterUuid = pending == null ? null : pending.requesterUuid();

    ctx.mod()
        .sendToRequesterOrOperators(
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
}
