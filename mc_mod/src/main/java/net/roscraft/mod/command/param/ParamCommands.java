package net.roscraft.mod.command.param;

import java.util.Objects;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros param ...` commands. */
public final class ParamCommands {

  private ParamCommands() {}

  /** Options for `param list`. */
  public record ParamListOptions(
      String nodeName, String[] prefixes, long depth, boolean includeTypes, String filterRegex) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamListOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      prefixes = prefixes == null ? new String[0] : prefixes.clone();
      if (depth < 0L) {
        throw new IllegalArgumentException("depth must be >= 0");
      }
      filterRegex = filterRegex == null ? "" : filterRegex;
    }

    public String encodeTrackingMetadata() {
      return ("node=" + nodeName
          + ";depth="
          + depth
          + ";include_types="
          + (includeTypes ? "1" : "0")
          + ";filter="
          + filterRegex);
    }

    /** Builder for {@link ParamListOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String[] prefixes = new String[0];
      private long depth;
      private boolean includeTypes;
      private String filterRegex = "";

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder prefixes(String[] prefixes) {
        this.prefixes = prefixes == null ? new String[0] : prefixes.clone();
        return this;
      }

      public Builder depth(long depth) {
        this.depth = depth;
        return this;
      }

      public Builder includeTypes(boolean includeTypes) {
        this.includeTypes = includeTypes;
        return this;
      }

      public Builder filterRegex(String filterRegex) {
        this.filterRegex = filterRegex;
        return this;
      }

      public ParamListOptions build() {
        return new ParamListOptions(nodeName, prefixes, depth, includeTypes, filterRegex);
      }
    }
  }

  /** Options for `param get`. */
  public record ParamGetOptions(String nodeName, String paramName, boolean hideType) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamGetOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
    }

    public String encodeTrackingMetadata() {
      return ("node=" + nodeName + ";param=" + paramName + ";hide_type=" + (hideType ? "1" : "0"));
    }

    /** Builder for {@link ParamGetOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String paramName = "";
      private boolean hideType;

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder paramName(String paramName) {
        this.paramName = paramName;
        return this;
      }

      public Builder hideType(boolean hideType) {
        this.hideType = hideType;
        return this;
      }

      public ParamGetOptions build() {
        return new ParamGetOptions(nodeName, paramName, hideType);
      }
    }
  }

  /** Options for `param set`. */
  public record ParamSetOptions(
      String nodeName, String paramName, String valueText, double timeoutSeconds) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamSetOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
      Objects.requireNonNull(valueText, "valueText must not be null");
      if (timeoutSeconds < 0.0) {
        throw new IllegalArgumentException("timeoutSeconds must be >= 0.0");
      }
    }

    public String encodeTrackingMetadata() {
      return ("node=" + nodeName + ";param=" + paramName + ";timeout_seconds=" + timeoutSeconds);
    }

    /** Builder for {@link ParamSetOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String paramName = "";
      private String valueText = "";
      private double timeoutSeconds;

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder paramName(String paramName) {
        this.paramName = paramName;
        return this;
      }

      public Builder valueText(String valueText) {
        this.valueText = valueText;
        return this;
      }

      public Builder timeoutSeconds(double timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
        return this;
      }

      public ParamSetOptions build() {
        return new ParamSetOptions(nodeName, paramName, valueText, timeoutSeconds);
      }
    }
  }

  /** Options for `param describe`. */
  public record ParamDescribeOptions(String nodeName, String paramName) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamDescribeOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(paramName, "paramName must not be null");
    }

    public String encodeTrackingMetadata() {
      return "node=" + nodeName + ";param=" + paramName;
    }

    /** Builder for {@link ParamDescribeOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String paramName = "";

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder paramName(String paramName) {
        this.paramName = paramName;
        return this;
      }

      public ParamDescribeOptions build() {
        return new ParamDescribeOptions(nodeName, paramName);
      }
    }
  }

  /** Options for `param dump`. */
  public record ParamDumpOptions(String nodeName, String[] prefixes) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamDumpOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      prefixes = prefixes == null ? new String[0] : prefixes.clone();
    }

    public String encodeTrackingMetadata() {
      return "node=" + nodeName + ";prefix_count=" + prefixes.length;
    }

    /** Builder for {@link ParamDumpOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String[] prefixes = new String[0];

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder prefixes(String[] prefixes) {
        this.prefixes = prefixes == null ? new String[0] : prefixes.clone();
        return this;
      }

      public ParamDumpOptions build() {
        return new ParamDumpOptions(nodeName, prefixes);
      }
    }
  }

  public static CommandResult list(CommandContext ctx, ParamListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Param list request #" + requestId + " for node '" + options.nodeName() + "' sent.",
        requestId);
  }

  public static CommandResult get(CommandContext ctx, ParamGetOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Param get request #" + requestId
            + " for "
            + options.nodeName()
            + "/"
            + options.paramName()
            + " sent.",
        requestId);
  }

  public static CommandResult set(CommandContext ctx, ParamSetOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Param set request #" + requestId
            + " for "
            + options.nodeName()
            + "/"
            + options.paramName()
            + " sent.",
        requestId);
  }

  public static CommandResult describe(CommandContext ctx, ParamDescribeOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Param describe request #" + requestId
            + " for "
            + options.nodeName()
            + "/"
            + options.paramName()
            + " sent.",
        requestId);
  }

  public static CommandResult dump(CommandContext ctx, ParamDumpOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Param dump request #" + requestId + " for node '" + options.nodeName() + "' sent.",
        requestId);
  }
}
