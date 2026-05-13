package net.roscraft.mod.command;

import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;
import net.roscraft.mod.command.param.ParamCommands;

final class ParamCommandActions {

  private ParamCommandActions() {}

  static int executeParamList(ServerCommandSource source, String nodeName, String rawFlags) {
    long depth = 0L;
    boolean includeTypes = false;
    String filterRegex = "";
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros param list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--depth" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for --depth");
            return 0;
          }
          String token = flags.get(++i);
          try {
            depth = Long.parseLong(token);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid depth value: " + token);
            return 0;
          }
          if (depth < 0L) {
            RoscraftCommandActions.sendError(source, "Depth must be non-negative.");
            return 0;
          }
        }
        case "--param-type" -> includeTypes = true;
        case "--filter" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for --filter");
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
              RoscraftCommandActions.sendError(source, "Invalid depth value: " + token);
              return 0;
            }
            if (depth < 0L) {
              RoscraftCommandActions.sendError(source, "Depth must be non-negative.");
              return 0;
            }
            continue;
          }
          if (flag.startsWith("--filter=")) {
            filterRegex = flag.substring("--filter=".length());
            continue;
          }

          RoscraftCommandActions.sendError(source, "Unsupported flag for /ros param list: " + flag);
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
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.PARAM_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.list(ctx, options));
  }

  static int executeParamGet(
      ServerCommandSource source, String nodeName, String paramName, String rawFlags) {
    boolean hideType = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros param get", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--hide-type".equals(flag)) {
        hideType = true;
        continue;
      }
      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros param get: " + flag);
      return 0;
    }

    ParamCommands.ParamGetOptions options = ParamCommands.ParamGetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .hideType(hideType)
        .build();
    return RoscraftCommandActions.executeLogicCommand(
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

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros param set", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--timeout".equals(flag)) {
        if (i + 1 >= flags.size()) {
          RoscraftCommandActions.sendError(source, "Missing value for --timeout");
          return 0;
        }
        String token = flags.get(++i);
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--timeout=")) {
        String token = flag.substring("--timeout=".length());
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros param set: " + flag);
      return 0;
    }

    ParamCommands.ParamSetOptions options = ParamCommands.ParamSetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .valueText(valueText)
        .timeoutSeconds(timeoutSeconds)
        .build();
    return RoscraftCommandActions.executeLogicCommand(
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
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DESCRIBE,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.describe(ctx, options));
  }

  static int executeParamDump(ServerCommandSource source, String nodeName, String rawFlags) {
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros param dump", rawFlags);
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
      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros param dump: " + flag);
      return 0;
    }

    ParamCommands.ParamDumpOptions options = ParamCommands.ParamDumpOptions.builder()
        .nodeName(nodeName)
        .prefixes(prefixes.toArray(String[]::new))
        .build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DUMP,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.dump(ctx, options));
  }

  static int executeParamLoad(
      ServerCommandSource source, String nodeName, String parameterFile, String rawFlags) {
    double timeoutSeconds = 0.0;
    boolean useWildcard = true;

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros param load", rawFlags);
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
          RoscraftCommandActions.sendError(source, "Missing value for --timeout");
          return 0;
        }
        String token = flags.get(++i);
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--timeout=")) {
        String token = flag.substring("--timeout=".length());
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          RoscraftCommandActions.sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          RoscraftCommandActions.sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros param load: " + flag);
      return 0;
    }

    ParamCommands.ParamLoadOptions options = ParamCommands.ParamLoadOptions.builder()
        .nodeName(nodeName)
        .parameterFile(parameterFile)
        .timeoutSeconds(timeoutSeconds)
        .useWildcard(useWildcard)
        .build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.PARAM_LOAD,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.load(ctx, options));
  }
}
