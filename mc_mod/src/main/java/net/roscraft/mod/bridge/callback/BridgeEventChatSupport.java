package net.roscraft.mod.bridge.callback;

import java.util.List;
import java.util.UUID;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.mod.RoscraftMod;

final class BridgeEventChatSupport {

  static final int MAX_GRAPH_ITEMS = 8;
  static final int MAX_COMMAND_ITEMS = 64;
  static final int MAX_PLAYERS = 10;

  private final RoscraftMod mod;

  BridgeEventChatSupport(RoscraftMod mod) {
    this.mod = mod;
  }

  // ── Display helpers ────────────────────────────────────────────────

  void sendValues(UUID requesterUuid, List<String> values, Formatting color) {
    if (values.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(values.size(), BridgeEventChatSupport.MAX_COMMAND_ITEMS);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal("  " + values.get(i)).formatted(color));
    }

    if (values.size() > BridgeEventChatSupport.MAX_COMMAND_ITEMS) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  ... and " + (values.size() - BridgeEventChatSupport.MAX_COMMAND_ITEMS)
                  + " more")
              .formatted(Formatting.GRAY));
    }
  }

  boolean metadataFlagEnabled(String metadata, String key) {
    String value = metadataValue(metadata, key);
    return "1".equals(value) || "true".equalsIgnoreCase(value);
  }

  String metadataValue(String metadata, String key) {
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

  boolean isHiddenName(String name) {
    for (String segment : name.split("/")) {
      if (!segment.isEmpty() && segment.startsWith("_")) {
        return true;
      }
    }
    return false;
  }

  void sendListPreview(UUID requesterUuid, String label, List<String> values, Formatting color) {
    if (values.isEmpty()) {
      mod.sendToRequesterOrOperators(
          requesterUuid, Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
      return;
    }

    int count = Math.min(values.size(), BridgeEventChatSupport.MAX_GRAPH_ITEMS);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label + ": ")
              .formatted(Formatting.DARK_GRAY)
              .append(Text.literal(values.get(i)).formatted(color)));
    }

    if (values.size() > BridgeEventChatSupport.MAX_GRAPH_ITEMS) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label
                  + ": ... and "
                  + (values.size() - BridgeEventChatSupport.MAX_GRAPH_ITEMS)
                  + " more")
              .formatted(Formatting.GRAY));
    }
  }

  <T> void sendEntryListPreview(
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

    int count = Math.min(entries.size(), BridgeEventChatSupport.MAX_GRAPH_ITEMS);
    for (int i = 0; i < count; i++) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label + ": ")
              .formatted(Formatting.DARK_GRAY)
              .append(Text.literal(formatter.apply(entries.get(i))).formatted(color)));
    }

    if (entries.size() > BridgeEventChatSupport.MAX_GRAPH_ITEMS) {
      mod.sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(" - " + label
                  + ": ... and "
                  + (entries.size() - BridgeEventChatSupport.MAX_GRAPH_ITEMS)
                  + " more")
              .formatted(Formatting.GRAY));
    }
  }
}
