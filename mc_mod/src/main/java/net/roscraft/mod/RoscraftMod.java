package net.roscraft.mod;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
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
import net.roscraft.mod.command.RoscraftCommands;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fabric mod initialiser for Roscraft.
 *
 * <p>
 * Responsible for:
 * <ul>
 * <li>Creating and configuring the active bridge via
 * {@link BridgeManager}.</li>
 * <li>Registering Fabric lifecycle and tick event listeners.</li>
 * <li>Registering mod commands.</li>
 * </ul>
 */
public final class RoscraftMod implements ModInitializer {

  public static final String MOD_ID = "roscraft";
  public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

  private static final long REQUEST_TIMEOUT_MILLIS = 5_000L;

  /**
   * Runtime bridge lifecycle/controller, initialised in {@link #onInitialize}.
   */
  private BridgeManager bridgeManager;

  private AddonManager addonManager;

  private MinecraftServer server;
  private final Map<Long, PendingRequest> pendingRequests = new HashMap<>();

  public enum PendingRequestKind {
    PLAYERS,
    CONNECTION_CHECK,
    NODE_LIST,
    NODE_INFO,
    TOPIC_LIST,
    TOPIC_TYPE,
    TOPIC_FIND,
    TOPIC_ECHO,
    TOPIC_ECHO_STOP,
    TOPIC_PUB,
    TOPIC_HZ,
    TOPIC_HZ_STOP,
    TOPIC_BW,
    TOPIC_BW_STOP,
    TOPIC_DELAY,
    TOPIC_DELAY_STOP,
    TOPIC_INFO,
    SERVICE_LIST,
    SERVICE_TYPE,
    SERVICE_FIND,
    SERVICE_INFO,
    SERVICE_CALL,
    ACTION_LIST,
    ACTION_TYPE,
    ACTION_INFO,
    ACTION_SEND_GOAL,
    PARAM_LIST,
    PARAM_GET,
    PARAM_SET,
    PARAM_DESCRIBE,
    PARAM_DUMP,
    PARAM_LOAD,
    INTERFACE_LIST,
    INTERFACE_SHOW,
  }

  record PendingRequest(
      PendingRequestKind kind, UUID requesterUuid, long createdAtMillis, String metadata) {}

