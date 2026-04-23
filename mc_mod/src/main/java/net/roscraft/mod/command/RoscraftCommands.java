package net.roscraft.mod.command;

import com.mojang.brigadier.CommandDispatcher;
import net.fabricmc.fabric.api.command.v2.CommandRegistrationCallback;
import net.minecraft.server.command.ServerCommandSource;

public final class RoscraftCommands {

  private RoscraftCommands() {}

  public static void register() {
    CommandRegistrationCallback.EVENT.register(
        (dispatcher, registryAccess, environment) -> registerAll(dispatcher));
  }

  static void registerAll(CommandDispatcher<ServerCommandSource> dispatcher) {
    dispatcher.register(RoscraftCommandTree.buildRoot());
  }
}
