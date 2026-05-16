package net.roscraft.mod.command;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;
import net.roscraft.mod.bridge.BridgeManager;
import net.roscraft.mod.command.node.NodeCommands;

final class RoscraftCommandActions {

  private RoscraftCommandActions() {}

  // ── Connection commands ─────────────────────────────────────────────

  static int executeConnectionStatus(ServerCommandSource source) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    String status = manager.isConnected() ? "connected" : "disconnected";
    String selected = manager.selectedBridgeType();
    String active = manager.activeBridgeType();
    String endpoint = manager.activeEndpoint();
    String inbound = manager.isConnected() && "network".equals(active)
        ? (manager.networkHasSeenInboundTraffic() ? "yes" : "no")
        : "n/a";

    sendStyled(
        source,
        prefix()
            .append(Text.literal("Status ").formatted(Formatting.GOLD))
            .append(Text.literal(status).formatted(formatStatus(status)))
            .append(Text.literal(" | selected=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(selected).formatted(Formatting.YELLOW))
            .append(Text.literal(" | active=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(active).formatted(Formatting.YELLOW))
            .append(Text.literal(" | endpoint=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(endpoint).formatted(Formatting.AQUA))
            .append(Text.literal(" | jniAvailable=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(String.valueOf(manager.isJniAvailable()))
                .formatted(manager.isJniAvailable() ? Formatting.GREEN : Formatting.RED))
            .append(Text.literal(" | inboundRx=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(inbound).formatted(formatInboundStatus(inbound))));
    return 1;
  }

  static int executeHelp(ServerCommandSource source) {
    sendStyled(source, prefix().append(Text.literal("Commands:").formatted(Formatting.GOLD)));
    sendStyled(
        source,
        Text.literal(" - /ros status | connect | disconnect | probe").formatted(Formatting.GRAY));
    sendStyled(source, Text.literal(" - /ros mode <network|jni>").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros host <ip> | /ros port <port> | /ros endpoint <ip> <port>")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros players | /ros subscribe <topic> <type>").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros node list [-a|-c] | node info <name> [--include-hidden]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic list [-t|-c|--include-hidden-topics]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic type <name> | topic find <type> | topic echo <topic> [<type>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal("   [--once] [--timeout <sec>] [--raw]").formatted(Formatting.DARK_GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic info <name> [-v|--verbose]").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros topic pub <topic> <type> <payload> [--once|-1] [-r|--rate <hz>] [-t|--times <n>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros topic hz <topic> [<type>] [--window <n>] [--wall-time] | topic bw <topic> [<type>] [...]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal("   | topic hz stop [<topic>] | topic bw stop [<topic>]")
            .formatted(Formatting.DARK_GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic delay <topic> [<type>] [--window <n>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal("   | topic delay stop [<topic>] | topic echo stop [<topic>]")
            .formatted(Formatting.DARK_GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros service list [-t|-c|--include-hidden-services] | service type/find/info")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros service info <name> [-v|--verbose]").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros service call <name> <type> <request> [--timeout <sec>] [-r|--repeat <n>] [--rate <hz>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros action list [-t] | action type <name> | action info <name> [--include-hidden]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros action send_goal <name> <type> <goal> [-f|--feedback] [--timeout <sec>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros param list/get/set/describe/dump ... (ros2-style flags)")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros param load <node> <parameter_file> [--timeout <sec>]")
            .formatted(Formatting.GRAY));
    sendStyled(source, Text.literal("   [--no-use-wildcard]").formatted(Formatting.DARK_GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros interface list [-m|-s|-a] | interface show <type>")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal("Tip: in WSL2 network mode, use /ros endpoint <wsl-ip> <port>")
            .formatted(Formatting.DARK_AQUA));
    return 1;
  }

  static int executeConnect(ServerCommandSource source) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.connect();
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));

    if ("network".equals(manager.activeBridgeType())) {
      sendStyled(
          source,
          prefix()
              .append(Text.literal("Running connectivity probe...").formatted(Formatting.GOLD)));
      return executeProbe(source);
    }
    return 1;
  }

  static int executeDisconnect(ServerCommandSource source) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.disconnect();
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(
        source, prefix().append(Text.literal(result.message()).formatted(Formatting.YELLOW)));
    return 1;
  }

  static int executeSetMode(ServerCommandSource source, String type) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.setBridgeType(type);
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  static int executeSetHost(ServerCommandSource source, String host) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.setNetworkHost(host);
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  static int executeSetPort(ServerCommandSource source, int port) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.setNetworkPort(port);
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  static int executeSetEndpoint(ServerCommandSource source, String host, int port) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    BridgeManager.OperationResult result = manager.setNetworkEndpoint(host, port);
    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  static int executeProbe(ServerCommandSource source) {
    RoscraftBridge bridge = requireBridge(source);
    if (bridge == null) {
      return 0;
    }

    long requestId = bridge.queryGraph();
    RoscraftMod mod = getMod();
    if (mod != null) {
      mod.trackRequest(requestId, PendingRequestKind.CONNECTION_CHECK, requesterUuid(source));
    }

    sendStyled(
        source,
        prefix()
            .append(Text.literal("Probe request #").formatted(Formatting.GOLD))
            .append(Text.literal(String.valueOf(requestId)).formatted(Formatting.YELLOW))
            .append(Text.literal(" sent.").formatted(Formatting.GRAY)));
    return 1;
  }

  // ── Node commands ────────────────────────────────────────────────────

  static int executeNodeList(ServerCommandSource source, String rawFlags) {
    boolean includeHidden = false;
    boolean count = false;
    List<String> flags = parseFlagsOrError(source, "/ros node list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-a", "--all" -> includeHidden = true;
        case "-c", "--count" -> count = true;
        default -> {
          sendError(source, "Unsupported flag for /ros node list: " + flag);
          return 0;
        }
      }
    }

    NodeCommands.NodeListOptions options = NodeCommands.NodeListOptions.builder()
        .includeHidden(includeHidden)
        .countOnly(count)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.NODE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> NodeCommands.list(ctx, options));
  }

  static int executeNodeInfo(ServerCommandSource source, String nodeName, String rawFlags) {
    boolean includeHidden = false;
    List<String> flags = parseFlagsOrError(source, "/ros node info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--include-hidden".equals(flag)) {
        includeHidden = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros node info: " + flag);
      return 0;
    }

    NodeCommands.NodeInfoOptions options =
        NodeCommands.NodeInfoOptions.builder().includeHidden(includeHidden).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.NODE_INFO,
        options.encodeTrackingMetadata(),
        ctx -> NodeCommands.info(ctx, nodeName, options));
  }

  // ── Players ──────────────────────────────────────────────────────────

  static int executePlayers(ServerCommandSource source) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    if ("network".equals(manager.activeBridgeType())) {
      sendError(
          source,
          "/ros players is currently unavailable in network mode. "
              + "Use /ros probe or /ros node list to verify connectivity.");
      return 0;
    }

    RoscraftBridge bridge = requireBridge(source);
    if (bridge == null) {
      return 0;
    }

    long requestId = bridge.queryPlayers();
    RoscraftMod mod = getMod();
    if (mod != null) {
      mod.trackRequest(requestId, PendingRequestKind.PLAYERS, requesterUuid(source));
    }

    sendStyled(
        source,
        prefix()
            .append(Text.literal("Player list request #").formatted(Formatting.GOLD))
            .append(Text.literal(String.valueOf(requestId)).formatted(Formatting.YELLOW))
            .append(Text.literal(" sent. Waiting for reply...").formatted(Formatting.GRAY)));
    return 1;
  }

  // ── Logic command execution (shared) ─────────────────────────────────

  static int executeLogicCommand(
      ServerCommandSource source,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    return executeLogicCommand(source, null, null, commandLogic);
  }

  static int executeLogicCommand(
      ServerCommandSource source,
      PendingRequestKind pendingKind,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    return executeLogicCommand(source, pendingKind, null, commandLogic);
  }

  static int executeLogicCommand(
      ServerCommandSource source,
      PendingRequestKind pendingKind,
      String trackingMetadata,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    UUID requesterUuid = requesterUuid(source);
    CommandResult result;
    try {
      result = commandLogic.apply(new CommandContext(manager, requesterUuid));
    } catch (IllegalStateException e) {
      sendError(source, e.getMessage());
      return 0;
    }

    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    if (pendingKind != null && result.requestId() != 0L) {
      RoscraftMod mod = getMod();
      if (mod != null) {
        mod.trackRequest(result.requestId(), pendingKind, requesterUuid, trackingMetadata);
      }
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  // ── Flag parsing ─────────────────────────────────────────────────────

  static List<String> parseFlags(String rawFlags) {
    if (rawFlags == null || rawFlags.isBlank()) {
      return List.of();
    }

    List<String> tokens = new ArrayList<>();
    StringBuilder current = new StringBuilder();
    boolean inQuotes = false;
    char quoteChar = '\0';
    boolean escaping = false;

    for (int index = 0; index < rawFlags.length(); index++) {
      char ch = rawFlags.charAt(index);

      if (escaping) {
        current.append(ch);
        escaping = false;
        continue;
      }

      if (ch == '\\') {
        escaping = true;
        continue;
      }

      if (inQuotes) {
        if (ch == quoteChar) {
          inQuotes = false;
          quoteChar = '\0';
        } else {
          current.append(ch);
        }
        continue;
      }

      if (ch == '"' || ch == '\'') {
        inQuotes = true;
        quoteChar = ch;
        continue;
      }

      if (Character.isWhitespace(ch)) {
        if (current.length() > 0) {
          tokens.add(current.toString());
          current.setLength(0);
        }
        continue;
      }

      current.append(ch);
    }

    if (escaping) {
      throw new IllegalStateException("Malformed flags: dangling escape at end of input.");
    }
    if (inQuotes) {
      throw new IllegalStateException("Malformed flags: unterminated quoted value.");
    }
    if (current.length() > 0) {
      tokens.add(current.toString());
    }

    return List.copyOf(tokens);
  }

  static List<String> parseFlagsOrError(
      ServerCommandSource source, String commandName, String rawFlags) {
    try {
      return parseFlags(rawFlags);
    } catch (IllegalStateException ex) {
      sendError(source, commandName + ": " + ex.getMessage());
      return null;
    }
  }

  // ── Bridge helpers ───────────────────────────────────────────────────

  static RoscraftBridge requireBridge(ServerCommandSource source) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return null;
    }

    try {
      return manager.requireConnectedBridge();
    } catch (IllegalStateException e) {
      sendError(source, e.getMessage());
      return null;
    }
  }

  static UUID requesterUuid(ServerCommandSource source) {
    try {
      if (source.getEntity() != null) {
        return source.getEntity().getUuid();
      }
    } catch (RuntimeException ignored) {
    }
    return null;
  }

  static RoscraftMod getMod() {
    var mods = net.fabricmc.loader.api.FabricLoader.getInstance()
        .getEntrypoints("main", net.fabricmc.api.ModInitializer.class);
    for (var mod : mods) {
      if (mod instanceof RoscraftMod rm) {
        return rm;
      }
    }
    RoscraftMod.LOGGER.error("RoscraftMod entrypoint not found!");
    return null;
  }

  static BridgeManager getBridgeManager() {
    RoscraftMod mod = getMod();
    if (mod == null) {
      return null;
    }
    return mod.bridgeManager();
  }

  // ── Display helpers ──────────────────────────────────────────────────

  static Formatting formatStatus(String status) {
    if ("connected".equals(status)) {
      return Formatting.GREEN;
    }
    return Formatting.RED;
  }

  static Formatting formatInboundStatus(String inbound) {
    if ("yes".equals(inbound)) {
      return Formatting.GREEN;
    }
    if ("no".equals(inbound)) {
      return Formatting.RED;
    }
    return Formatting.GRAY;
  }

  static void sendStyled(ServerCommandSource source, Text message) {
    source.sendFeedback(() -> message, false);
  }

  static void sendError(ServerCommandSource source, String message) {
    source.sendError(prefix().append(Text.literal(message).formatted(Formatting.RED)));
  }

  static MutableText prefix() {
    return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
  }
}
