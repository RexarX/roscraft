package net.roscraft.mod.command.param;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.util.Objects;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros param ...` commands. */
public final class ParamCommands {

  private static final double DEFAULT_TIMEOUT_SECONDS = 0.0;

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

  /** Options for `param load`. */
  public record ParamLoadOptions(
      String nodeName, String parameterFile, double timeoutSeconds, boolean useWildcard) {
    public static Builder builder() {
      return new Builder();
    }

    public ParamLoadOptions {
      Objects.requireNonNull(nodeName, "nodeName must not be null");
      Objects.requireNonNull(parameterFile, "parameterFile must not be null");
      if (parameterFile.isBlank()) {
        throw new IllegalArgumentException("parameterFile must not be blank");
      }
      if (timeoutSeconds < 0.0) {
        throw new IllegalArgumentException("timeoutSeconds must be >= 0.0");
      }
    }

    public String encodeTrackingMetadata() {
      return "node="
          + nodeName
          + ";timeout_seconds="
          + timeoutSeconds
          + ";use_wildcard="
          + (useWildcard ? "1" : "0")
          + ";parameter_file="
          + parameterFile;
    }

    /** Builder for {@link ParamLoadOptions}. */
    public static final class Builder {

      private String nodeName = "";
      private String parameterFile = "";
      private double timeoutSeconds;
      private boolean useWildcard = true;

      public Builder nodeName(String nodeName) {
        this.nodeName = nodeName;
        return this;
      }

      public Builder parameterFile(String parameterFile) {
        this.parameterFile = parameterFile;
        return this;
      }

      public Builder timeoutSeconds(double timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
        return this;
      }

      public Builder useWildcard(boolean useWildcard) {
        this.useWildcard = useWildcard;
        return this;
      }

      public ParamLoadOptions build() {
        return new ParamLoadOptions(nodeName, parameterFile, timeoutSeconds, useWildcard);
      }
    }
  }

  public static CommandResult list(CommandContext ctx, ParamListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.paramList(
        options.nodeName(),
        options.prefixes(),
        Math.toIntExact(options.depth()),
        options.includeTypes(),
        options.filterRegex(),
        DEFAULT_TIMEOUT_SECONDS);
    return CommandResult.success(
        "Param list request #" + requestId + " for node '" + options.nodeName() + "' sent.",
        requestId);
  }

  public static CommandResult get(CommandContext ctx, ParamGetOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.paramGet(
        options.nodeName(), options.paramName(), options.hideType(), DEFAULT_TIMEOUT_SECONDS);
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
    long requestId = bridge.paramSet(
        options.nodeName(), options.paramName(), options.valueText(), options.timeoutSeconds());
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
    long requestId =
        bridge.paramDescribe(options.nodeName(), options.paramName(), DEFAULT_TIMEOUT_SECONDS);
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
    long requestId =
        bridge.paramDump(options.nodeName(), options.prefixes(), DEFAULT_TIMEOUT_SECONDS);
    return CommandResult.success(
        "Param dump request #" + requestId + " for node '" + options.nodeName() + "' sent.",
        requestId);
  }

  public static CommandResult load(CommandContext ctx, ParamLoadOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();

    final Path parameterFilePath;
    try {
      parameterFilePath = Path.of(options.parameterFile());
    } catch (InvalidPathException ex) {
      return CommandResult.failure(
          "Param load failed: invalid parameter file path '" + options.parameterFile() + "'.");
    }

    final String yamlText;
    try {
      yamlText = Files.readString(parameterFilePath);
    } catch (IOException ex) {
      return CommandResult.failure("Param load failed: unable to read parameter file '"
          + options.parameterFile()
          + "': "
          + ex.getMessage());
    }

    if (yamlText.isBlank()) {
      return CommandResult.failure(
          "Param load failed: parameter file '" + options.parameterFile() + "' is empty.");
    }

    long requestId = bridge.paramLoad(
        options.nodeName(), yamlText, options.timeoutSeconds(), options.useWildcard());
    return CommandResult.success(
        "Param load request #"
            + requestId
            + " for node '"
            + options.nodeName()
            + "' from file '"
            + options.parameterFile()
            + "' sent.",
        requestId);
  }
}
