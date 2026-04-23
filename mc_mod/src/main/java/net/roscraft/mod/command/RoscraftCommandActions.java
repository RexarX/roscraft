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
import net.roscraft.mod.command.action.ActionCommands;
import net.roscraft.mod.command.interface_cmd.InterfaceCommands;
import net.roscraft.mod.command.node.NodeCommands;
import net.roscraft.mod.command.param.ParamCommands;
import net.roscraft.mod.command.service.ServiceCommands;
import net.roscraft.mod.command.topic.TopicCommands;

final class RoscraftCommandActions {

  private RoscraftCommandActions() {}

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
        Text.literal(" - /ros topic delay <topic> [<type>] [--window <n>]")
            .formatted(Formatting.GRAY));
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

  static int executeSubscribe(ServerCommandSource source, String topic, String type) {
    TopicCommands.TopicEchoOptions options =
        TopicCommands.TopicEchoOptions.builder().build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.echo(ctx, topic, type, options));
  }

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

  static int executeTopicList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    List<String> flags = parseFlagsOrError(source, "/ros topic list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-topics" -> includeHidden = true;
        default -> {
          sendError(source, "Unsupported flag for /ros topic list: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicListOptions options = TopicCommands.TopicListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenTopics(includeHidden)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_LIST,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.list(ctx, options));
  }

  static int executeTopicType(ServerCommandSource source, String topicName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_TYPE,
        topicName,
        ctx -> TopicCommands.type(ctx, topicName));
  }

  static int executeTopicFind(ServerCommandSource source, String topicType) {
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_FIND,
        topicType,
        ctx -> TopicCommands.find(ctx, topicType));
  }

  static int executeTopicEcho(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags = parseFlagsOrError(source, "/ros topic echo", rawFlags);
    if (flags == null) {
      return 0;
    }
    return executeTopicEchoWithFlags(source, topicName, messageType, flags);
  }

  static int executeTopicEchoTail(ServerCommandSource source, String topicName, String rawTail) {
    TopicCommandTail tail = parseTopicCommandTailOrError(source, "/ros topic echo", rawTail);
    if (tail == null) {
      return 0;
    }
    return executeTopicEchoWithFlags(source, topicName, tail.messageType(), tail.flags());
  }

  private static int executeTopicEchoWithFlags(
      ServerCommandSource source, String topicName, String messageType, List<String> flags) {
    boolean once = false;
    boolean raw = false;
    double timeoutSeconds = 0.0;

    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--once" -> once = true;
        case "--raw" -> raw = true;
        case "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --timeout");
            return 0;
          }
          String timeoutToken = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(timeoutToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + timeoutToken);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String timeoutToken = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(timeoutToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + timeoutToken);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic echo: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicEchoOptions options = TopicCommands.TopicEchoOptions.builder()
        .once(once)
        .timeoutSeconds(timeoutSeconds)
        .raw(raw)
        .build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.echo(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicInfo(ServerCommandSource source, String topicName, String rawFlags) {
    boolean verbose = false;
    List<String> flags = parseFlagsOrError(source, "/ros topic info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          sendError(source, "Unsupported flag for /ros topic info: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicInfoOptions options =
        TopicCommands.TopicInfoOptions.builder().verbose(verbose).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_INFO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.info(ctx, topicName, options));
  }

  static int executeTopicPub(
      ServerCommandSource source,
      String topicName,
      String messageType,
      String payloadText,
      String rawFlags) {
    boolean once = false;
    double rateHz = 0.0;
    int times = 0;
    String qosProfile = "default";

    List<String> flags = parseFlagsOrError(source, "/ros topic pub", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--once", "-1" -> once = true;
        case "-r", "--rate" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String rateToken = flags.get(++i);
          try {
            rateHz = Double.parseDouble(rateToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid rate value: " + rateToken);
            return 0;
          }
          if (rateHz < 0.0) {
            sendError(source, "Rate must be non-negative.");
            return 0;
          }
        }
        case "-t", "--times" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String timesToken = flags.get(++i);
          try {
            times = Integer.parseInt(timesToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid times value: " + timesToken);
            return 0;
          }
          if (times < 0) {
            sendError(source, "Times must be non-negative.");
            return 0;
          }
        }
        case "--qos-profile" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --qos-profile");
            return 0;
          }
          qosProfile = flags.get(++i);
        }
        default -> {
          if (flag.startsWith("--rate=")) {
            String rateToken = flag.substring("--rate=".length());
            try {
              rateHz = Double.parseDouble(rateToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid rate value: " + rateToken);
              return 0;
            }
            if (rateHz < 0.0) {
              sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--times=")) {
            String timesToken = flag.substring("--times=".length());
            try {
              times = Integer.parseInt(timesToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid times value: " + timesToken);
              return 0;
            }
            if (times < 0) {
              sendError(source, "Times must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--qos-profile=")) {
            qosProfile = flag.substring("--qos-profile=".length());
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic pub: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicPublishOptions options = TopicCommands.TopicPublishOptions.builder()
        .once(once)
        .rateHz(rateHz)
        .times(times)
        .qosProfile(qosProfile)
        .build();
    byte[] payload = payloadText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_PUB,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.pub(ctx, topicName, messageType, payload, options));
  }

  static int executeTopicHz(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags = parseFlagsOrError(source, "/ros topic hz", rawFlags);
    if (flags == null) {
      return 0;
    }
    return executeTopicHzWithFlags(source, topicName, messageType, flags);
  }

  static int executeTopicHzTail(ServerCommandSource source, String topicName, String rawTail) {
    TopicCommandTail tail = parseTopicCommandTailOrError(source, "/ros topic hz", rawTail);
    if (tail == null) {
      return 0;
    }
    return executeTopicHzWithFlags(source, topicName, tail.messageType(), tail.flags());
  }

  private static int executeTopicHzWithFlags(
      ServerCommandSource source, String topicName, String messageType, List<String> flags) {
    int window = 10000;
    boolean wallTime = false;

    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--wall-time" -> wallTime = true;
        case "--window" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic hz: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicHzOptions options =
        TopicCommands.TopicHzOptions.builder().window(window).wallTime(wallTime).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_HZ,
        trackingMetadata,
        ctx -> TopicCommands.hz(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicBw(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags = parseFlagsOrError(source, "/ros topic bw", rawFlags);
    if (flags == null) {
      return 0;
    }
    return executeTopicBwWithFlags(source, topicName, messageType, flags);
  }

  static int executeTopicBwTail(ServerCommandSource source, String topicName, String rawTail) {
    TopicCommandTail tail = parseTopicCommandTailOrError(source, "/ros topic bw", rawTail);
    if (tail == null) {
      return 0;
    }
    return executeTopicBwWithFlags(source, topicName, tail.messageType(), tail.flags());
  }

  private static int executeTopicBwWithFlags(
      ServerCommandSource source, String topicName, String messageType, List<String> flags) {
    int window = 100;
    boolean wallTime = false;

    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--wall-time" -> wallTime = true;
        case "--window" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic bw: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicBwOptions options =
        TopicCommands.TopicBwOptions.builder().window(window).wallTime(wallTime).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_BW,
        trackingMetadata,
        ctx -> TopicCommands.bw(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicDelay(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags = parseFlagsOrError(source, "/ros topic delay", rawFlags);
    if (flags == null) {
      return 0;
    }
    return executeTopicDelayWithFlags(source, topicName, messageType, flags);
  }

  static int executeTopicDelayTail(ServerCommandSource source, String topicName, String rawTail) {
    TopicCommandTail tail = parseTopicCommandTailOrError(source, "/ros topic delay", rawTail);
    if (tail == null) {
      return 0;
    }
    return executeTopicDelayWithFlags(source, topicName, tail.messageType(), tail.flags());
  }

  private static int executeTopicDelayWithFlags(
      ServerCommandSource source, String topicName, String messageType, List<String> flags) {
    int window = 10;

    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--window".equals(flag)) {
        if (i + 1 >= flags.size()) {
          sendError(source, "Missing value for --window");
          return 0;
        }
        String windowToken = flags.get(++i);
        try {
          window = Integer.parseInt(windowToken);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid window value: " + windowToken);
          return 0;
        }
        if (window < 1) {
          sendError(source, "Window must be >= 1.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--window=")) {
        String windowToken = flag.substring("--window=".length());
        try {
          window = Integer.parseInt(windowToken);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid window value: " + windowToken);
          return 0;
        }
        if (window < 1) {
          sendError(source, "Window must be >= 1.");
          return 0;
        }
        continue;
      }

      sendError(source, "Unsupported flag for /ros topic delay: " + flag);
      return 0;
    }

    TopicCommands.TopicDelayOptions options =
        TopicCommands.TopicDelayOptions.builder().window(window).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_DELAY,
        trackingMetadata,
        ctx -> TopicCommands.delay(ctx, topicName, resolvedMessageType, options));
  }

  static int executeServiceList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    List<String> flags = parseFlagsOrError(source, "/ros service list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-services" -> includeHidden = true;
        default -> {
          sendError(source, "Unsupported flag for /ros service list: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceListOptions options = ServiceCommands.ServiceListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenServices(includeHidden)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.list(ctx, options));
  }

  static int executeServiceType(ServerCommandSource source, String serviceName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_TYPE,
        serviceName,
        ctx -> ServiceCommands.type(ctx, serviceName));
  }

  static int executeServiceFind(ServerCommandSource source, String serviceType) {
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_FIND,
        serviceType,
        ctx -> ServiceCommands.find(ctx, serviceType));
  }

  static int executeServiceInfo(ServerCommandSource source, String serviceName, String rawFlags) {
    boolean verbose = false;
    List<String> flags = parseFlagsOrError(source, "/ros service info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          sendError(source, "Unsupported flag for /ros service info: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceInfoOptions options =
        ServiceCommands.ServiceInfoOptions.builder().verbose(verbose).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_INFO,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.info(ctx, serviceName, options));
  }

  static int executeServiceCall(
      ServerCommandSource source,
      String serviceName,
      String serviceType,
      String requestText,
      String rawFlags) {
    double timeoutSeconds = 0.0;
    int repeatCount = 0;
    double rateHz = 0.0;

    List<String> flags = parseFlagsOrError(source, "/ros service call", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --timeout");
            return 0;
          }
          String token = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + token);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        case "-r", "--repeat" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String token = flags.get(++i);
          try {
            repeatCount = Integer.parseInt(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid repeat value: " + token);
            return 0;
          }
          if (repeatCount < 0) {
            sendError(source, "Repeat must be non-negative.");
            return 0;
          }
        }
        case "--rate" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --rate");
            return 0;
          }
          String token = flags.get(++i);
          try {
            rateHz = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid rate value: " + token);
            return 0;
          }
          if (rateHz < 0.0) {
            sendError(source, "Rate must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String token = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + token);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--repeat=")) {
            String token = flag.substring("--repeat=".length());
            try {
              repeatCount = Integer.parseInt(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid repeat value: " + token);
              return 0;
            }
            if (repeatCount < 0) {
              sendError(source, "Repeat must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--rate=")) {
            String token = flag.substring("--rate=".length());
            try {
              rateHz = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid rate value: " + token);
              return 0;
            }
            if (rateHz < 0.0) {
              sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros service call: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceCallOptions options = ServiceCommands.ServiceCallOptions.builder()
        .timeoutSeconds(timeoutSeconds)
        .repeatCount(repeatCount)
        .rateHz(rateHz)
        .build();
    String trackingMetadata = "service_name=" + serviceName
        + ";service_type="
        + serviceType
        + ";"
        + options.encodeTrackingMetadata();
    byte[] payload = requestText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_CALL,
        trackingMetadata,
        ctx -> ServiceCommands.call(ctx, serviceName, serviceType, payload, options));
  }

  static int executeActionList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    List<String> flags = parseFlagsOrError(source, "/ros action list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        default -> {
          sendError(source, "Unsupported flag for /ros action list: " + flag);
          return 0;
        }
      }
    }

    ActionCommands.ActionListOptions options =
        ActionCommands.ActionListOptions.builder().showTypes(showTypes).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ActionCommands.list(ctx, options));
  }

  static int executeActionType(ServerCommandSource source, String actionName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_TYPE,
        actionName,
        ctx -> ActionCommands.type(ctx, actionName));
  }

  static int executeActionInfo(ServerCommandSource source, String actionName, String rawFlags) {
    boolean includeHidden = false;
    List<String> flags = parseFlagsOrError(source, "/ros action info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--include-hidden".equals(flag)) {
        includeHidden = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros action info: " + flag);
      return 0;
    }

    ActionCommands.ActionInfoOptions options =
        ActionCommands.ActionInfoOptions.builder().includeHidden(includeHidden).build();
    String trackingMetadata = "action_name=" + actionName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_INFO,
        trackingMetadata,
        ctx -> ActionCommands.info(ctx, actionName, options));
  }

  static int executeActionSendGoal(
      ServerCommandSource source,
      String actionName,
      String actionType,
      String goalText,
      String rawFlags) {
    boolean feedback = false;
    double timeoutSeconds = 0.0;

    List<String> flags = parseFlagsOrError(source, "/ros action send_goal", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "-f", "--feedback" -> feedback = true;
        case "-t", "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String token = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + token);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String token = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + token);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros action send_goal: " + flag);
          return 0;
        }
      }
    }

    ActionCommands.ActionSendGoalOptions options = ActionCommands.ActionSendGoalOptions.builder()
        .feedback(feedback)
        .timeoutSeconds(timeoutSeconds)
        .build();
    String trackingMetadata = "action_name=" + actionName
        + ";action_type="
        + actionType
        + ";"
        + options.encodeTrackingMetadata();
    byte[] payload = goalText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_SEND_GOAL,
        trackingMetadata,
        ctx -> ActionCommands.sendGoal(ctx, actionName, actionType, payload, options));
  }

  static int executeParamList(ServerCommandSource source, String nodeName, String rawFlags) {
    long depth = 0L;
    boolean includeTypes = false;
    String filterRegex = "";
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags = parseFlagsOrError(source, "/ros param list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--depth" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --depth");
            return 0;
          }
          String token = flags.get(++i);
          try {
            depth = Long.parseLong(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid depth value: " + token);
            return 0;
          }
          if (depth < 0L) {
            sendError(source, "Depth must be non-negative.");
            return 0;
          }
        }
        case "--param-type" -> includeTypes = true;
        case "--filter" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --filter");
            return 0;
          }
          filterRegex = flags.get(++i);
        }
        case "--param-prefixes" -> {
          while (i + 1 < flags.size() && !flags.get(i + 1).startsWith("-")) {
            prefixes.add(flags.get(++i));
          }
        }
        default -> {
          if (flag.startsWith("--depth=")) {
            String token = flag.substring("--depth=".length());
            try {
              depth = Long.parseLong(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid depth value: " + token);
              return 0;
            }
            if (depth < 0L) {
              sendError(source, "Depth must be non-negative.");
              return 0;
            }
            continue;
          }
          if (flag.startsWith("--filter=")) {
            filterRegex = flag.substring("--filter=".length());
            continue;
          }

          sendError(source, "Unsupported flag for /ros param list: " + flag);
          return 0;
        }
      }
    }

    ParamCommands.ParamListOptions options = ParamCommands.ParamListOptions.builder()
        .nodeName(nodeName)
        .prefixes(prefixes.toArray(String[]::new))
        .depth(depth)
        .includeTypes(includeTypes)
        .filterRegex(filterRegex)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.list(ctx, options));
  }

  static int executeParamGet(
      ServerCommandSource source, String nodeName, String paramName, String rawFlags) {
    boolean hideType = false;
    List<String> flags = parseFlagsOrError(source, "/ros param get", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--hide-type".equals(flag)) {
        hideType = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros param get: " + flag);
      return 0;
    }

    ParamCommands.ParamGetOptions options = ParamCommands.ParamGetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .hideType(hideType)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_GET,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.get(ctx, options));
  }

  static int executeParamSet(
      ServerCommandSource source,
      String nodeName,
      String paramName,
      String valueText,
      String rawFlags) {
    double timeoutSeconds = 0.0;

    List<String> flags = parseFlagsOrError(source, "/ros param set", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--timeout".equals(flag)) {
        if (i + 1 >= flags.size()) {
          sendError(source, "Missing value for --timeout");
          return 0;
        }
        String token = flags.get(++i);
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--timeout=")) {
        String token = flag.substring("--timeout=".length());
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      sendError(source, "Unsupported flag for /ros param set: " + flag);
      return 0;
    }

    ParamCommands.ParamSetOptions options = ParamCommands.ParamSetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .valueText(valueText)
        .timeoutSeconds(timeoutSeconds)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_SET,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.set(ctx, options));
  }

  static int executeParamDescribe(ServerCommandSource source, String nodeName, String paramName) {
    ParamCommands.ParamDescribeOptions options = ParamCommands.ParamDescribeOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DESCRIBE,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.describe(ctx, options));
  }

  static int executeParamDump(ServerCommandSource source, String nodeName, String rawFlags) {
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags = parseFlagsOrError(source, "/ros param dump", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--param-prefixes".equals(flag)) {
        while (i + 1 < flags.size() && !flags.get(i + 1).startsWith("-")) {
          prefixes.add(flags.get(++i));
        }
        continue;
      }
      sendError(source, "Unsupported flag for /ros param dump: " + flag);
      return 0;
    }

    ParamCommands.ParamDumpOptions options = ParamCommands.ParamDumpOptions.builder()
        .nodeName(nodeName)
        .prefixes(prefixes.toArray(String[]::new))
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DUMP,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.dump(ctx, options));
  }

  static int executeParamLoad(
      ServerCommandSource source, String nodeName, String parameterFile, String rawFlags) {
    double timeoutSeconds = 0.0;
    boolean useWildcard = true;

    List<String> flags = parseFlagsOrError(source, "/ros param load", rawFlags);
    if (flags == null) {
      return 0;
    }

    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--no-use-wildcard".equals(flag)) {
        useWildcard = false;
        continue;
      }

      if ("--timeout".equals(flag)) {
        if (i + 1 >= flags.size()) {
          sendError(source, "Missing value for --timeout");
          return 0;
        }
        String token = flags.get(++i);
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--timeout=")) {
        String token = flag.substring("--timeout=".length());
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      sendError(source, "Unsupported flag for /ros param load: " + flag);
      return 0;
    }

    ParamCommands.ParamLoadOptions options = ParamCommands.ParamLoadOptions.builder()
        .nodeName(nodeName)
        .parameterFile(parameterFile)
        .timeoutSeconds(timeoutSeconds)
        .useWildcard(useWildcard)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_LOAD,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.load(ctx, options));
  }

  static int executeInterfaceShow(
      ServerCommandSource source, String interfaceType, String rawFlags) {
    boolean noComments = false;
    List<String> flags = parseFlagsOrError(source, "/ros interface show", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--no-comments".equals(flag)) {
        noComments = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros interface show: " + flag);
      return 0;
    }

    InterfaceCommands.InterfaceShowOptions options =
        InterfaceCommands.InterfaceShowOptions.builder().noComments(noComments).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.INTERFACE_SHOW,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.show(ctx, interfaceType, options));
  }

  static int executeInterfaceList(ServerCommandSource source, String rawFlags) {
    boolean onlyMsgs = false;
    boolean onlySrvs = false;
    boolean onlyActions = false;

    List<String> flags = parseFlagsOrError(source, "/ros interface list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-m", "--only-msgs" -> onlyMsgs = true;
        case "-s", "--only-srvs" -> onlySrvs = true;
        case "-a", "--only-actions" -> onlyActions = true;
        default -> {
          sendError(source, "Unsupported flag for /ros interface list: " + flag);
          return 0;
        }
      }
    }

    int selected = (onlyMsgs ? 1 : 0) + (onlySrvs ? 1 : 0) + (onlyActions ? 1 : 0);
    if (selected > 1) {
      sendError(source, "Choose at most one of -m, -s, or -a for /ros interface list.");
      return 0;
    }

    boolean includeMessages = !onlySrvs && !onlyActions;
    boolean includeServices = !onlyMsgs && !onlyActions;
    boolean includeActions = !onlyMsgs && !onlySrvs;

    InterfaceCommands.InterfaceListOptions options =
        InterfaceCommands.InterfaceListOptions.builder()
            .includeMessages(includeMessages)
            .includeServices(includeServices)
            .includeActions(includeActions)
            .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.INTERFACE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.list(ctx, options));
  }

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

  private record TopicCommandTail(String messageType, List<String> flags) {}

  private static TopicCommandTail parseTopicCommandTailOrError(
      ServerCommandSource source, String commandName, String rawTail) {
    List<String> tokens = parseFlagsOrError(source, commandName, rawTail);
    if (tokens == null) {
      return null;
    }

    if (tokens.isEmpty()) {
      return new TopicCommandTail("", List.of());
    }

    String messageType = "";
    int flagStartIndex = 0;
    if (!tokens.get(0).startsWith("-")) {
      messageType = tokens.get(0);
      flagStartIndex = 1;
    }

    List<String> flags = flagStartIndex >= tokens.size()
        ? List.of()
        : List.copyOf(tokens.subList(flagStartIndex, tokens.size()));
    return new TopicCommandTail(messageType, flags);
  }

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
}
