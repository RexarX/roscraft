package net.roscraft.mod.command;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import java.util.UUID;
import net.fabricmc.fabric.api.command.v2.CommandRegistrationCallback;
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.RoscraftMod.PendingRequestKind;
import net.roscraft.mod.bridge.BridgeManager;

public final class RoscraftCommands {

    private RoscraftCommands() {}

    public static void register() {
        CommandRegistrationCallback.EVENT.register(
                (dispatcher, registryAccess, environment) -> registerAll(dispatcher));
    }

    private static void registerAll(CommandDispatcher<ServerCommandSource> dispatcher) {
        dispatcher.register(
                CommandManager.literal("ros")
                        .requires(src -> src.hasPermissionLevel(2))
                        .executes(ctx -> executeHelp(ctx.getSource()))
                        .then(
                                CommandManager.literal("status")
                                        .executes(ctx -> executeConnectionStatus(ctx.getSource())))
                        .then(
                                CommandManager.literal("connect")
                                        .executes(ctx -> executeConnect(ctx.getSource())))
                        .then(
                                CommandManager.literal("disconnect")
                                        .executes(ctx -> executeDisconnect(ctx.getSource())))
                        .then(
                                CommandManager.literal("mode")
                                        .then(
                                                CommandManager.argument(
                                                                "type", StringArgumentType.word())
                                                        .suggests(
                                                                (context, builder) -> {
                                                                    builder.suggest("network");
                                                                    builder.suggest("jni");
                                                                    return builder.buildFuture();
                                                                })
                                                        .executes(
                                                                ctx ->
                                                                        executeSetMode(
                                                                                ctx.getSource(),
                                                                                StringArgumentType
                                                                                        .getString(
                                                                                                ctx,
                                                                                                "type")))))
                        .then(
                                CommandManager.literal("host")
                                        .then(
                                                CommandManager.argument(
                                                                "ip", StringArgumentType.string())
                                                        .executes(
                                                                ctx ->
                                                                        executeSetHost(
                                                                                ctx.getSource(),
                                                                                StringArgumentType
                                                                                        .getString(
                                                                                                ctx,
                                                                                                "ip")))))
                        .then(
                                CommandManager.literal("port")
                                        .then(
                                                CommandManager.argument(
                                                                "port",
                                                                IntegerArgumentType.integer(
                                                                        1, 65535))
                                                        .executes(
                                                                ctx ->
                                                                        executeSetPort(
                                                                                ctx.getSource(),
                                                                                IntegerArgumentType
                                                                                        .getInteger(
                                                                                                ctx,
                                                                                                "port")))))
                        .then(
                                CommandManager.literal("endpoint")
                                        .then(
                                                CommandManager.argument(
                                                                "ip", StringArgumentType.string())
                                                        .then(
                                                                CommandManager.argument(
                                                                                "port",
                                                                                IntegerArgumentType
                                                                                        .integer(
                                                                                                1,
                                                                                                65535))
                                                                        .executes(
                                                                                ctx ->
                                                                                        executeSetEndpoint(
                                                                                                ctx
                                                                                                        .getSource(),
                                                                                                StringArgumentType
                                                                                                        .getString(
                                                                                                                ctx,
                                                                                                                "ip"),
                                                                                                IntegerArgumentType
                                                                                                        .getInteger(
                                                                                                                ctx,
                                                                                                                "port"))))))
                        .then(
                                CommandManager.literal("probe")
                                        .executes(ctx -> executeProbe(ctx.getSource())))
                        .then(
                                CommandManager.literal("graph")
                                        .executes(ctx -> executeGraph(ctx.getSource())))
                        .then(
                                CommandManager.literal("subscribe")
                                        .then(
                                                CommandManager.argument(
                                                                "topic",
                                                                StringArgumentType.string())
                                                        .then(
                                                                CommandManager.argument(
                                                                                "type",
                                                                                StringArgumentType
                                                                                        .string())
                                                                        .executes(
                                                                                ctx ->
                                                                                        executeSubscribe(
                                                                                                ctx
                                                                                                        .getSource(),
                                                                                                StringArgumentType
                                                                                                        .getString(
                                                                                                                ctx,
                                                                                                                "topic"),
                                                                                                StringArgumentType
                                                                                                        .getString(
                                                                                                                ctx,
                                                                                                                "type"))))))
                        .then(
                                CommandManager.literal("players")
                                        .executes(ctx -> executePlayers(ctx.getSource())))
                        .then(
                                CommandManager.literal("connection")
                                        .executes(ctx -> executeConnectionStatus(ctx.getSource()))
                                        .then(
                                                CommandManager.literal("status")
                                                        .executes(
                                                                ctx ->
                                                                        executeConnectionStatus(
                                                                                ctx.getSource())))
                                        .then(
                                                CommandManager.literal("connect")
                                                        .executes(
                                                                ctx ->
                                                                        executeConnect(
                                                                                ctx.getSource())))
                                        .then(
                                                CommandManager.literal("disconnect")
                                                        .executes(
                                                                ctx ->
                                                                        executeDisconnect(
                                                                                ctx.getSource())))
                                        .then(
                                                CommandManager.literal("mode")
                                                        .then(
                                                                CommandManager.argument(
                                                                                "type",
                                                                                StringArgumentType
                                                                                        .word())
                                                                        .suggests(
                                                                                (context,
                                                                                        builder) -> {
                                                                                    builder.suggest(
                                                                                            "network");
                                                                                    builder.suggest(
                                                                                            "jni");
                                                                                    return builder
                                                                                            .buildFuture();
                                                                                })
                                                                        .executes(
                                                                                ctx ->
                                                                                        executeSetMode(
                                                                                                ctx
                                                                                                        .getSource(),
                                                                                                StringArgumentType
                                                                                                        .getString(
                                                                                                                ctx,
                                                                                                                "type")))))
                                        .then(
                                                CommandManager.literal("host")
                                                        .then(
                                                                CommandManager.argument(
                                                                                "ip",
                                                                                StringArgumentType
                                                                                        .string())
                                                                        .executes(
                                                                                ctx ->
                                                                                        executeSetHost(
                                                                                                ctx
                                                                                                        .getSource(),
                                                                                                StringArgumentType
                                                                                                        .getString(
                                                                                                                ctx,
                                                                                                                "ip")))))
                                        .then(
                                                CommandManager.literal("port")
                                                        .then(
                                                                CommandManager.argument(
                                                                                "port",
                                                                                IntegerArgumentType
                                                                                        .integer(
                                                                                                1,
                                                                                                65535))
                                                                        .executes(
                                                                                ctx ->
                                                                                        executeSetPort(
                                                                                                ctx
                                                                                                        .getSource(),
                                                                                                IntegerArgumentType
                                                                                                        .getInteger(
                                                                                                                ctx,
                                                                                                                "port")))))
                                        .then(
                                                CommandManager.literal("endpoint")
                                                        .then(
                                                                CommandManager.argument(
                                                                                "ip",
                                                                                StringArgumentType
                                                                                        .string())
                                                                        .then(
                                                                                CommandManager
                                                                                        .argument(
                                                                                                "port",
                                                                                                IntegerArgumentType
                                                                                                        .integer(
                                                                                                                1,
                                                                                                                65535))
                                                                                        .executes(
                                                                                                ctx ->
                                                                                                        executeSetEndpoint(
                                                                                                                ctx
                                                                                                                        .getSource(),
                                                                                                                StringArgumentType
                                                                                                                        .getString(
                                                                                                                                ctx,
                                                                                                                                "ip"),
                                                                                                                IntegerArgumentType
                                                                                                                        .getInteger(
                                                                                                                                ctx,
                                                                                                                                "port"))))))
                                        .then(
                                                CommandManager.literal("probe")
                                                        .executes(
                                                                ctx ->
                                                                        executeProbe(
                                                                                ctx
                                                                                        .getSource())))));
    }

