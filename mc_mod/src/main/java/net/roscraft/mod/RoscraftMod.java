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
import net.roscraft.bridge.BridgeCallback;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.TopicPayload;
import net.roscraft.mod.bridge.BridgeManager;
import net.roscraft.mod.command.RoscraftCommands;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fabric mod initialiser for Roscraft.
 *
 * <p>Responsible for:
 * <ul>
 *   <li>Creating and configuring the active bridge via {@link BridgeManager}.</li>
 *   <li>Registering Fabric lifecycle and tick event listeners.</li>
 *   <li>Registering mod commands.</li>
 * </ul>
 */
public final class RoscraftMod implements ModInitializer {

    public static final String MOD_ID = "roscraft";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

    private static final int MAX_GRAPH_ITEMS_TO_SHOW = 8;
    private static final int MAX_PLAYERS_TO_SHOW = 10;
    private static final long REQUEST_TIMEOUT_MILLIS = 5_000L;

    /** Runtime bridge lifecycle/controller, initialised in {@link #onInitialize}. */
    private BridgeManager bridgeManager;

    private MinecraftServer server;
    private final Map<Long, PendingRequest> pendingRequests = new HashMap<>();

    public enum PendingRequestKind {
        GRAPH,
        PLAYERS,
        CONNECTION_CHECK,
    }

    private record PendingRequest(
            PendingRequestKind kind, UUID requesterUuid, long createdAtMillis) {}

    @Override
    public void onInitialize() {
        LOGGER.info("Roscraft initialising...");

        var config = RoscraftConfig.load();
        bridgeManager = new BridgeManager(config, new ModBridgeCallback());

        var connectResult = bridgeManager.connect();
        if (connectResult.success()) {
            LOGGER.info(connectResult.message());
        } else {
            LOGGER.error(connectResult.message());
        }

        RoscraftCommands.register();

        ServerTickEvents.END_SERVER_TICK.register(server -> onServerTick());

        ServerLifecycleEvents.SERVER_STARTED.register(server -> this.server = server);

        ServerLifecycleEvents.SERVER_STOPPING.register(
                server -> {
                    LOGGER.info("Roscraft shutting down...");
                    this.server = null;
                    bridgeManager.close();
                });

        LOGGER.info(
                "Roscraft initialised (selected mode: {}, JNI available: {})",
                bridgeManager.selectedBridgeType(),
                bridgeManager.isJniAvailable());
    }

    /** Returns the bridge manager. May be {@code null} before initialisation completes. */
    public BridgeManager bridgeManager() {
        return bridgeManager;
    }

    public synchronized void trackRequest(
            long requestId, PendingRequestKind kind, UUID requesterUuid) {
        pendingRequests.put(
                requestId, new PendingRequest(kind, requesterUuid, System.currentTimeMillis()));
    }

    private synchronized PendingRequest completeRequest(long requestId) {
        return pendingRequests.remove(requestId);
    }

    private void onServerTick() {
        bridgeManager.tick();
        processTimedOutRequests();
    }

    private void processTimedOutRequests() {
        long now = System.currentTimeMillis();
        List<Map.Entry<Long, PendingRequest>> timedOut = new ArrayList<>();

        synchronized (this) {
            var iterator = pendingRequests.entrySet().iterator();
            while (iterator.hasNext()) {
                var entry = iterator.next();
                PendingRequest pending = entry.getValue();
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
                        case GRAPH -> "Graph";
                        case PLAYERS -> "Player list";
                        case CONNECTION_CHECK -> "Connection";
                    };

            LOGGER.warn(
                    "{} request #{} timed out after {} ms",
                    kindName,
                    requestId,
                    REQUEST_TIMEOUT_MILLIS);

            sendToRequesterOrOperators(
                    pending.requesterUuid(),
                    prefix().append(
                                    Text.literal(
                                                    kindName
                                                            + " request #"
                                                            + requestId
                                                            + " timed out. Verify bridge host/port"
                                                            + " and network routing/firewall.")
                                            .formatted(Formatting.RED)));
        }
    }

    // -------------------------------------------------------------------------
    // Inner callback — forwards ROS2 events to Minecraft
    // -------------------------------------------------------------------------

    /**
     * Default {@link BridgeCallback} that handles ROS2 events during normal
     * mod operation.  Commands and subsystem handlers can register their own
     * callbacks via {@link RoscraftBridge#registerCallback}.
     */
    private final class ModBridgeCallback implements BridgeCallback {

        @Override
        public void onGraphSnapshot(GraphSnapshot snapshot) {
            LOGGER.debug(
                    "Graph snapshot received: {} topics, {} services, {} actions",
                    snapshot.topics().size(),
                    snapshot.services().size(),
                    snapshot.actions().size());

            PendingRequest pending = completeRequest(snapshot.requestId());
            UUID requesterUuid = pending == null ? null : pending.requesterUuid();

            if (pending != null && pending.kind() == PendingRequestKind.CONNECTION_CHECK) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        prefix().append(
                                        Text.literal("Connection probe #")
                                                .formatted(Formatting.GOLD))
                                .append(
                                        Text.literal(String.valueOf(snapshot.requestId()))
                                                .formatted(Formatting.YELLOW))
                                .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                                .append(
                                        Text.literal(
                                                        "Bridge replied with topics="
                                                                + snapshot.topics().size()
                                                                + ", services="
                                                                + snapshot.services().size()
                                                                + ", actions="
                                                                + snapshot.actions().size())
                                                .formatted(Formatting.GRAY)));
                return;
            }

