package net.roscraft.mod.command.node;

import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros node ...` commands. */
public final class NodeCommands {

  private NodeCommands() {}

  /** Options for `node list`. */
  public record NodeListOptions(boolean includeHidden, boolean countOnly) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return ((includeHidden ? "include_hidden=1" : "include_hidden=0") + ";"
          + (countOnly ? "count_only=1" : "count_only=0"));
    }

    /** Builder for {@link NodeListOptions}. */
    public static final class Builder {

      private boolean includeHidden;
      private boolean countOnly;

      public Builder includeHidden(boolean includeHidden) {
        this.includeHidden = includeHidden;
        return this;
      }

      public Builder countOnly(boolean countOnly) {
        this.countOnly = countOnly;
        return this;
      }

      public NodeListOptions build() {
        return new NodeListOptions(includeHidden, countOnly);
      }
    }
  }

  /** Options for `node info`. */
  public record NodeInfoOptions(boolean includeHidden) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return includeHidden ? "include_hidden=1" : "include_hidden=0";
    }

    /** Builder for {@link NodeInfoOptions}. */
    public static final class Builder {

      private boolean includeHidden;

      public Builder includeHidden(boolean includeHidden) {
        this.includeHidden = includeHidden;
        return this;
      }

      public NodeInfoOptions build() {
        return new NodeInfoOptions(includeHidden);
      }
    }
  }

  /** Request node list (backed by graph query). */
  public static CommandResult list(CommandContext ctx, NodeListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.graph().snapshot();

    StringBuilder detail = new StringBuilder();
    if (options.includeHidden()) {
      detail.append(" include-hidden");
    }
    if (options.countOnly()) {
      detail.append(" count-only");
    }

    return CommandResult.success("Node list request #" + requestId + detail + " sent.", requestId);
  }

  /** Request detailed information for a node. */
  public static CommandResult info(CommandContext ctx, String nodeName, NodeInfoOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.graph().nodeInfo(nodeName, options.includeHidden());
    return CommandResult.success("Node info request #" + requestId + " sent.", requestId);
  }
}
