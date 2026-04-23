package net.roscraft.mod.command.service;

import java.util.Arrays;
import java.util.Objects;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.command.CommandContext;
import net.roscraft.mod.command.CommandResult;

/** Bridge-agnostic logic for `/ros service ...` commands. */
public final class ServiceCommands {

  private ServiceCommands() {}

  /** Options for `service list`. */
  public record ServiceListOptions(
      boolean showTypes, boolean countOnly, boolean includeHiddenServices) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return ((showTypes ? "show_types=1" : "show_types=0") + ";"
          + (countOnly ? "count_only=1" : "count_only=0")
          + ";"
          + (includeHiddenServices ? "include_hidden=1" : "include_hidden=0"));
    }

    /** Builder for {@link ServiceListOptions}. */
    public static final class Builder {

      private boolean showTypes;
      private boolean countOnly;
      private boolean includeHiddenServices;

      public Builder showTypes(boolean showTypes) {
        this.showTypes = showTypes;
        return this;
      }

      public Builder countOnly(boolean countOnly) {
        this.countOnly = countOnly;
        return this;
      }

      public Builder includeHiddenServices(boolean includeHiddenServices) {
        this.includeHiddenServices = includeHiddenServices;
        return this;
      }

      public ServiceListOptions build() {
        return new ServiceListOptions(showTypes, countOnly, includeHiddenServices);
      }
    }
  }

  /** Options for `service info`. */
  public record ServiceInfoOptions(boolean verbose) {
    public static Builder builder() {
      return new Builder();
    }

    public String encodeTrackingMetadata() {
      return verbose ? "verbose=1" : "verbose=0";
    }

    /** Builder for {@link ServiceInfoOptions}. */
    public static final class Builder {

      private boolean verbose;

      public Builder verbose(boolean verbose) {
        this.verbose = verbose;
        return this;
      }

      public ServiceInfoOptions build() {
        return new ServiceInfoOptions(verbose);
      }
    }
  }

  /** Options for `service call`. */
  public record ServiceCallOptions(double timeoutSeconds, int repeatCount, double rateHz) {
    public static Builder builder() {
      return new Builder();
    }

    public ServiceCallOptions {
      if (timeoutSeconds < 0.0) {
        throw new IllegalArgumentException("timeoutSeconds must be >= 0.0");
      }
      if (repeatCount < 0) {
        throw new IllegalArgumentException("repeatCount must be >= 0");
      }
      if (rateHz < 0.0) {
        throw new IllegalArgumentException("rateHz must be >= 0.0");
      }
    }

    public String encodeTrackingMetadata() {
      return ("timeout_seconds=" + timeoutSeconds
          + ";repeat_count="
          + repeatCount
          + ";rate_hz="
          + rateHz);
    }

    /** Builder for {@link ServiceCallOptions}. */
    public static final class Builder {

      private double timeoutSeconds;
      private int repeatCount;
      private double rateHz;

      public Builder timeoutSeconds(double timeoutSeconds) {
        this.timeoutSeconds = timeoutSeconds;
        return this;
      }

      public Builder repeatCount(int repeatCount) {
        this.repeatCount = repeatCount;
        return this;
      }

      public Builder rateHz(double rateHz) {
        this.rateHz = rateHz;
        return this;
      }

      public ServiceCallOptions build() {
        return new ServiceCallOptions(timeoutSeconds, repeatCount, rateHz);
      }
    }
  }

  /** Request service list (backed by graph query). */
  public static CommandResult list(CommandContext ctx, ServiceListOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    String suffix = options.showTypes() ? " (with types)" : "";
    return CommandResult.success(
        "Service list request #" + requestId + suffix + " sent.", requestId);
  }

  /** Resolve service type for a service name (backed by graph query). */
  public static CommandResult type(CommandContext ctx, String serviceName) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Service type request #" + requestId + " for " + serviceName + " sent.", requestId);
  }

  /** Find services by service type (backed by graph query). */
  public static CommandResult find(CommandContext ctx, String serviceType) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.queryGraph();
    return CommandResult.success(
        "Service find request #" + requestId + " for " + serviceType + " sent.", requestId);
  }

  /** Request detailed information for a service. */
  public static CommandResult info(
      CommandContext ctx, String serviceName, ServiceInfoOptions options) {
    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.serviceInfo(serviceName);
    String suffix = options.verbose() ? " (verbose)" : "";
    return CommandResult.success(
        "Service info request #" + requestId + suffix + " sent.", requestId);
  }

  /** Request `service call` execution with serialized request payload. */
  public static CommandResult call(
      CommandContext ctx,
      String serviceName,
      String serviceType,
      byte[] payload,
      ServiceCallOptions options) {
    Objects.requireNonNull(payload, "payload must not be null");
    if (!serviceType.contains("/")) {
      return CommandResult.failure("Invalid service type '" + serviceType
          + "'. Expected format: 'package/type'"
          + " (e.g., 'std_srvs/srv/Empty')");
    }

    RoscraftBridge bridge = ctx.requireBridge();
    long requestId = bridge.serviceCall(
        serviceName,
        serviceType,
        Arrays.copyOf(payload, payload.length),
        options.timeoutSeconds(),
        options.repeatCount(),
        options.rateHz());

    String repeats = options.repeatCount() > 0 ? String.valueOf(options.repeatCount()) : "1";
    String rate = options.rateHz() > 0.0 ? String.valueOf(options.rateHz()) : "default";
    return CommandResult.success(
        "Service call request #" + requestId
            + " for "
            + serviceName
            + " ("
            + serviceType
            + ") sent."
            + " [repeat="
            + repeats
            + ", rate="
            + rate
            + ", timeout="
            + options.timeoutSeconds()
            + "s]"
            + " Request preview="
            + Arrays.toString(Arrays.copyOf(payload, Math.min(payload.length, 8))),
        requestId);
  }
}
