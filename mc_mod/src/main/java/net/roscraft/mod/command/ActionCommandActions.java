package net.roscraft.mod.command;

import java.nio.charset.StandardCharsets;
import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.command.action.ActionCommands;
import net.roscraft.mod.command.request.CommandRequestKind;

final class ActionCommandActions {

  private ActionCommandActions() {}

  static int executeActionList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros action list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        default -> {
          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros action list: " + flag);
          return 0;
        }
      }
    }

    ActionCommands.ActionListOptions options =
        ActionCommands.ActionListOptions.builder().showTypes(showTypes).build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.ACTION_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ActionCommands.list(ctx, options));
  }

  static int executeActionType(ServerCommandSource source, String actionName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.ACTION_TYPE,
        actionName,
        ctx -> ActionCommands.type(ctx, actionName));
  }

  static int executeActionInfo(ServerCommandSource source, String actionName, String rawFlags) {
    boolean includeHidden = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros action info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--include-hidden".equals(flag)) {
        includeHidden = true;
        continue;
      }
      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros action info: " + flag);
      return 0;
    }

    ActionCommands.ActionInfoOptions options =
        ActionCommands.ActionInfoOptions.builder().includeHidden(includeHidden).build();
    String trackingMetadata = "action_name=" + actionName + ";" + options.encodeTrackingMetadata();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.ACTION_INFO,
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

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros action send_goal", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "-f", "--feedback" -> feedback = true;
        case "-t", "--timeout" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for " + flag);
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
        }
        default -> {
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

          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros action send_goal: " + flag);
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
    byte[] payload = goalText.getBytes(StandardCharsets.UTF_8);
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.ACTION_SEND_GOAL,
        trackingMetadata,
        ctx -> ActionCommands.sendGoal(ctx, actionName, actionType, payload, options));
  }
}
