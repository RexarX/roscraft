package net.roscraft.mod.command;

import java.nio.charset.StandardCharsets;
import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;
import net.roscraft.mod.command.service.ServiceCommands;

final class ServiceCommandActions {

  private ServiceCommandActions() {}

  static int executeServiceList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros service list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-services" -> includeHidden = true;
        default -> {
          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros service list: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceListOptions options = ServiceCommands.ServiceListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenServices(includeHidden)
        .build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.list(ctx, options));
  }

  static int executeServiceType(ServerCommandSource source, String serviceName) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_TYPE,
        serviceName,
        ctx -> ServiceCommands.type(ctx, serviceName));
  }

  static int executeServiceFind(ServerCommandSource source, String serviceType) {
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_FIND,
        serviceType,
        ctx -> ServiceCommands.find(ctx, serviceType));
  }

  static int executeServiceInfo(ServerCommandSource source, String serviceName, String rawFlags) {
    boolean verbose = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros service info", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros service info: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceInfoOptions options =
        ServiceCommands.ServiceInfoOptions.builder().verbose(verbose).build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_INFO,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.info(ctx, serviceName, options));
  }

  static int executeServiceCall(
      ServerCommandSource source,
      String serviceName,
      String serviceType,
      String requestText,
      String rawFlags) {
    double timeoutSeconds = 0.0;
    int repeatCount = 0;
    double rateHz = 0.0;

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros service call", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--timeout" -> {
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
        }
        case "-r", "--repeat" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for " + flag);
            return 0;
          }
          String token = flags.get(++i);
          try {
            repeatCount = Integer.parseInt(token);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid repeat value: " + token);
            return 0;
          }
          if (repeatCount < 0) {
            RoscraftCommandActions.sendError(source, "Repeat must be non-negative.");
            return 0;
          }
        }
        case "--rate" -> {
          if (i + 1 >= flags.size()) {
            RoscraftCommandActions.sendError(source, "Missing value for --rate");
            return 0;
          }
          String token = flags.get(++i);
          try {
            rateHz = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            RoscraftCommandActions.sendError(source, "Invalid rate value: " + token);
            return 0;
          }
          if (rateHz < 0.0) {
            RoscraftCommandActions.sendError(source, "Rate must be non-negative.");
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

          if (flag.startsWith("--repeat=")) {
            String token = flag.substring("--repeat=".length());
            try {
              repeatCount = Integer.parseInt(token);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid repeat value: " + token);
              return 0;
            }
            if (repeatCount < 0) {
              RoscraftCommandActions.sendError(source, "Repeat must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--rate=")) {
            String token = flag.substring("--rate=".length());
            try {
              rateHz = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              RoscraftCommandActions.sendError(source, "Invalid rate value: " + token);
              return 0;
            }
            if (rateHz < 0.0) {
              RoscraftCommandActions.sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros service call: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceCallOptions options = ServiceCommands.ServiceCallOptions.builder()
        .timeoutSeconds(timeoutSeconds)
        .repeatCount(repeatCount)
        .rateHz(rateHz)
        .build();
    String trackingMetadata = "service_name=" + serviceName
        + ";service_type="
        + serviceType
        + ";"
        + options.encodeTrackingMetadata();
    byte[] payload = requestText.getBytes(StandardCharsets.UTF_8);
    return RoscraftCommandActions.executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_CALL,
        trackingMetadata,
        ctx -> ServiceCommands.call(ctx, serviceName, serviceType, payload, options));
  }
}
