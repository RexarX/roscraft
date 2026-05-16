package net.roscraft.mod.command;

import java.nio.charset.StandardCharsets;
import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;
import net.roscraft.mod.command.topic.TopicCommands;

final class TopicCommandActions {

  private TopicCommandActions() {}

  static int executeSubscribe(ServerCommandSource source, String topic, String type) {
    TopicCommands.TopicEchoOptions options =
        TopicCommands.TopicEchoOptions.builder().build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.echo(ctx, topic, type, options));
  }

  static int executeTopicList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-topics" -> includeHidden = true;
        default -> {
          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic list: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicListOptions options = TopicCommands.TopicListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenTopics(includeHidden)
        .build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_LIST,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.list(ctx, options));
  }

  static int executeTopicType(ServerCommandSource source, String topicName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_TYPE,
        topicName,
        ctx -> TopicCommands.type(ctx, topicName));
  }

  static int executeTopicFind(ServerCommandSource source, String topicType) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_FIND,
        topicType,
        ctx -> TopicCommands.find(ctx, topicType));
  }

  static int executeTopicEcho(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic echo", rawFlags);
    if (flags == null) {
      return 0;
    }
    return executeTopicEchoWithFlags(source, topicName, messageType, flags);
  }

  static int executeTopicEchoStop(ServerCommandSource source, String topicName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO_STOP,
        topicName,
        ctx -> TopicCommands.echoStop(ctx, topicName));
  }

  static int executeTopicEchoStopAll(ServerCommandSource source) {
    return RoscraftCommandActions.executeLogicCommand(
        source, PendingRequestKind.TOPIC_ECHO_STOP, ctx -> TopicCommands.echoStopAll(ctx));
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
            RoscraftCommandActions.sendError(source, "Missing value for --timeout");
            return 0;
          }
          String timeoutToken = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(timeoutToken);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid timeout value: " + timeoutToken);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String timeoutToken = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(timeoutToken);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid timeout value: " + timeoutToken);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic echo: " + flag);
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

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        trackingMetadata,
        ctx -> TopicCommands.echo(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicInfo(ServerCommandSource source, String topicName, String rawFlags) {
    boolean verbose = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic info: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicInfoOptions options =
        TopicCommands.TopicInfoOptions.builder().verbose(verbose).build();
    return RoscraftCommandActions.executeLogicCommand(
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

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic pub", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--once", "-1" -> once = true;
        case "-r", "--rate" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for " + flag);
            return 0;
          }
          String rateToken = flags.get(++i);
          try {
            rateHz = Double.parseDouble(rateToken);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid rate value: " + rateToken);
            return 0;
          }
          if (rateHz < 0.0) {
            RoscraftCommandActions.sendError(source, "Rate must be non-negative.");
            return 0;
          }
        }
        case "-t", "--times" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for " + flag);
            return 0;
          }
          String timesToken = flags.get(++i);
          try {
            times = Integer.parseInt(timesToken);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid times value: " + timesToken);
            return 0;
          }
          if (times < 0) {
            RoscraftCommandActions.sendError(source, "Times must be non-negative.");
            return 0;
          }
        }
        case "--qos-profile" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for --qos-profile");
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
              RoscraftCommandActions.sendError(source, "Invalid rate value: " + rateToken);
              return 0;
            }
            if (rateHz < 0.0) {
              RoscraftCommandActions.sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--times=")) {
            String timesToken = flag.substring("--times=".length());
            try {
              times = Integer.parseInt(timesToken);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid times value: " + timesToken);
              return 0;
            }
            if (times < 0) {
              RoscraftCommandActions.sendError(source, "Times must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--qos-profile=")) {
            qosProfile = flag.substring("--qos-profile=".length());
            continue;
          }

          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic pub: " + flag);
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
    byte[] payload = payloadText.getBytes(StandardCharsets.UTF_8);
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_PUB,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.pub(ctx, topicName, messageType, payload, options));
  }

  static int executeTopicHz(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic hz", rawFlags);
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
            RoscraftCommandActions.sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            RoscraftCommandActions.sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              RoscraftCommandActions.sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic hz: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicHzOptions options =
        TopicCommands.TopicHzOptions.builder().window(window).wallTime(wallTime).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_HZ,
        trackingMetadata,
        ctx -> TopicCommands.hz(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicHzStop(ServerCommandSource source, String topicName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_HZ_STOP,
        topicName,
        ctx -> TopicCommands.hzStop(ctx, topicName));
  }

  static int executeTopicHzStopAll(ServerCommandSource source) {
    return RoscraftCommandActions.executeLogicCommand(
        source, PendingRequestKind.TOPIC_HZ_STOP, ctx -> TopicCommands.hzStopAll(ctx));
  }

  static int executeTopicBw(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic bw", rawFlags);
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
            RoscraftCommandActions.sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            RoscraftCommandActions.sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              RoscraftCommandActions.sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic bw: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicBwOptions options =
        TopicCommands.TopicBwOptions.builder().window(window).wallTime(wallTime).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_BW,
        trackingMetadata,
        ctx -> TopicCommands.bw(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicBwStop(ServerCommandSource source, String topicName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_BW_STOP,
        topicName,
        ctx -> TopicCommands.bwStop(ctx, topicName));
  }

  static int executeTopicBwStopAll(ServerCommandSource source) {
    return RoscraftCommandActions.executeLogicCommand(
        source, PendingRequestKind.TOPIC_BW_STOP, ctx -> TopicCommands.bwStopAll(ctx));
  }

  static int executeTopicDelay(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros topic delay", rawFlags);
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
          RoscraftCommandActions.sendError(source, "Missing value for --window");
          return 0;
        }
        String windowToken = flags.get(++i);
        try {
          window = Integer.parseInt(windowToken);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
          return 0;
        }
        if (window < 1) {
          RoscraftCommandActions.sendError(source, "Window must be >= 1.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--window=")) {
        String windowToken = flag.substring("--window=".length());
        try {
          window = Integer.parseInt(windowToken);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid window value: " + windowToken);
          return 0;
        }
        if (window < 1) {
          RoscraftCommandActions.sendError(source, "Window must be >= 1.");
          return 0;
        }
        continue;
      }

      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros topic delay: " + flag);
      return 0;
    }

    TopicCommands.TopicDelayOptions options =
        TopicCommands.TopicDelayOptions.builder().window(window).build();

    final String resolvedMessageType =
        (messageType == null || messageType.isBlank()) ? "" : messageType;

    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_DELAY,
        trackingMetadata,
        ctx -> TopicCommands.delay(ctx, topicName, resolvedMessageType, options));
  }

  static int executeTopicDelayStop(ServerCommandSource source, String topicName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_DELAY_STOP,
        topicName,
        ctx -> TopicCommands.delayStop(ctx, topicName));
  }

  static int executeTopicDelayStopAll(ServerCommandSource source) {
    return RoscraftCommandActions.executeLogicCommand(
        source, PendingRequestKind.TOPIC_DELAY_STOP, ctx -> TopicCommands.delayStopAll(ctx));
  }

  private record TopicCommandTail(String messageType, List<String> flags) {}

  private static TopicCommandTail parseTopicCommandTailOrError(
      ServerCommandSource source, String commandName, String rawTail) {
    List<String> tokens = RoscraftCommandActions.parseFlagsOrError(source, commandName, rawTail);
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
}
