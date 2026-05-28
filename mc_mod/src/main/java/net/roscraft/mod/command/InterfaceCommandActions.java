package net.roscraft.mod.command;

import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.command.interfaces.InterfaceCommands;
import net.roscraft.mod.command.request.CommandRequestKind;

final class InterfaceCommandActions {

  private InterfaceCommandActions() {}

  static int executeInterfaceShow(
      ServerCommandSource source, String interfaceType, String rawFlags) {
    boolean noComments = false;
    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros interface show", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      if ("--no-comments".equals(flag)) {
        noComments = true;
        continue;
      }
      RoscraftCommandActions.sendError(source, "Unsupported flag for /ros interface show: " + flag);
      return 0;
    }

    InterfaceCommands.InterfaceShowOptions options =
        InterfaceCommands.InterfaceShowOptions.builder().noComments(noComments).build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.INTERFACE_SHOW,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.show(ctx, interfaceType, options));
  }

  static int executeInterfaceList(ServerCommandSource source, String rawFlags) {
    boolean onlyMsgs = false;
    boolean onlySrvs = false;
    boolean onlyActions = false;

    List<String> flags =
        RoscraftCommandActions.parseFlagsOrError(source, "/ros interface list", rawFlags);
    if (flags == null) {
      return 0;
    }
    for (String flag : flags) {
      switch (flag) {
        case "-m", "--only-msgs" -> onlyMsgs = true;
        case "-s", "--only-srvs" -> onlySrvs = true;
        case "-a", "--only-actions" -> onlyActions = true;
        default -> {
          RoscraftCommandActions.sendError(
              source, "Unsupported flag for /ros interface list: " + flag);
          return 0;
        }
      }
    }

    int selected = (onlyMsgs ? 1 : 0) + (onlySrvs ? 1 : 0) + (onlyActions ? 1 : 0);
    if (selected > 1) {
      RoscraftCommandActions.sendError(
          source, "Choose at most one of -m, -s, or -a for /ros interface list.");
      return 0;
    }

    boolean includeMessages = !onlySrvs && !onlyActions;
    boolean includeServices = !onlyMsgs && !onlyActions;
    boolean includeActions = !onlyMsgs && !onlySrvs;

    InterfaceCommands.InterfaceListOptions options =
        InterfaceCommands.InterfaceListOptions.builder()
            .includeMessages(includeMessages)
            .includeServices(includeServices)
            .includeActions(includeActions)
            .build();
    return RoscraftCommandActions.executeLogicCommand(
        source,
        CommandRequestKind.INTERFACE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.list(ctx, options));
  }
}
