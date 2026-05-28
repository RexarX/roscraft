package net.roscraft.mod;

import java.util.UUID;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.mod.addon.AddonManager;
import net.roscraft.mod.bridge.BridgeManager;
import net.roscraft.mod.bridge.BridgeRequestHub;
import net.roscraft.mod.bridge.callback.CommandBridgeEventRouter;
import net.roscraft.mod.command.RoscraftCommands;
import net.roscraft.mod.command.request.CommandRequestKind;
import net.roscraft.mod.command.request.CommandRequestTracker;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fabric mod initialiser for Roscraft.
 */
public final class RoscraftMod implements ModInitializer {

  public static final String MOD_ID = "roscraft";
  public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

  private static RoscraftMod instance;

  private BridgeManager bridgeManager;
  private BridgeRequestHub requestHub;
  private CommandRequestTracker commandRequests;
  private AddonManager addonManager;
  private CommandBridgeEventRouter commandEvents;

  private MinecraftServer server;

  public static RoscraftMod getInstance() {
    return instance;
  }

  @Override
  public void onInitialize() {
    instance = this;
    LOGGER.info("Roscraft initialising...");

    var config = RoscraftConfig.load();
    commandRequests = new CommandRequestTracker(this);
    requestHub = new BridgeRequestHub(commandRequests);
    commandEvents = new CommandBridgeEventRouter(this, commandRequests);
    addonManager = new AddonManager(this, requestHub);

    bridgeManager = new BridgeManager(config, new ModBridgeCallback(this));
    bridgeManager.setPreDisconnectHook(this::stopRunningSessions);
    addonManager.loadAddons();

    RoscraftCommands.registerWithAddons(addonManager);

    ServerTickEvents.END_SERVER_TICK.register(server -> onServerTick());
    ServerLifecycleEvents.SERVER_STARTED.register(server -> this.server = server);
    ServerLifecycleEvents.SERVER_STOPPING.register(server -> {
      LOGGER.info("Roscraft shutting down...");
      this.server = null;
      addonManager.shutdown();
      bridgeManager.close();
    });

    LOGGER.info(
        "Roscraft initialised (selected mode: {}, JNI available: {})",
        bridgeManager.selectedBridgeType(),
        bridgeManager.isJniAvailable());
  }

  public BridgeManager bridgeManager() {
    return bridgeManager;
  }

  public AddonManager addonManager() {
    return addonManager;
  }

  public BridgeRequestHub requestHub() {
    return requestHub;
  }

  public CommandRequestTracker commandRequests() {
    return commandRequests;
  }

  public CommandBridgeEventRouter commandEvents() {
    return commandEvents;
  }

  public void trackRequest(long requestId, CommandRequestKind kind, UUID requesterUuid) {
    commandRequests.track(requestId, kind, requesterUuid);
  }

  public void trackRequest(
      long requestId, CommandRequestKind kind, UUID requesterUuid, String metadata) {
    commandRequests.track(requestId, kind, requesterUuid, metadata);
  }

  private void onServerTick() {
    bridgeManager.tick();
    commandRequests.processTimeouts();
  }

  private void stopRunningSessions() {
    if (bridgeManager == null) {
      return;
    }
    commandRequests.stopRunningSessions(bridgeManager.getBridge());
  }

  public void sendToRequesterOrOperators(UUID requesterUuid, Text message) {
    if (requesterUuid != null) {
      ServerPlayerEntity player = findPlayer(requesterUuid);
      if (player != null) {
        player.sendMessage(message.copy(), false);
        return;
      }
    }
    broadcastToOperators(message);
  }

  private ServerPlayerEntity findPlayer(UUID requesterUuid) {
    var currentServer = server;
    if (currentServer == null) {
      return null;
    }
    var playerManager = currentServer.getPlayerManager();
    if (playerManager == null) {
      return null;
    }
    return playerManager.getPlayer(requesterUuid);
  }

  private void broadcastToOperators(Text message) {
    var currentServer = server;
    if (currentServer == null) {
      return;
    }
    var playerManager = currentServer.getPlayerManager();
    if (playerManager == null) {
      return;
    }
    MutableText copy = message.copy();
    playerManager.getPlayerList().forEach(player -> {
      if (player.hasPermissionLevel(2)) {
        player.sendMessage(copy.copy(), false);
      }
    });
  }

  public static MutableText prefix() {
    return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
  }
}