    private static int executeConnectionStatus(ServerCommandSource source) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        String status = manager.isConnected() ? "connected" : "disconnected";
        String selected = manager.selectedBridgeType();
        String active = manager.activeBridgeType();
        String endpoint = manager.activeEndpoint();
        String inbound =
                manager.isConnected() && "network".equals(active)
                        ? (manager.networkHasSeenInboundTraffic() ? "yes" : "no")
                        : "n/a";

        sendStyled(
                source,
                prefix().append(Text.literal("Status ").formatted(Formatting.GOLD))
                        .append(Text.literal(status).formatted(formatStatus(status)))
                        .append(Text.literal(" | selected=").formatted(Formatting.DARK_GRAY))
                        .append(Text.literal(selected).formatted(Formatting.YELLOW))
                        .append(Text.literal(" | active=").formatted(Formatting.DARK_GRAY))
                        .append(Text.literal(active).formatted(Formatting.YELLOW))
                        .append(Text.literal(" | endpoint=").formatted(Formatting.DARK_GRAY))
                        .append(Text.literal(endpoint).formatted(Formatting.AQUA))
                        .append(Text.literal(" | jniAvailable=").formatted(Formatting.DARK_GRAY))
                        .append(
                                Text.literal(String.valueOf(manager.isJniAvailable()))
                                        .formatted(
                                                manager.isJniAvailable()
                                                        ? Formatting.GREEN
                                                        : Formatting.RED))
                        .append(Text.literal(" | inboundRx=").formatted(Formatting.DARK_GRAY))
                        .append(Text.literal(inbound).formatted(formatInboundStatus(inbound))));
        return 1;
    }

    private static int executeHelp(ServerCommandSource source) {
        sendStyled(source, prefix().append(Text.literal("Commands:").formatted(Formatting.GOLD)));
        sendStyled(
                source,
                Text.literal(" - /ros status | connect | disconnect | probe")
                        .formatted(Formatting.GRAY));
        sendStyled(source, Text.literal(" - /ros mode <network|jni>").formatted(Formatting.GRAY));
        sendStyled(
                source,
                Text.literal(" - /ros host <ip> | /ros port <port> | /ros endpoint <ip> <port>")
                        .formatted(Formatting.GRAY));
        sendStyled(
                source,
                Text.literal(" - /ros graph | /ros players | /ros subscribe <topic> <type>")
                        .formatted(Formatting.GRAY));
        sendStyled(
                source,
                Text.literal("Tip: in WSL2 network mode, use /ros endpoint <wsl-ip> <port>")
                        .formatted(Formatting.DARK_AQUA));
        return 1;
    }

    private static int executeConnect(ServerCommandSource source) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.connect();
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));

        if ("network".equals(manager.activeBridgeType())) {
            sendStyled(
                    source,
                    prefix().append(
                                    Text.literal("Running connectivity probe...")
                                            .formatted(Formatting.GOLD)));
            return executeProbe(source);
        }
        return 1;
    }

    private static int executeDisconnect(ServerCommandSource source) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.disconnect();
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.YELLOW)));
        return 1;
    }

    private static int executeSetMode(ServerCommandSource source, String type) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.setBridgeType(type);
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
        return 1;
    }

    private static int executeSetHost(ServerCommandSource source, String host) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.setNetworkHost(host);
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
        return 1;
    }

    private static int executeSetPort(ServerCommandSource source, int port) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.setNetworkPort(port);
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
        return 1;
    }

    private static int executeSetEndpoint(ServerCommandSource source, String host, int port) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        BridgeManager.OperationResult result = manager.setNetworkEndpoint(host, port);
        if (!result.success()) {
            sendError(source, result.message());
            return 0;
        }

        sendStyled(
                source,
                prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
        return 1;
    }

    private static int executeProbe(ServerCommandSource source) {
        RoscraftBridge bridge = requireBridge(source);
        if (bridge == null) {
            return 0;
        }

        long requestId = bridge.queryGraph();
        RoscraftMod mod = getMod();
        if (mod != null) {
            mod.trackRequest(requestId, PendingRequestKind.CONNECTION_CHECK, requesterUuid(source));
        }

        sendStyled(
                source,
                prefix().append(Text.literal("Probe request #").formatted(Formatting.GOLD))
                        .append(
                                Text.literal(String.valueOf(requestId))
                                        .formatted(Formatting.YELLOW))
                        .append(Text.literal(" sent.").formatted(Formatting.GRAY)));
        return 1;
    }

    private static int executeGraph(ServerCommandSource source) {
        RoscraftBridge bridge = requireBridge(source);
        if (bridge == null) {
            return 0;
        }

        long requestId = bridge.queryGraph();
        RoscraftMod mod = getMod();
        if (mod != null) {
            mod.trackRequest(requestId, PendingRequestKind.GRAPH, requesterUuid(source));
        }

        sendStyled(
                source,
                prefix().append(Text.literal("Graph request #").formatted(Formatting.GOLD))
                        .append(
                                Text.literal(String.valueOf(requestId))
                                        .formatted(Formatting.YELLOW))
                        .append(
                                Text.literal(" sent. Waiting for reply...")
                                        .formatted(Formatting.GRAY)));
        return 1;
    }

    private static int executeSubscribe(ServerCommandSource source, String topic, String type) {
        RoscraftBridge bridge = requireBridge(source);
        if (bridge == null) {
            return 0;
        }

        long requestId = bridge.subscribeTopic(topic, type);
        sendStyled(
                source,
                prefix().append(Text.literal("Subscribed: ").formatted(Formatting.GREEN))
                        .append(Text.literal(topic).formatted(Formatting.YELLOW))
                        .append(Text.literal(" (" + type + ") ").formatted(Formatting.GRAY))
                        .append(
                                Text.literal("request #" + requestId)
                                        .formatted(Formatting.DARK_AQUA)));
        return 1;
    }

    private static int executePlayers(ServerCommandSource source) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return 0;
        }

        if ("network".equals(manager.activeBridgeType())) {
            sendError(
                    source,
                    "/ros players is currently unavailable in network mode. "
                            + "Use /ros probe or /ros graph to verify connectivity.");
            return 0;
        }

        RoscraftBridge bridge = requireBridge(source);
        if (bridge == null) {
            return 0;
        }

        long requestId = bridge.queryPlayers();
        RoscraftMod mod = getMod();
        if (mod != null) {
            mod.trackRequest(requestId, PendingRequestKind.PLAYERS, requesterUuid(source));
        }

        sendStyled(
                source,
                prefix().append(Text.literal("Player list request #").formatted(Formatting.GOLD))
                        .append(
                                Text.literal(String.valueOf(requestId))
                                        .formatted(Formatting.YELLOW))
                        .append(
                                Text.literal(" sent. Waiting for reply...")
                                        .formatted(Formatting.GRAY)));
        return 1;
    }

    private static RoscraftBridge requireBridge(ServerCommandSource source) {
        BridgeManager manager = getBridgeManager();
        if (manager == null) {
            sendError(source, "Roscraft is not ready yet.");
            return null;
        }

        try {
            return manager.requireConnectedBridge();
        } catch (IllegalStateException e) {
            sendError(source, e.getMessage());
            return null;
        }
    }

    private static UUID requesterUuid(ServerCommandSource source) {
        try {
            if (source.getEntity() != null) {
                return source.getEntity().getUuid();
            }
        } catch (RuntimeException ignored) {
        }
        return null;
    }

    private static Formatting formatStatus(String status) {
        if ("connected".equals(status)) {
            return Formatting.GREEN;
        }
        return Formatting.RED;
    }

    private static Formatting formatInboundStatus(String inbound) {
        if ("yes".equals(inbound)) {
            return Formatting.GREEN;
        }
        if ("no".equals(inbound)) {
            return Formatting.RED;
        }
        return Formatting.GRAY;
    }

    private static void sendStyled(ServerCommandSource source, Text message) {
        source.sendFeedback(() -> message, false);
    }

    private static void sendError(ServerCommandSource source, String message) {
        source.sendError(prefix().append(Text.literal(message).formatted(Formatting.RED)));
    }

    private static MutableText prefix() {
        return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
    }

    private static RoscraftMod getMod() {
        var mods =
                net.fabricmc.loader.api.FabricLoader.getInstance()
                        .getEntrypoints("main", net.fabricmc.api.ModInitializer.class);
        for (var mod : mods) {
            if (mod instanceof RoscraftMod rm) {
                return rm;
            }
        }
        RoscraftMod.LOGGER.error("RoscraftMod entrypoint not found!");
        return null;
    }

    private static BridgeManager getBridgeManager() {
        RoscraftMod mod = getMod();
        if (mod == null) {
            return null;
        }
        return mod.bridgeManager();
    }
}
