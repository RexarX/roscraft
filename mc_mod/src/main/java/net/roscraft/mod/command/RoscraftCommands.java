package net.roscraft.mod.command;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.Collections;
import java.util.List;
import net.fabricmc.fabric.api.command.v2.CommandRegistrationCallback;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.addon.AddonManager;

public final class RoscraftCommands {

  private RoscraftCommands() {}

  public static void register() {
    CommandRegistrationCallback.EVENT.register((dispatcher, registryAccess, environment) ->
        registerAll(dispatcher, Collections.emptyList()));
  }

  /**
   * Re-register commands after addon discovery.
   *
   * <p>
   * Called by {@link RoscraftMod} during initialisation to inject addon
   * sub-commands under {@code /ros}.
   */
  public static void registerWithAddons(AddonManager addonManager) {
    var addonCommands = addonManager != null
        ? addonManager.collectCommands()
        : Collections.<LiteralArgumentBuilder<ServerCommandSource>>emptyList();

    CommandRegistrationCallback.EVENT.register(
        (dispatcher, registryAccess, environment) -> registerAll(dispatcher, addonCommands));
  }

  static void registerAll(
      CommandDispatcher<ServerCommandSource> dispatcher,
      List<LiteralArgumentBuilder<ServerCommandSource>> addonCommands) {
    dispatcher.register(RoscraftCommandTree.buildRoot(addonCommands));
  }
}
