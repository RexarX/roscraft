package net.roscraft.mod.command.action;

import java.util.Arrays;
import java.util.Objects;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros action ...` commands. */
public final class ActionCommands {

  private ActionCommands() {}

  /** Options for `action list`. */
  public record ActionListOptions(boolean showTypes) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return showTypes ? "show_types=1" : "show_types=0";
    }

    /** Builder for {@link ActionListOptions}. */
    public static final class Builder {

      private boolean showTypes;

      public Builder showTypes(boolean showTypes) {
        this.showTypes = showTypes;
        return this;
      }

      public ActionListOptions build() {
        return new ActionListOptions(showTypes);
      }
    }
  }

  /** Options for `action info`. */
  public record ActionInfoOptions(boolean includeHidden) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return includeHidden ? "include_hidden=1" : "include_hidden=0";
    }

    /** Builder for {@link ActionInfoOptions}. */
    public static final class Builder {

      private boolean includeHidden;

      public Builder includeHidden(boolean includeHidden) {
        this.includeHidden = includeHidden;
        return this;
      }

      public ActionInfoOptions build() {
        return new ActionInfoOptions(includeHidden);
      }
    }
  }

  /** Options for `action send_goal`. */
  public record ActionSendGoalOptions(boolean feedback, double timeoutSeconds) {
    public static Builder builder() {
      return new Builder();
    }

    public ActionSendGoalOptions {
      if (timeoutSeconds < 0.0) {
        throw new IllegalArgumentException("timeoutSeconds must be >= 0.0");
      }
    }

    public String encodeTrackingMetadata() {
      return ((feedback ? "feedback=1" : "feedback=0") + ";timeout_seconds=" + timeoutSeconds);
    }

    /** Builder for {@link ActionSendGoalOptions}. */
    public static final class Builder {

      private boolean feedback;
      private double timeoutSeconds;

      public Builder feedback(boolean feedback) {
        this.feedback = feedback;
        return this;
      }

      public Builder timeoutSeconds(double timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
        return this;
      }

      public ActionSendGoalOptions build() {
        return new ActionSendGoalOptions(feedback, timeoutSeconds);
      }
    }
  }

  /** Request action list (backed by graph query). */
  public static CommandResult list(CommandContext ctx, ActionListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.graph().snapshot();
    String suffix = options.showTypes() ? " (with types)" : "";
    return CommandResult.success(
        "Action list request #" + requestId + suffix + " sent.", requestId);
  }

  /** Request action type for a specific action name (backed by graph query). */
  public static CommandResult type(CommandContext ctx, String actionName) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.graph().snapshot();
    return CommandResult.success(
        "Action type request #" + requestId + " for " + actionName + " sent.", requestId);
  }

  /** Request detailed action information (backed by graph query). */
  public static CommandResult info(CommandContext ctx, String actionName) {
    return info(ctx, actionName, ActionInfoOptions.builder().build());
  }

  /** Request detailed action information (backed by graph query). */
  public static CommandResult info(
      CommandContext ctx, String actionName, ActionInfoOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.actions().info(actionName, options.includeHidden());
    String suffix = options.includeHidden() ? " (include hidden)" : "";
    return CommandResult.success(
        "Action info request #" + requestId + " for " + actionName + suffix + " sent.", requestId);
  }

  /** Request action goal execution (typed registry path). */
  public static CommandResult sendGoal(
      CommandContext ctx,
      String actionName,
      String actionType,
      byte[] goalPayload,
      ActionSendGoalOptions options) {
    Objects.requireNonNull(goalPayload, "goalPayload must not be null");
    if (!isValidActionType(actionType)) {
      return CommandResult.failure("Invalid action type '" + actionType
          + "'. Expected format: 'package/action/Type'"
          + " (e.g., 'example_interfaces/action/Fibonacci')");
    }

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge
        .actions()
        .sendGoal(
            actionName,
            actionType,
            Arrays.copyOf(goalPayload, goalPayload.length),
            new BridgeOperations.ActionOps.ActionGoalOptions(
                options.feedback(), options.timeoutSeconds()));
    return CommandResult.success(
        "Action send_goal request #" + requestId
            + " for "
            + actionName
            + " ("
            + actionType
            + ") sent."
            + " [feedback="
            + options.feedback()
            + ", timeout="
            + options.timeoutSeconds()
            + "s]"
            + " Goal preview="
            + Arrays.toString(Arrays.copyOf(goalPayload, Math.min(goalPayload.length, 8))),
        requestId);
  }

  private static boolean isValidActionType(String actionType) {
    if (actionType == null || actionType.isBlank()) {
      return false;
    }

    String[] parts = actionType.split("/");
    return parts.length == 3
        && !parts[0].isBlank()
        && "action".equals(parts[1])
        && !parts[2].isBlank();
  }
}