            if (pending != null && pending.kind() != PendingRequestKind.GRAPH) {
                return;
            }

            sendToRequesterOrOperators(
                    requesterUuid,
                    prefix().append(
                                    Text.literal("Graph reply #" + snapshot.requestId() + " ")
                                            .formatted(Formatting.GOLD))
                            .append(
                                    Text.literal(
                                                    "topics="
                                                            + snapshot.topics().size()
                                                            + ", services="
                                                            + snapshot.services().size()
                                                            + ", actions="
                                                            + snapshot.actions().size())
                                            .formatted(Formatting.GREEN)));

            sendListPreviewToRequesterOrOperators(
                    requesterUuid, "Topics", snapshot.topics(), Formatting.AQUA);
            sendListPreviewToRequesterOrOperators(
                    requesterUuid, "Services", snapshot.services(), Formatting.BLUE);
            sendListPreviewToRequesterOrOperators(
                    requesterUuid, "Actions", snapshot.actions(), Formatting.DARK_AQUA);
        }

        @Override
        public void onTopicPayload(TopicPayload payload) {
            LOGGER.trace(
                    "Topic payload: {} ({} bytes)", payload.topicName(), payload.payloadLength());
        }

        @Override
        public void onPlayerList(PlayerList playerList) {
            LOGGER.debug("Player list received: {} players", playerList.size());

            PendingRequest pending = completeRequest(playerList.requestId());
            UUID requesterUuid = pending == null ? null : pending.requesterUuid();

            if (pending != null && pending.kind() == PendingRequestKind.CONNECTION_CHECK) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        prefix().append(
                                        Text.literal("Connection probe #")
                                                .formatted(Formatting.GOLD))
                                .append(
                                        Text.literal(String.valueOf(playerList.requestId()))
                                                .formatted(Formatting.YELLOW))
                                .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                                .append(
                                        Text.literal(
                                                        "Bridge replied with "
                                                                + playerList.size()
                                                                + " players.")
                                                .formatted(Formatting.GRAY)));
                return;
            }

            if (pending != null && pending.kind() != PendingRequestKind.PLAYERS) {
                return;
            }

            sendToRequesterOrOperators(
                    requesterUuid,
                    prefix().append(
                                    Text.literal(
                                                    "Player list reply #"
                                                            + playerList.requestId()
                                                            + " ")
                                            .formatted(Formatting.GOLD))
                            .append(
                                    Text.literal("count=" + playerList.size())
                                            .formatted(Formatting.GREEN)));

            int count = Math.min(playerList.players().size(), MAX_PLAYERS_TO_SHOW);
            for (int i = 0; i < count; i++) {
                Player player = playerList.players().get(i);
                sendToRequesterOrOperators(
                        requesterUuid,
                        Text.literal(" - ")
                                .formatted(Formatting.DARK_GRAY)
                                .append(Text.literal(player.name()).formatted(Formatting.YELLOW))
                                .append(
                                        Text.literal(
                                                        String.format(
                                                                " (%.1f, %.1f, %.1f)",
                                                                player.x(), player.y(), player.z()))
                                                .formatted(Formatting.GRAY)));
            }

            if (playerList.players().size() > MAX_PLAYERS_TO_SHOW) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        Text.literal(
                                        " - ... and "
                                                + (playerList.players().size()
                                                        - MAX_PLAYERS_TO_SHOW)
                                                + " more")
                                .formatted(Formatting.GRAY));
            }
        }

        private void sendListPreviewToRequesterOrOperators(
                UUID requesterUuid, String label, List<String> values, Formatting color) {
            if (values.isEmpty()) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
                return;
            }

            int count = Math.min(values.size(), MAX_GRAPH_ITEMS_TO_SHOW);
            for (int i = 0; i < count; i++) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        Text.literal(" - " + label + ": ")
                                .formatted(Formatting.DARK_GRAY)
                                .append(Text.literal(values.get(i)).formatted(color)));
            }

            if (values.size() > MAX_GRAPH_ITEMS_TO_SHOW) {
                sendToRequesterOrOperators(
                        requesterUuid,
                        Text.literal(
                                        " - "
                                                + label
                                                + ": ... and "
                                                + (values.size() - MAX_GRAPH_ITEMS_TO_SHOW)
                                                + " more")
                                .formatted(Formatting.GRAY));
            }
        }
    }

    private void sendToRequesterOrOperators(UUID requesterUuid, Text message) {
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
        playerManager
                .getPlayerList()
                .forEach(
                        player -> {
                            if (player.hasPermissionLevel(2)) {
                                player.sendMessage(copy.copy(), false);
                            }
                        });
    }

    private static MutableText prefix() {
        return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
    }
}
