package net.roscraft.mod.command.interfaces;

import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros interface ...` commands. */
public final class InterfaceCommands {

  private InterfaceCommands() {}

  /** Options for `interface show`. */
  public record InterfaceShowOptions(boolean noComments) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return noComments ? "no_comments=1" : "no_comments=0";
    }

    /** Builder for {@link InterfaceShowOptions}. */
    public static final class Builder {

      private boolean noComments;

      public Builder noComments(boolean noComments) {
        this.noComments = noComments;
        return this;
      }

      public InterfaceShowOptions build() {
        return new InterfaceShowOptions(noComments);
      }
    }
  }

  /** Options for `interface list`. */
  public record InterfaceListOptions(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return ((includeMessages ? "include_messages=1" : "include_messages=0") + ";"
          + (includeServices ? "include_services=1" : "include_services=0")
          + ";"
          + (includeActions ? "include_actions=1" : "include_actions=0"));
    }

    /** Builder for {@link InterfaceListOptions}. */
    public static final class Builder {

      private boolean includeMessages = true;
      private boolean includeServices = true;
      private boolean includeActions = true;

      public Builder includeMessages(boolean includeMessages) {
        this.includeMessages = includeMessages;
        return this;
      }

      public Builder includeServices(boolean includeServices) {
        this.includeServices = includeServices;
        return this;
      }

      public Builder includeActions(boolean includeActions) {
        this.includeActions = includeActions;
        return this;
      }

      public InterfaceListOptions build() {
        return new InterfaceListOptions(includeMessages, includeServices, includeActions);
      }
    }
  }

  /** Request interface definition text. */
  public static CommandResult show(
      CommandContext ctx, String interfaceType, InterfaceShowOptions options) {
    return show(ctx, interfaceType);
  }

  /** Request interface definition text. */
  public static CommandResult show(CommandContext ctx, String interfaceType) {
    if (interfaceType.chars().filter(ch -> ch == '/').count() < 2) {
      return CommandResult.failure("Invalid interface type '" + interfaceType
          + "'. Expected format: package/kind/name"
          + " (e.g., std_msgs/msg/String)");
    }

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.interfaceShow(interfaceType);
    return CommandResult.success("Interface show request #" + requestId + " sent.", requestId);
  }

  /** Request available interface types grouped by kind. */
  public static CommandResult list(CommandContext ctx, InterfaceListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.interfaceList(
        options.includeMessages(), options.includeServices(), options.includeActions());
    return CommandResult.success("Interface list request #" + requestId + " sent.", requestId);
  }
}