  @Override
  public void onInitialize() {
    LOGGER.info("Roscraft initialising...");

    var config = RoscraftConfig.load();
    bridgeManager = new BridgeManager(config, new ModBridgeCallback(this));
    bridgeManager.setPreDisconnectHook(this::stopRunningSessions);

    addonManager = new AddonManager(this);
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

  /**
   * Returns the bridge manager. May be {@code null} before initialisation
   * completes.
   */
  public BridgeManager bridgeManager() {
    return bridgeManager;
  }

  public AddonManager addonManager() {
    return addonManager;
  }

  public synchronized void trackRequest(
      long requestId, PendingRequestKind kind, UUID requesterUuid) {
    trackRequest(requestId, kind, requesterUuid, null);
  }

  public synchronized void trackRequest(
      long requestId, PendingRequestKind kind, UUID requesterUuid, String metadata) {
    pendingRequests.put(
        requestId, new PendingRequest(kind, requesterUuid, System.currentTimeMillis(), metadata));
  }

  synchronized PendingRequest completeRequest(long requestId) {
    return pendingRequests.remove(requestId);
  }

  synchronized PendingRequest pendingRequest(long requestId) {
    return pendingRequests.get(requestId);
  }

  private void onServerTick() {
    bridgeManager.tick();
    processTimedOutRequests();
  }

  synchronized void stopRunningSessions() {
    if (bridgeManager == null) {
      return;
    }

    var activeBridge = bridgeManager.getBridge();
    if (activeBridge == null) {
      return;
    }

    List<Map.Entry<Long, PendingRequest>> toStop = new ArrayList<>();
    for (var entry : pendingRequests.entrySet()) {
      PendingRequest pending = entry.getValue();
      switch (pending.kind()) {
        case TOPIC_HZ, TOPIC_BW, TOPIC_DELAY, TOPIC_ECHO -> {
          if (pending.metadata() != null) {
            toStop.add(entry);
          }
        }
        default -> {}
      }
    }

    for (var entry : toStop) {
      long requestId = entry.getKey();
      PendingRequest pending = entry.getValue();
      String topicName = extractTopicName(pending.metadata());
      if (topicName == null || topicName.isBlank()) {
        pendingRequests.remove(requestId);
        continue;
      }

      try {
        switch (pending.kind()) {
          case TOPIC_HZ -> activeBridge.topics().hz(topicName, "", 0);
          case TOPIC_BW -> activeBridge.topics().bw(topicName, "", 0);
          case TOPIC_DELAY -> activeBridge.topics().delay(topicName, "", 0);
          case TOPIC_ECHO -> activeBridge.topics().unsubscribe(topicName);
          default -> {}
        }
      } catch (RuntimeException e) {
        LOGGER.warn(
            "Failed to stop running session #{} for {}: {}", requestId, topicName, e.getMessage());
      }

      pendingRequests.remove(requestId);
    }
  }

  private static String extractTopicName(String metadata) {
    if (metadata == null || metadata.isBlank()) {
      return null;
    }
    if (metadata.startsWith("topic_name=")) {
      int semicolonIdx = metadata.indexOf(';', "topic_name=".length());
      if (semicolonIdx < 0) {
        return metadata.substring("topic_name=".length());
      }
      return metadata.substring("topic_name=".length(), semicolonIdx);
    }
    return metadata;
  }

  private void processTimedOutRequests() {
    long now = System.currentTimeMillis();
    List<Map.Entry<Long, PendingRequest>> timedOut = new ArrayList<>();

    synchronized (this) {
      var iterator = pendingRequests.entrySet().iterator();
      while (iterator.hasNext()) {
        var entry = iterator.next();
        PendingRequest pending = entry.getValue();
        if (pending.kind() == PendingRequestKind.TOPIC_ECHO
            || pending.kind() == PendingRequestKind.TOPIC_PUB
            || pending.kind() == PendingRequestKind.TOPIC_ECHO_STOP
            || pending.kind() == PendingRequestKind.TOPIC_HZ
            || pending.kind() == PendingRequestKind.TOPIC_HZ_STOP
            || pending.kind() == PendingRequestKind.TOPIC_BW
            || pending.kind() == PendingRequestKind.TOPIC_BW_STOP
            || pending.kind() == PendingRequestKind.TOPIC_DELAY
            || pending.kind() == PendingRequestKind.TOPIC_DELAY_STOP) {
          continue;
        }
        if (now - pending.createdAtMillis() >= REQUEST_TIMEOUT_MILLIS) {
          timedOut.add(Map.entry(entry.getKey(), pending));
          iterator.remove();
        }
      }
    }

    for (var entry : timedOut) {
      long requestId = entry.getKey();
      PendingRequest pending = entry.getValue();

      String kindName =
          switch (pending.kind()) {
            case PLAYERS -> "Player list";
            case CONNECTION_CHECK -> "Connection";
            case NODE_LIST -> "Node list";
            case NODE_INFO -> "Node info";
            case TOPIC_LIST -> "Topic list";
            case TOPIC_TYPE -> "Topic type";
            case TOPIC_FIND -> "Topic find";
            case TOPIC_ECHO -> "Topic echo";
            case TOPIC_ECHO_STOP -> "Topic echo stop";
            case TOPIC_PUB -> "Topic pub";
            case TOPIC_HZ -> "Topic hz";
            case TOPIC_HZ_STOP -> "Topic hz stop";
            case TOPIC_BW -> "Topic bw";
            case TOPIC_BW_STOP -> "Topic bw stop";
            case TOPIC_DELAY -> "Topic delay";
            case TOPIC_DELAY_STOP -> "Topic delay stop";
            case TOPIC_INFO -> "Topic info";
            case SERVICE_LIST -> "Service list";
            case SERVICE_TYPE -> "Service type";
            case SERVICE_FIND -> "Service find";
            case SERVICE_INFO -> "Service info";
            case SERVICE_CALL -> "Service call";
            case ACTION_LIST -> "Action list";
            case ACTION_TYPE -> "Action type";
            case ACTION_INFO -> "Action info";
            case ACTION_SEND_GOAL -> "Action send_goal";
            case PARAM_LIST -> "Param list";
            case PARAM_GET -> "Param get";
            case PARAM_SET -> "Param set";
            case PARAM_DESCRIBE -> "Param describe";
            case PARAM_DUMP -> "Param dump";
            case PARAM_LOAD -> "Param load";
            case INTERFACE_LIST -> "Interface list";
            case INTERFACE_SHOW -> "Interface show";
          };

      LOGGER.warn(
          "{} request #{} timed out after {} ms", kindName, requestId, REQUEST_TIMEOUT_MILLIS);

      sendToRequesterOrOperators(
          pending.requesterUuid(),
          prefix()
              .append(Text.literal(kindName + " request #"
                      + requestId
                      + " timed out. Verify bridge host/port"
                      + " and network routing/firewall.")
                  .formatted(Formatting.RED)));
    }
  }

  void sendToRequesterOrOperators(UUID requesterUuid, Text message) {
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

  static MutableText prefix() {
    return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
  }
}
