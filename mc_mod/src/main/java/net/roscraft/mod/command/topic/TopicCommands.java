package net.roscraft.mod.command.topic;

import java.util.Arrays;
import java.util.Objects;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros topic ...` commands. */
public final class TopicCommands {

  private TopicCommands() {}

  /** Options for `topic list`. */
  public record TopicListOptions(
      boolean showTypes, boolean countOnly, boolean includeHiddenTopics) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return ((showTypes ? "show_types=1" : "show_types=0") + ";"
          + (countOnly ? "count_only=1" : "count_only=0")
          + ";"
          + (includeHiddenTopics ? "include_hidden=1" : "include_hidden=0"));
    }

    /** Builder for {@link TopicListOptions}. */
    public static final class Builder {

      private boolean showTypes;
      private boolean countOnly;
      private boolean includeHiddenTopics;

      public Builder showTypes(boolean showTypes) {
        this.showTypes = showTypes;
        return this;
      }

      public Builder countOnly(boolean countOnly) {
        this.countOnly = countOnly;
        return this;
      }

      public Builder includeHiddenTopics(boolean includeHiddenTopics) {
        this.includeHiddenTopics = includeHiddenTopics;
        return this;
      }

      public TopicListOptions build() {
        return new TopicListOptions(showTypes, countOnly, includeHiddenTopics);
      }
    }
  }

  /** Options for `topic info`. */
  public record TopicInfoOptions(boolean verbose) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return verbose ? "verbose=1" : "verbose=0";
    }

    /** Builder for {@link TopicInfoOptions}. */
    public static final class Builder {

      private boolean verbose;

      public Builder verbose(boolean verbose) {
        this.verbose = verbose;
        return this;
      }

      public TopicInfoOptions build() {
        return new TopicInfoOptions(verbose);
      }
    }
  }

  /** Options for `topic echo`. */
  public record TopicEchoOptions(boolean once, double timeoutSeconds, boolean raw) {
    public static Builder builder() {
      return new Builder();
    }

    public TopicEchoOptions {
      if (timeoutSeconds < 0.0) {
        throw new IllegalArgumentException("timeoutSeconds must be >= 0.0");
      }
    }

    public String encodeTrackingMetadata() {
      return ((once ? "once=1" : "once=0") + ";timeout_seconds="
          + timeoutSeconds
          + ";"
          + (raw ? "raw=1" : "raw=0"));
    }

    /** Builder for {@link TopicEchoOptions}. */
    public static final class Builder {

      private boolean once;
      private double timeoutSeconds;
      private boolean raw;

      public Builder once(boolean once) {
        this.once = once;
        return this;
      }

      public Builder timeoutSeconds(double timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
        return this;
      }

      public Builder raw(boolean raw) {
        this.raw = raw;
        return this;
      }

      public TopicEchoOptions build() {
        return new TopicEchoOptions(once, timeoutSeconds, raw);
      }
    }
  }

  /** Options for `topic pub`. */
  public record TopicPublishOptions(boolean once, double rateHz, int times, String qosProfile) {
    public static Builder builder() {
      return new Builder();
    }

    public TopicPublishOptions {
      if (rateHz < 0.0) {
        throw new IllegalArgumentException("rateHz must be >= 0.0");
      }
      if (times < 0) {
        throw new IllegalArgumentException("times must be >= 0");
      }
      qosProfile = normalizeQosProfile(qosProfile);
    }

    public String encodeTrackingMetadata() {
      return ((once ? "once=1" : "once=0") + ";rate_hz="
          + rateHz
          + ";times="
          + times
          + ";qos_profile="
          + qosProfile);
    }

    /** Builder for {@link TopicPublishOptions}. */
    public static final class Builder {

      private boolean once;
      private double rateHz;
      private int times;
      private String qosProfile;

      public Builder once(boolean once) {
        this.once = once;
        return this;
      }

      public Builder rateHz(double rateHz) {
        this.rateHz = rateHz;
        return this;
      }

      public Builder times(int times) {
        this.times = times;
        return this;
      }

      public Builder qosProfile(String qosProfile) {
        this.qosProfile = qosProfile;
        return this;
      }

      public TopicPublishOptions build() {
        return new TopicPublishOptions(once, rateHz, times, qosProfile);
      }
    }
  }

  /** Options for `topic hz`. */
  public record TopicHzOptions(int window, boolean wallTime) {
    public static Builder builder() {
      return new Builder();
    }

    public TopicHzOptions {
      if (window < 1) {
        throw new IllegalArgumentException("window must be >= 1");
      }
    }

    public String encodeTrackingMetadata() {
      return ("window=" + window + ";" + (wallTime ? "wall_time=1" : "wall_time=0"));
    }

    /** Builder for {@link TopicHzOptions}. */
    public static final class Builder {

      private int window = 10000;
      private boolean wallTime;

      public Builder window(int window) {
        this.window = window;
        return this;
      }

      public Builder wallTime(boolean wallTime) {
        this.wallTime = wallTime;
        return this;
      }

      public TopicHzOptions build() {
        return new TopicHzOptions(window, wallTime);
      }
    }
  }

  /** Options for `topic bw`. */
  public record TopicBwOptions(int window, boolean wallTime) {
    public static Builder builder() {
      return new Builder();
    }

    public TopicBwOptions {
      if (window < 1) {
        throw new IllegalArgumentException("window must be >= 1");
      }
    }

    public String encodeTrackingMetadata() {
      return ("window=" + window + ";" + (wallTime ? "wall_time=1" : "wall_time=0"));
    }

    /** Builder for {@link TopicBwOptions}. */
    public static final class Builder {

      private int window = 100;
      private boolean wallTime;

      public Builder window(int window) {
        this.window = window;
        return this;
      }

      public Builder wallTime(boolean wallTime) {
        this.wallTime = wallTime;
        return this;
      }

      public TopicBwOptions build() {
        return new TopicBwOptions(window, wallTime);
      }
    }
  }

  /** Options for `topic delay`. */
  public record TopicDelayOptions(int window) {
    public static Builder builder() {
      return new Builder();
    }

    public TopicDelayOptions {
      if (window < 1) {
        throw new IllegalArgumentException("window must be >= 1");
      }
    }

    public String encodeTrackingMetadata() {
      return "window=" + window;
    }

    /** Builder for {@link TopicDelayOptions}. */
    public static final class Builder {

      private int window = 10;

      public Builder window(int window) {
        this.window = window;
        return this;
      }

      public TopicDelayOptions build() {
        return new TopicDelayOptions(window);
      }
    }
  }

  /** Request topic list (backed by graph query). */
  public static CommandResult list(CommandContext ctx, TopicListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    String suffix = options.showTypes() ? " (with types)" : "";
    return CommandResult.success("Topic list request #" + requestId + suffix + " sent.", requestId);
  }

  /** Resolve message type for a topic name (backed by graph query). */
  public static CommandResult type(CommandContext ctx, String topicName) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Topic type request #" + requestId + " for " + topicName + " sent.", requestId);
  }

  /** Find topics by message type (backed by graph query). */
  public static CommandResult find(CommandContext ctx, String topicType) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Topic find request #" + requestId + " for " + topicType + " sent.", requestId);
  }

  /** Subscribe to a topic and stream payloads. */
  public static CommandResult echo(CommandContext ctx, String topicName, String messageType) {
    return echo(ctx, topicName, messageType, TopicEchoOptions.builder().build());
  }

  /** Subscribe to a topic and stream payloads with echo options. */
  public static CommandResult echo(
      CommandContext ctx, String topicName, String messageType, TopicEchoOptions options) {
    if (messageType != null && !messageType.isBlank() && !messageType.contains("/")) {
      return CommandResult.failure("Invalid message type '" + messageType
          + "'. Expected format: 'package/type'"
          + " (e.g., 'std_msgs/msg/String')");
    }

    String resolvedMessageType = messageType == null ? "" : messageType;

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.subscribeTopic(
        topicName, resolvedMessageType, options.once(), options.timeoutSeconds(), options.raw());

    StringBuilder suffixBuilder = new StringBuilder();
    if (options.once()) {
      suffixBuilder.append(", once");
    }
    if (options.timeoutSeconds() > 0.0) {
      suffixBuilder.append(", timeout=").append(options.timeoutSeconds()).append("s");
    }
    if (options.raw()) {
      suffixBuilder.append(", raw");
    }

    return CommandResult.success(
        "Subscribed to "
            + topicName
            + " ("
            + (resolvedMessageType.isBlank() ? "auto" : resolvedMessageType)
            + ")"
            + suffixBuilder,
        requestId);
  }

  /** Request detailed information for a topic. */
  public static CommandResult info(CommandContext ctx, String topicName, TopicInfoOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.topicInfo(topicName);
    String suffix = options.verbose() ? " (verbose)" : "";
    return CommandResult.success("Topic info request #" + requestId + suffix + " sent.", requestId);
  }

  /** Publish a serialized message once or in a stream. */
  public static CommandResult pub(
      CommandContext ctx,
      String topicName,
      String messageType,
      byte[] payload,
      TopicPublishOptions options) {
    Objects.requireNonNull(payload, "payload must not be null");
    if (!isValidMessageType(messageType)) {
      return CommandResult.failure("Invalid message type '" + messageType
          + "'. Expected format: 'package/msg/Type'"
          + " (e.g., 'std_msgs/msg/String')");
    }

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.publishMessage(
        topicName,
        messageType,
        Arrays.copyOf(payload, payload.length),
        options.once(),
        options.rateHz(),
        options.times(),
        options.qosProfile());

    int requestedTimes = options.times();
    String repeatText =
        requestedTimes > 0 ? String.valueOf(requestedTimes) : (options.once() ? "1" : "inf");
    String rateText = options.rateHz() > 0.0 ? String.valueOf(options.rateHz()) : "default";

    return CommandResult.success(
        "Published to "
            + topicName
            + " ("
            + messageType
            + ") request #"
            + requestId
            + " [times="
            + repeatText
            + ", rate="
            + rateText
            + ", qos="
            + options.qosProfile()
            + "].",
        requestId);
  }

  /** Start topic rate measurement (`topic hz`). */
  public static CommandResult hz(
      CommandContext ctx, String topicName, String messageType, TopicHzOptions options) {
    if (topicName == null || topicName.isBlank()) {
      return CommandResult.failure("Topic name must be non-empty.");
    }
    if (messageType != null && !messageType.isBlank() && !messageType.contains("/")) {
      return CommandResult.failure("Invalid message type '" + messageType
          + "'. Expected format: 'package/type'"
          + " (e.g., 'std_msgs/msg/String')");
    }

    String resolvedMessageType = messageType == null ? "" : messageType;

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId =
        bridge.topicHz(topicName, resolvedMessageType, options.window(), options.wallTime());
    return CommandResult.success(
        "Topic hz request #" + requestId
            + " for "
            + topicName
            + " ("
            + (resolvedMessageType.isBlank() ? "auto" : resolvedMessageType)
            + ")"
            + " sent (window="
            + options.window()
            + ", wall_time="
            + options.wallTime()
            + ").",
        requestId);
  }

  /** Start topic bandwidth measurement (`topic bw`). */
  public static CommandResult bw(
      CommandContext ctx, String topicName, String messageType, TopicBwOptions options) {
    if (topicName == null || topicName.isBlank()) {
      return CommandResult.failure("Topic name must be non-empty.");
    }
    if (messageType != null && !messageType.isBlank() && !messageType.contains("/")) {
      return CommandResult.failure("Invalid message type '" + messageType
          + "'. Expected format: 'package/type'"
          + " (e.g., 'std_msgs/msg/String')");
    }

    String resolvedMessageType = messageType == null ? "" : messageType;

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId =
        bridge.topicBw(topicName, resolvedMessageType, options.window(), options.wallTime());
    return CommandResult.success(
        "Topic bw request #" + requestId
            + " for "
            + topicName
            + " ("
            + (resolvedMessageType.isBlank() ? "auto" : resolvedMessageType)
            + ")"
            + " sent (window="
            + options.window()
            + ", wall_time="
            + options.wallTime()
            + ").",
        requestId);
  }

  /** Start topic delay measurement (`topic delay`). */
  public static CommandResult delay(
      CommandContext ctx, String topicName, String messageType, TopicDelayOptions options) {
    if (topicName == null || topicName.isBlank()) {
      return CommandResult.failure("Topic name must be non-empty.");
    }
    if (messageType != null && !messageType.isBlank() && !messageType.contains("/")) {
      return CommandResult.failure("Invalid message type '" + messageType
          + "'. Expected format: 'package/type'"
          + " (e.g., 'std_msgs/msg/Header')");
    }

    String resolvedMessageType = messageType == null ? "" : messageType;

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.topicDelay(topicName, resolvedMessageType, options.window());
    return CommandResult.success(
        "Topic delay request #" + requestId
            + " for "
            + topicName
            + " ("
            + (resolvedMessageType.isBlank() ? "auto" : resolvedMessageType)
            + ")"
            + " sent (window="
            + options.window()
            + ").",
        requestId);
  }

  private static String normalizeQosProfile(String qosProfile) {
    if (qosProfile == null || qosProfile.isBlank()) {
      return "default";
    }
    return qosProfile.trim();
  }

  private static boolean isValidMessageType(String messageType) {
    if (messageType == null || messageType.isBlank()) {
      return false;
    }

    String[] parts = messageType.split("/");
    return parts.length == 3
        && !parts[0].isBlank()
        && "msg".equals(parts[1])
        && !parts[2].isBlank();
  }
}
