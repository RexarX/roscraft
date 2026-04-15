package net.roscraft.mod.command;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.Arrays;
import java.util.List;
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
import net.roscraft.mod.command.action.ActionCommands;
import net.roscraft.mod.command.interface_cmd.InterfaceCommands;
import net.roscraft.mod.command.node.NodeCommands;
import net.roscraft.mod.command.param.ParamCommands;
import net.roscraft.mod.command.service.ServiceCommands;
import net.roscraft.mod.command.topic.TopicCommands;

public final class RoscraftCommands {

  private RoscraftCommands() {}

  public static void register() {
    CommandRegistrationCallback.EVENT.register(
        (dispatcher, registryAccess, environment) -> registerAll(dispatcher));
  }

  private static void registerAll(CommandDispatcher<ServerCommandSource> dispatcher) {
    dispatcher.register(CommandManager.literal("ros")
        .requires(src -> src.hasPermissionLevel(2))
        .executes(ctx -> executeHelp(ctx.getSource()))
        .then(CommandManager.literal("status")
            .executes(ctx -> executeConnectionStatus(ctx.getSource())))
        .then(CommandManager.literal("connect").executes(ctx -> executeConnect(ctx.getSource())))
        .then(CommandManager.literal("disconnect")
            .executes(ctx -> executeDisconnect(ctx.getSource())))
        .then(CommandManager.literal("mode")
            .then(CommandManager.argument("type", StringArgumentType.word())
                .suggests((context, builder) -> {
                  builder.suggest("network");
                  builder.suggest("jni");
                  return builder.buildFuture();
                })
                .executes(ctx ->
                    executeSetMode(ctx.getSource(), StringArgumentType.getString(ctx, "type")))))
        .then(CommandManager.literal("host")
            .then(CommandManager.argument("ip", StringArgumentType.string())
                .executes(ctx ->
                    executeSetHost(ctx.getSource(), StringArgumentType.getString(ctx, "ip")))))
        .then(CommandManager.literal("port")
            .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                .executes(ctx ->
                    executeSetPort(ctx.getSource(), IntegerArgumentType.getInteger(ctx, "port")))))
        .then(CommandManager.literal("endpoint")
            .then(CommandManager.argument("ip", StringArgumentType.string())
                .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                    .executes(ctx -> executeSetEndpoint(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "ip"),
                        IntegerArgumentType.getInteger(ctx, "port"))))))
        .then(CommandManager.literal("probe").executes(ctx -> executeProbe(ctx.getSource())))
        .then(CommandManager.literal("subscribe")
            .then(CommandManager.argument("topic", StringArgumentType.string())
                .then(CommandManager.argument("type", StringArgumentType.string())
                    .executes(ctx -> executeSubscribe(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic"),
                        StringArgumentType.getString(ctx, "type"))))))
        .then(CommandManager.literal("node")
            .then(CommandManager.literal("list")
                .executes(ctx -> executeNodeList(ctx.getSource(), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-a");
                      builder.suggest("--all");
                      builder.suggest("-c");
                      builder.suggest("--count");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeNodeList(
                        ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
            .then(CommandManager.literal("info")
                .then(CommandManager.argument("node_name", StringArgumentType.string())
                    .executes(ctx -> executeNodeInfo(
                        ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--include-hidden");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeNodeInfo(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "node_name"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("topic")
            .then(CommandManager.literal("list")
                .executes(ctx -> executeTopicList(ctx.getSource(), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-t");
                      builder.suggest("--show-types");
                      builder.suggest("-c");
                      builder.suggest("--count");
                      builder.suggest("--include-hidden-topics");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeTopicList(
                        ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
            .then(CommandManager.literal("type")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .executes(ctx -> executeTopicType(
                        ctx.getSource(), StringArgumentType.getString(ctx, "topic_name")))))
            .then(CommandManager.literal("find")
                .then(CommandManager.argument("topic_type", StringArgumentType.string())
                    .executes(ctx -> executeTopicFind(
                        ctx.getSource(), StringArgumentType.getString(ctx, "topic_type")))))
            .then(CommandManager.literal("echo")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .then(CommandManager.argument("message_type", StringArgumentType.string())
                        .executes(ctx -> executeTopicEcho(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--once");
                              builder.suggest("--timeout 5");
                              builder.suggest("--raw");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeTopicEcho(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "flags")))))))
            .then(CommandManager.literal("pub")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .then(CommandManager.argument("message_type", StringArgumentType.string())
                        .then(CommandManager.argument("payload", StringArgumentType.string())
                            .executes(ctx -> executeTopicPub(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "payload"),
                                ""))
                            .then(CommandManager.argument(
                                    "flags", StringArgumentType.greedyString())
                                .suggests((context, builder) -> {
                                  builder.suggest("--once");
                                  builder.suggest("-1");
                                  builder.suggest("-r 10");
                                  builder.suggest("-t 5");
                                  builder.suggest("--qos-profile sensor_data");
                                  return builder.buildFuture();
                                })
                                .executes(ctx -> executeTopicPub(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "topic_name"),
                                    StringArgumentType.getString(ctx, "message_type"),
                                    StringArgumentType.getString(ctx, "payload"),
                                    StringArgumentType.getString(ctx, "flags"))))))))
            .then(CommandManager.literal("hz")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .then(CommandManager.argument("message_type", StringArgumentType.string())
                        .executes(ctx -> executeTopicHz(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--window 10000");
                              builder.suggest("--wall-time");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeTopicHz(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "flags")))))))
            .then(CommandManager.literal("bw")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .then(CommandManager.argument("message_type", StringArgumentType.string())
                        .executes(ctx -> executeTopicBw(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--window 100");
                              builder.suggest("--wall-time");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeTopicBw(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "flags")))))))
            .then(CommandManager.literal("info")
                .then(CommandManager.argument("topic_name", StringArgumentType.string())
                    .executes(ctx -> executeTopicInfo(
                        ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("-v");
                          builder.suggest("--verbose");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeTopicInfo(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("service")
            .then(CommandManager.literal("list")
                .executes(ctx -> executeServiceList(ctx.getSource(), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-t");
                      builder.suggest("--show-types");
                      builder.suggest("-c");
                      builder.suggest("--count");
                      builder.suggest("--include-hidden-services");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeServiceList(
                        ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
            .then(CommandManager.literal("type")
                .then(CommandManager.argument("service_name", StringArgumentType.string())
                    .executes(ctx -> executeServiceType(
                        ctx.getSource(), StringArgumentType.getString(ctx, "service_name")))))
            .then(CommandManager.literal("find")
                .then(CommandManager.argument("service_type", StringArgumentType.string())
                    .executes(ctx -> executeServiceFind(
                        ctx.getSource(), StringArgumentType.getString(ctx, "service_type")))))
            .then(CommandManager.literal("info")
                .then(CommandManager.argument("service_name", StringArgumentType.string())
                    .executes(ctx -> executeServiceInfo(
                        ctx.getSource(), StringArgumentType.getString(ctx, "service_name"), ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("-v");
                          builder.suggest("--verbose");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeServiceInfo(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "service_name"),
                            StringArgumentType.getString(ctx, "flags")))))
                .then(CommandManager.literal("call")
                    .then(CommandManager.argument("service_name", StringArgumentType.string())
                        .then(CommandManager.argument("service_type", StringArgumentType.string())
                            .then(CommandManager.argument("request", StringArgumentType.string())
                                .executes(ctx -> executeServiceCall(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "service_name"),
                                    StringArgumentType.getString(ctx, "service_type"),
                                    StringArgumentType.getString(ctx, "request"),
                                    ""))
                                .then(CommandManager.argument(
                                        "flags", StringArgumentType.greedyString())
                                    .suggests((context, builder) -> {
                                      builder.suggest("--timeout 5");
                                      builder.suggest("-r 3");
                                      builder.suggest("--rate 1");
                                      return builder.buildFuture();
                                    })
                                    .executes(ctx -> executeServiceCall(
                                        ctx.getSource(),
                                        StringArgumentType.getString(ctx, "service_name"),
                                        StringArgumentType.getString(ctx, "service_type"),
                                        StringArgumentType.getString(ctx, "request"),
                                        StringArgumentType.getString(ctx, "flags")))))))))
            .then(CommandManager.literal("action")
                .then(CommandManager.literal("list")
                    .executes(ctx -> executeActionList(ctx.getSource(), ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("-t");
                          builder.suggest("--show-types");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeActionList(
                            ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
                .then(CommandManager.literal("type")
                    .then(CommandManager.argument("action_name", StringArgumentType.string())
                        .executes(ctx -> executeActionType(
                            ctx.getSource(), StringArgumentType.getString(ctx, "action_name")))))
                .then(CommandManager.literal("info")
                    .then(CommandManager.argument("action_name", StringArgumentType.string())
                        .executes(ctx -> executeActionInfo(
                            ctx.getSource(), StringArgumentType.getString(ctx, "action_name"), ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--include-hidden");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeActionInfo(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "action_name"),
                                StringArgumentType.getString(ctx, "flags"))))))
                .then(CommandManager.literal("send_goal")
                    .then(CommandManager.argument("action_name", StringArgumentType.string())
                        .then(CommandManager.argument("action_type", StringArgumentType.string())
                            .then(CommandManager.argument("goal", StringArgumentType.string())
                                .executes(ctx -> executeActionSendGoal(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "action_name"),
                                    StringArgumentType.getString(ctx, "action_type"),
                                    StringArgumentType.getString(ctx, "goal"),
                                    ""))
                                .then(CommandManager.argument(
                                        "flags", StringArgumentType.greedyString())
                                    .suggests((context, builder) -> {
                                      builder.suggest("-f");
                                      builder.suggest("--feedback");
                                      builder.suggest("--timeout 10");
                                      return builder.buildFuture();
                                    })
                                    .executes(ctx -> executeActionSendGoal(
                                        ctx.getSource(),
                                        StringArgumentType.getString(ctx, "action_name"),
                                        StringArgumentType.getString(ctx, "action_type"),
                                        StringArgumentType.getString(ctx, "goal"),
                                        StringArgumentType.getString(ctx, "flags"))))))))
                .then(CommandManager.literal("param")
                    .then(CommandManager.literal("list")
                        .then(CommandManager.argument("node_name", StringArgumentType.string())
                            .executes(ctx -> executeParamList(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "node_name"),
                                ""))
                            .then(CommandManager.argument(
                                    "flags", StringArgumentType.greedyString())
                                .suggests((context, builder) -> {
                                  builder.suggest("--depth 0");
                                  builder.suggest("--param-prefixes foo bar");
                                  builder.suggest("--param-type");
                                  builder.suggest("--filter .*rate.*");
                                  return builder.buildFuture();
                                })
                                .executes(ctx -> executeParamList(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "node_name"),
                                    StringArgumentType.getString(ctx, "flags"))))))
                    .then(CommandManager.literal("get")
                        .then(CommandManager.argument("node_name", StringArgumentType.string())
                            .then(CommandManager.argument("param_name", StringArgumentType.string())
                                .executes(ctx -> executeParamGet(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "node_name"),
                                    StringArgumentType.getString(ctx, "param_name"),
                                    ""))
                                .then(CommandManager.argument(
                                        "flags", StringArgumentType.greedyString())
                                    .suggests((context, builder) -> {
                                      builder.suggest("--hide-type");
                                      return builder.buildFuture();
                                    })
                                    .executes(ctx -> executeParamGet(
                                        ctx.getSource(),
                                        StringArgumentType.getString(ctx, "node_name"),
                                        StringArgumentType.getString(ctx, "param_name"),
                                        StringArgumentType.getString(ctx, "flags"))))))))
                .then(CommandManager.literal("set")
                    .then(CommandManager.argument("node_name", StringArgumentType.string())
                        .then(CommandManager.argument("param_name", StringArgumentType.string())
                            .then(CommandManager.argument("value", StringArgumentType.string())
                                .executes(ctx -> executeParamSet(
                                    ctx.getSource(),
                                    StringArgumentType.getString(ctx, "node_name"),
                                    StringArgumentType.getString(ctx, "param_name"),
                                    StringArgumentType.getString(ctx, "value"),
                                    ""))
                                .then(CommandManager.argument(
                                        "flags", StringArgumentType.greedyString())
                                    .suggests((context, builder) -> {
                                      builder.suggest("--timeout 5");
                                      return builder.buildFuture();
                                    })
                                    .executes(ctx -> executeParamSet(
                                        ctx.getSource(),
                                        StringArgumentType.getString(ctx, "node_name"),
                                        StringArgumentType.getString(ctx, "param_name"),
                                        StringArgumentType.getString(ctx, "value"),
                                        StringArgumentType.getString(ctx, "flags"))))))))
                .then(CommandManager.literal("describe")
                    .then(CommandManager.argument("node_name", StringArgumentType.string())
                        .then(CommandManager.argument("param_name", StringArgumentType.string())
                            .executes(ctx -> executeParamDescribe(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "node_name"),
                                StringArgumentType.getString(ctx, "param_name"))))))
                .then(CommandManager.literal("dump")
                    .then(CommandManager.argument("node_name", StringArgumentType.string())
                        .executes(ctx -> executeParamDump(
                            ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--param-prefixes foo bar");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeParamDump(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "node_name"),
                                StringArgumentType.getString(ctx, "flags")))))))
            .then(CommandManager.literal("interface")
                .then(CommandManager.literal("list")
                    .executes(ctx -> executeInterfaceList(ctx.getSource(), ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("-m");
                          builder.suggest("--only-msgs");
                          builder.suggest("-s");
                          builder.suggest("--only-srvs");
                          builder.suggest("-a");
                          builder.suggest("--only-actions");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeInterfaceList(
                            ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
                .then(CommandManager.literal("show")
                    .then(CommandManager.argument("interface_type", StringArgumentType.string())
                        .executes(ctx -> executeInterfaceShow(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "interface_type"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--no-comments");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeInterfaceShow(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "interface_type"),
                                StringArgumentType.getString(ctx, "flags")))))))
            .then(
                CommandManager.literal("players").executes(ctx -> executePlayers(ctx.getSource())))
            .then(CommandManager.literal("connection")
                .executes(ctx -> executeConnectionStatus(ctx.getSource()))
                .then(CommandManager.literal("status")
                    .executes(ctx -> executeConnectionStatus(ctx.getSource())))
                .then(CommandManager.literal("connect")
                    .executes(ctx -> executeConnect(ctx.getSource())))
                .then(CommandManager.literal("disconnect")
                    .executes(ctx -> executeDisconnect(ctx.getSource())))
                .then(CommandManager.literal("mode")
                    .then(CommandManager.argument("type", StringArgumentType.word())
                        .suggests((context, builder) -> {
                          builder.suggest("network");
                          builder.suggest("jni");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeSetMode(
                            ctx.getSource(), StringArgumentType.getString(ctx, "type")))))
                .then(CommandManager.literal("host")
                    .then(CommandManager.argument("ip", StringArgumentType.string())
                        .executes(ctx -> executeSetHost(
                            ctx.getSource(), StringArgumentType.getString(ctx, "ip")))))
                .then(CommandManager.literal("port")
                    .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                        .executes(ctx -> executeSetPort(
                            ctx.getSource(), IntegerArgumentType.getInteger(ctx, "port")))))
                .then(CommandManager.literal("endpoint")
                    .then(CommandManager.argument("ip", StringArgumentType.string())
                        .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                            .executes(ctx -> executeSetEndpoint(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "ip"),
                                IntegerArgumentType.getInteger(ctx, "port"))))))
                .then(CommandManager.literal("probe")
                    .executes(ctx -> executeProbe(ctx.getSource()))))));
    registerTopLevelParityCommandAliases(dispatcher);
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
    String inbound = manager.isConnected() && "network".equals(active)
        ? (manager.networkHasSeenInboundTraffic() ? "yes" : "no")
        : "n/a";

    sendStyled(
        source,
        prefix()
            .append(Text.literal("Status ").formatted(Formatting.GOLD))
            .append(Text.literal(status).formatted(formatStatus(status)))
            .append(Text.literal(" | selected=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(selected).formatted(Formatting.YELLOW))
            .append(Text.literal(" | active=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(active).formatted(Formatting.YELLOW))
            .append(Text.literal(" | endpoint=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(endpoint).formatted(Formatting.AQUA))
            .append(Text.literal(" | jniAvailable=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(String.valueOf(manager.isJniAvailable()))
                .formatted(manager.isJniAvailable() ? Formatting.GREEN : Formatting.RED))
            .append(Text.literal(" | inboundRx=").formatted(Formatting.DARK_GRAY))
            .append(Text.literal(inbound).formatted(formatInboundStatus(inbound))));
    return 1;
  }

  private static void registerTopLevelParityCommandAliases(
      CommandDispatcher<ServerCommandSource> dispatcher) {
    dispatcher.register(CommandManager.literal("ros")
        .requires(src -> src.hasPermissionLevel(2))
        .then(buildNodeCommandAlias())
        .then(buildTopicCommandAlias())
        .then(buildServiceCommandAlias())
        .then(buildActionCommandAlias())
        .then(buildParamCommandAlias())
        .then(buildInterfaceCommandAlias()));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildNodeCommandAlias() {
    return CommandManager.literal("node")
        .then(CommandManager.literal("list")
            .executes(ctx -> executeNodeList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-a");
                  builder.suggest("--all");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  return builder.buildFuture();
                })
                .executes(ctx ->
                    executeNodeList(ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> executeNodeInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--include-hidden");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeNodeInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildTopicCommandAlias() {
    return CommandManager.literal("topic")
        .then(CommandManager.literal("list")
            .executes(ctx -> executeTopicList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  builder.suggest("--include-hidden-topics");
                  return builder.buildFuture();
                })
                .executes(ctx ->
                    executeTopicList(ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> executeTopicType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name")))))
        .then(CommandManager.literal("find")
            .then(CommandManager.argument("topic_type", StringArgumentType.string())
                .executes(ctx -> executeTopicFind(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_type")))))
        .then(CommandManager.literal("echo")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .then(CommandManager.argument("message_type", StringArgumentType.string())
                    .executes(ctx -> executeTopicEcho(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "message_type"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--once");
                          builder.suggest("--timeout 5");
                          builder.suggest("--raw");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeTopicEcho(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("pub")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .then(CommandManager.argument("message_type", StringArgumentType.string())
                    .then(CommandManager.argument("payload", StringArgumentType.string())
                        .executes(ctx -> executeTopicPub(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            StringArgumentType.getString(ctx, "payload"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--once");
                              builder.suggest("-1");
                              builder.suggest("-r 10");
                              builder.suggest("-t 5");
                              builder.suggest("--qos-profile sensor_data");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeTopicPub(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "payload"),
                                StringArgumentType.getString(ctx, "flags"))))))))
        .then(CommandManager.literal("hz")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .then(CommandManager.argument("message_type", StringArgumentType.string())
                    .executes(ctx -> executeTopicHz(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "message_type"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--window 10000");
                          builder.suggest("--wall-time");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeTopicHz(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("bw")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .then(CommandManager.argument("message_type", StringArgumentType.string())
                    .executes(ctx -> executeTopicBw(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "message_type"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--window 100");
                          builder.suggest("--wall-time");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeTopicBw(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "topic_name"),
                            StringArgumentType.getString(ctx, "message_type"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> executeTopicInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-v");
                      builder.suggest("--verbose");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeTopicInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildServiceCommandAlias() {
    return CommandManager.literal("service")
        .then(CommandManager.literal("list")
            .executes(ctx -> executeServiceList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  builder.suggest("--include-hidden-services");
                  return builder.buildFuture();
                })
                .executes(ctx -> executeServiceList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .executes(ctx -> executeServiceType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_name")))))
        .then(CommandManager.literal("find")
            .then(CommandManager.argument("service_type", StringArgumentType.string())
                .executes(ctx -> executeServiceFind(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_type")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .executes(ctx -> executeServiceInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-v");
                      builder.suggest("--verbose");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeServiceInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "service_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("call")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .then(CommandManager.argument("service_type", StringArgumentType.string())
                    .then(CommandManager.argument("request", StringArgumentType.string())
                        .executes(ctx -> executeServiceCall(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "service_name"),
                            StringArgumentType.getString(ctx, "service_type"),
                            StringArgumentType.getString(ctx, "request"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--timeout 5");
                              builder.suggest("-r 3");
                              builder.suggest("--rate 1");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeServiceCall(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "service_name"),
                                StringArgumentType.getString(ctx, "service_type"),
                                StringArgumentType.getString(ctx, "request"),
                                StringArgumentType.getString(ctx, "flags"))))))));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildActionCommandAlias() {
    return CommandManager.literal("action")
        .then(CommandManager.literal("list")
            .executes(ctx -> executeActionList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  return builder.buildFuture();
                })
                .executes(ctx -> executeActionList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .executes(ctx -> executeActionType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "action_name")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .executes(ctx -> executeActionInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "action_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--include-hidden");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeActionInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "action_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("send_goal")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .then(CommandManager.argument("action_type", StringArgumentType.string())
                    .then(CommandManager.argument("goal", StringArgumentType.string())
                        .executes(ctx -> executeActionSendGoal(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "action_name"),
                            StringArgumentType.getString(ctx, "action_type"),
                            StringArgumentType.getString(ctx, "goal"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("-f");
                              builder.suggest("--feedback");
                              builder.suggest("--timeout 10");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeActionSendGoal(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "action_name"),
                                StringArgumentType.getString(ctx, "action_type"),
                                StringArgumentType.getString(ctx, "goal"),
                                StringArgumentType.getString(ctx, "flags"))))))));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildParamCommandAlias() {
    return CommandManager.literal("param")
        .then(CommandManager.literal("list")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> executeParamList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--depth 0");
                      builder.suggest("--param-prefixes foo bar");
                      builder.suggest("--param-type");
                      builder.suggest("--filter .*rate.*");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeParamList(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("get")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .executes(ctx -> executeParamGet(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "param_name"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--hide-type");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> executeParamGet(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "node_name"),
                            StringArgumentType.getString(ctx, "param_name"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("set")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .then(CommandManager.argument("value", StringArgumentType.string())
                        .executes(ctx -> executeParamSet(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "node_name"),
                            StringArgumentType.getString(ctx, "param_name"),
                            StringArgumentType.getString(ctx, "value"),
                            ""))
                        .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                            .suggests((context, builder) -> {
                              builder.suggest("--timeout 5");
                              return builder.buildFuture();
                            })
                            .executes(ctx -> executeParamSet(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "node_name"),
                                StringArgumentType.getString(ctx, "param_name"),
                                StringArgumentType.getString(ctx, "value"),
                                StringArgumentType.getString(ctx, "flags"))))))))
        .then(CommandManager.literal("describe")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .executes(ctx -> executeParamDescribe(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "param_name"))))))
        .then(CommandManager.literal("dump")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> executeParamDump(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--param-prefixes foo bar");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeParamDump(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  private static LiteralArgumentBuilder<ServerCommandSource> buildInterfaceCommandAlias() {
    return CommandManager.literal("interface")
        .then(CommandManager.literal("list")
            .executes(ctx -> executeInterfaceList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-m");
                  builder.suggest("--only-msgs");
                  builder.suggest("-s");
                  builder.suggest("--only-srvs");
                  builder.suggest("-a");
                  builder.suggest("--only-actions");
                  return builder.buildFuture();
                })
                .executes(ctx -> executeInterfaceList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("show")
            .then(CommandManager.argument("interface_type", StringArgumentType.string())
                .executes(ctx -> executeInterfaceShow(
                    ctx.getSource(), StringArgumentType.getString(ctx, "interface_type"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--no-comments");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> executeInterfaceShow(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "interface_type"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  private static int executeHelp(ServerCommandSource source) {
    sendStyled(source, prefix().append(Text.literal("Commands:").formatted(Formatting.GOLD)));
    sendStyled(
        source,
        Text.literal(" - /ros status | connect | disconnect | probe").formatted(Formatting.GRAY));
    sendStyled(source, Text.literal(" - /ros mode <network|jni>").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros host <ip> | /ros port <port> | /ros endpoint <ip> <port>")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros players | /ros subscribe <topic> <type>").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros node list [-a|-c] | node info <name> [--include-hidden]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic list [-t|-c|--include-hidden-topics]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic type <name> | topic find <type> | topic echo <topic> <type>")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal("   [--once] [--timeout <sec>] [--raw]").formatted(Formatting.DARK_GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros topic info <name> [-v|--verbose]").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros topic pub <topic> <type> <payload> [--once|-1] [-r|--rate <hz>] [-t|--times <n>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros topic hz <topic> [--window <n>] [--wall-time] | topic bw <topic> [...]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros service list [-t|-c|--include-hidden-services] | service type/find/info")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros service info <name> [-v|--verbose]").formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros service call <name> <type> <request> [--timeout <sec>] [-r|--repeat <n>] [--rate <hz>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros action list [-t] | action type <name> | action info <name> [--include-hidden]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(
                " - /ros action send_goal <name> <type> <goal> [-f|--feedback] [--timeout <sec>]")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros param list/get/set/describe/dump ... (ros2-style flags)")
            .formatted(Formatting.GRAY));
    sendStyled(
        source,
        Text.literal(" - /ros interface list [-m|-s|-a] | interface show <type>")
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

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));

    if ("network".equals(manager.activeBridgeType())) {
      sendStyled(
          source,
          prefix()
              .append(Text.literal("Running connectivity probe...").formatted(Formatting.GOLD)));
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
        source, prefix().append(Text.literal(result.message()).formatted(Formatting.YELLOW)));
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

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
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

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
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

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
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

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
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
        prefix()
            .append(Text.literal("Probe request #").formatted(Formatting.GOLD))
            .append(Text.literal(String.valueOf(requestId)).formatted(Formatting.YELLOW))
            .append(Text.literal(" sent.").formatted(Formatting.GRAY)));
    return 1;
  }

  private static int executeSubscribe(ServerCommandSource source, String topic, String type) {
    TopicCommands.TopicEchoOptions options =
        TopicCommands.TopicEchoOptions.builder().build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.echo(ctx, topic, type, options));
  }

  private static int executeNodeList(ServerCommandSource source, String rawFlags) {
    boolean includeHidden = false;
    boolean count = false;
    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-a", "--all" -> includeHidden = true;
        case "-c", "--count" -> count = true;
        default -> {
          sendError(source, "Unsupported flag for /ros node list: " + flag);
          return 0;
        }
      }
    }

    NodeCommands.NodeListOptions options = NodeCommands.NodeListOptions.builder()
        .includeHidden(includeHidden)
        .countOnly(count)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.NODE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> NodeCommands.list(ctx, options));
  }

  private static int executeNodeInfo(ServerCommandSource source, String nodeName, String rawFlags) {
    boolean includeHidden = false;
    for (String flag : parseFlags(rawFlags)) {
      if ("--include-hidden".equals(flag)) {
        includeHidden = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros node info: " + flag);
      return 0;
    }

    NodeCommands.NodeInfoOptions options =
        NodeCommands.NodeInfoOptions.builder().includeHidden(includeHidden).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.NODE_INFO,
        options.encodeTrackingMetadata(),
        ctx -> NodeCommands.info(ctx, nodeName, options));
  }

  private static int executeTopicList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-topics" -> includeHidden = true;
        default -> {
          sendError(source, "Unsupported flag for /ros topic list: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicListOptions options = TopicCommands.TopicListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenTopics(includeHidden)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_LIST,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.list(ctx, options));
  }

  private static int executeTopicType(ServerCommandSource source, String topicName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_TYPE,
        topicName,
        ctx -> TopicCommands.type(ctx, topicName));
  }

  private static int executeTopicFind(ServerCommandSource source, String topicType) {
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_FIND,
        topicType,
        ctx -> TopicCommands.find(ctx, topicType));
  }

  private static int executeTopicEcho(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    boolean once = false;
    boolean raw = false;
    double timeoutSeconds = 0.0;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--once" -> once = true;
        case "--raw" -> raw = true;
        case "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --timeout");
            return 0;
          }
          String timeoutToken = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(timeoutToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + timeoutToken);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String timeoutToken = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(timeoutToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + timeoutToken);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic echo: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicEchoOptions options = TopicCommands.TopicEchoOptions.builder()
        .once(once)
        .timeoutSeconds(timeoutSeconds)
        .raw(raw)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_ECHO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.echo(ctx, topicName, messageType, options));
  }

  private static int executeTopicInfo(
      ServerCommandSource source, String topicName, String rawFlags) {
    boolean verbose = false;
    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          sendError(source, "Unsupported flag for /ros topic info: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicInfoOptions options =
        TopicCommands.TopicInfoOptions.builder().verbose(verbose).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_INFO,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.info(ctx, topicName, options));
  }

  private static int executeTopicPub(
      ServerCommandSource source,
      String topicName,
      String messageType,
      String payloadText,
      String rawFlags) {
    boolean once = false;
    double rateHz = 0.0;
    int times = 0;
    String qosProfile = "default";

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--once", "-1" -> once = true;
        case "-r", "--rate" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String rateToken = flags.get(++i);
          try {
            rateHz = Double.parseDouble(rateToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid rate value: " + rateToken);
            return 0;
          }
          if (rateHz < 0.0) {
            sendError(source, "Rate must be non-negative.");
            return 0;
          }
        }
        case "-t", "--times" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String timesToken = flags.get(++i);
          try {
            times = Integer.parseInt(timesToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid times value: " + timesToken);
            return 0;
          }
          if (times < 0) {
            sendError(source, "Times must be non-negative.");
            return 0;
          }
        }
        case "--qos-profile" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --qos-profile");
            return 0;
          }
          qosProfile = flags.get(++i);
        }
        default -> {
          if (flag.startsWith("--rate=")) {
            String rateToken = flag.substring("--rate=".length());
            try {
              rateHz = Double.parseDouble(rateToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid rate value: " + rateToken);
              return 0;
            }
            if (rateHz < 0.0) {
              sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--times=")) {
            String timesToken = flag.substring("--times=".length());
            try {
              times = Integer.parseInt(timesToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid times value: " + timesToken);
              return 0;
            }
            if (times < 0) {
              sendError(source, "Times must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--qos-profile=")) {
            qosProfile = flag.substring("--qos-profile=".length());
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic pub: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicPublishOptions options = TopicCommands.TopicPublishOptions.builder()
        .once(once)
        .rateHz(rateHz)
        .times(times)
        .qosProfile(qosProfile)
        .build();
    byte[] payload = payloadText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_PUB,
        options.encodeTrackingMetadata(),
        ctx -> TopicCommands.pub(ctx, topicName, messageType, payload, options));
  }

  private static int executeTopicHz(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    int window = 10000;
    boolean wallTime = false;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--wall-time" -> wallTime = true;
        case "--window" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic hz: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicHzOptions options =
        TopicCommands.TopicHzOptions.builder().window(window).wallTime(wallTime).build();
    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_HZ,
        trackingMetadata,
        ctx -> TopicCommands.hz(ctx, topicName, messageType, options));
  }

  private static int executeTopicBw(
      ServerCommandSource source, String topicName, String messageType, String rawFlags) {
    int window = 100;
    boolean wallTime = false;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--wall-time" -> wallTime = true;
        case "--window" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --window");
            return 0;
          }
          String windowToken = flags.get(++i);
          try {
            window = Integer.parseInt(windowToken);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid window value: " + windowToken);
            return 0;
          }
          if (window < 1) {
            sendError(source, "Window must be >= 1.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--window=")) {
            String windowToken = flag.substring("--window=".length());
            try {
              window = Integer.parseInt(windowToken);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid window value: " + windowToken);
              return 0;
            }
            if (window < 1) {
              sendError(source, "Window must be >= 1.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros topic bw: " + flag);
          return 0;
        }
      }
    }

    TopicCommands.TopicBwOptions options =
        TopicCommands.TopicBwOptions.builder().window(window).wallTime(wallTime).build();
    String trackingMetadata = "topic_name=" + topicName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.TOPIC_BW,
        trackingMetadata,
        ctx -> TopicCommands.bw(ctx, topicName, messageType, options));
  }

  private static int executeServiceList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    boolean count = false;
    boolean includeHidden = false;

    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        case "-c", "--count" -> count = true;
        case "--include-hidden-services" -> includeHidden = true;
        default -> {
          sendError(source, "Unsupported flag for /ros service list: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceListOptions options = ServiceCommands.ServiceListOptions.builder()
        .showTypes(showTypes)
        .countOnly(count)
        .includeHiddenServices(includeHidden)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.list(ctx, options));
  }

  private static int executeServiceType(ServerCommandSource source, String serviceName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_TYPE,
        serviceName,
        ctx -> ServiceCommands.type(ctx, serviceName));
  }

  private static int executeServiceFind(ServerCommandSource source, String serviceType) {
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_FIND,
        serviceType,
        ctx -> ServiceCommands.find(ctx, serviceType));
  }

  private static int executeServiceInfo(
      ServerCommandSource source, String serviceName, String rawFlags) {
    boolean verbose = false;
    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-v", "--verbose" -> verbose = true;
        default -> {
          sendError(source, "Unsupported flag for /ros service info: " + flag);
          return 0;
        }
      }
    }

    ServiceCommands.ServiceInfoOptions options =
        ServiceCommands.ServiceInfoOptions.builder().verbose(verbose).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_INFO,
        options.encodeTrackingMetadata(),
        ctx -> ServiceCommands.info(ctx, serviceName, options));
  }

  private static int executeServiceCall(
      ServerCommandSource source,
      String serviceName,
      String serviceType,
      String requestText,
      String rawFlags) {
    double timeoutSeconds = 0.0;
    int repeatCount = 0;
    double rateHz = 0.0;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --timeout");
            return 0;
          }
          String token = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + token);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        case "-r", "--repeat" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String token = flags.get(++i);
          try {
            repeatCount = Integer.parseInt(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid repeat value: " + token);
            return 0;
          }
          if (repeatCount < 0) {
            sendError(source, "Repeat must be non-negative.");
            return 0;
          }
        }
        case "--rate" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --rate");
            return 0;
          }
          String token = flags.get(++i);
          try {
            rateHz = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid rate value: " + token);
            return 0;
          }
          if (rateHz < 0.0) {
            sendError(source, "Rate must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String token = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + token);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--repeat=")) {
            String token = flag.substring("--repeat=".length());
            try {
              repeatCount = Integer.parseInt(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid repeat value: " + token);
              return 0;
            }
            if (repeatCount < 0) {
              sendError(source, "Repeat must be non-negative.");
              return 0;
            }
            continue;
          }

          if (flag.startsWith("--rate=")) {
            String token = flag.substring("--rate=".length());
            try {
              rateHz = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid rate value: " + token);
              return 0;
            }
            if (rateHz < 0.0) {
              sendError(source, "Rate must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros service call: " + flag);
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
    byte[] payload = requestText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.SERVICE_CALL,
        trackingMetadata,
        ctx -> ServiceCommands.call(ctx, serviceName, serviceType, payload, options));
  }

  private static int executeActionList(ServerCommandSource source, String rawFlags) {
    boolean showTypes = false;
    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-t", "--show-types" -> showTypes = true;
        default -> {
          sendError(source, "Unsupported flag for /ros action list: " + flag);
          return 0;
        }
      }
    }

    ActionCommands.ActionListOptions options =
        ActionCommands.ActionListOptions.builder().showTypes(showTypes).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ActionCommands.list(ctx, options));
  }

  private static int executeActionType(ServerCommandSource source, String actionName) {
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_TYPE,
        actionName,
        ctx -> ActionCommands.type(ctx, actionName));
  }

  private static int executeActionInfo(
      ServerCommandSource source, String actionName, String rawFlags) {
    boolean includeHidden = false;
    for (String flag : parseFlags(rawFlags)) {
      if ("--include-hidden".equals(flag)) {
        includeHidden = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros action info: " + flag);
      return 0;
    }

    ActionCommands.ActionInfoOptions options =
        ActionCommands.ActionInfoOptions.builder().includeHidden(includeHidden).build();
    String trackingMetadata = "action_name=" + actionName + ";" + options.encodeTrackingMetadata();
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_INFO,
        trackingMetadata,
        ctx -> ActionCommands.info(ctx, actionName, options));
  }

  private static int executeActionSendGoal(
      ServerCommandSource source,
      String actionName,
      String actionType,
      String goalText,
      String rawFlags) {
    boolean feedback = false;
    double timeoutSeconds = 0.0;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "-f", "--feedback" -> feedback = true;
        case "-t", "--timeout" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for " + flag);
            return 0;
          }
          String token = flags.get(++i);
          try {
            timeoutSeconds = Double.parseDouble(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid timeout value: " + token);
            return 0;
          }
          if (timeoutSeconds < 0.0) {
            sendError(source, "Timeout must be non-negative.");
            return 0;
          }
        }
        default -> {
          if (flag.startsWith("--timeout=")) {
            String token = flag.substring("--timeout=".length());
            try {
              timeoutSeconds = Double.parseDouble(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid timeout value: " + token);
              return 0;
            }
            if (timeoutSeconds < 0.0) {
              sendError(source, "Timeout must be non-negative.");
              return 0;
            }
            continue;
          }

          sendError(source, "Unsupported flag for /ros action send_goal: " + flag);
          return 0;
        }
      }
    }

    ActionCommands.ActionSendGoalOptions options = ActionCommands.ActionSendGoalOptions.builder()
        .feedback(feedback)
        .timeoutSeconds(timeoutSeconds)
        .build();
    String trackingMetadata = "action_name=" + actionName
        + ";action_type="
        + actionType
        + ";"
        + options.encodeTrackingMetadata();
    byte[] payload = goalText.getBytes(java.nio.charset.StandardCharsets.UTF_8);
    return executeLogicCommand(
        source,
        PendingRequestKind.ACTION_SEND_GOAL,
        trackingMetadata,
        ctx -> ActionCommands.sendGoal(ctx, actionName, actionType, payload, options));
  }

  private static int executeParamList(
      ServerCommandSource source, String nodeName, String rawFlags) {
    long depth = 0L;
    boolean includeTypes = false;
    String filterRegex = "";
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      switch (flag) {
        case "--depth" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --depth");
            return 0;
          }
          String token = flags.get(++i);
          try {
            depth = Long.parseLong(token);
          } catch (NumberFormatException ex) {
            sendError(source, "Invalid depth value: " + token);
            return 0;
          }
          if (depth < 0L) {
            sendError(source, "Depth must be non-negative.");
            return 0;
          }
        }
        case "--param-type" -> includeTypes = true;
        case "--filter" -> {
          if (i + 1 >= flags.size()) {
            sendError(source, "Missing value for --filter");
            return 0;
          }
          filterRegex = flags.get(++i);
        }
        case "--param-prefixes" -> {
          while (i + 1 < flags.size() && !flags.get(i + 1).startsWith("-")) {
            prefixes.add(flags.get(++i));
          }
        }
        default -> {
          if (flag.startsWith("--depth=")) {
            String token = flag.substring("--depth=".length());
            try {
              depth = Long.parseLong(token);
            } catch (NumberFormatException ex) {
              sendError(source, "Invalid depth value: " + token);
              return 0;
            }
            if (depth < 0L) {
              sendError(source, "Depth must be non-negative.");
              return 0;
            }
            continue;
          }
          if (flag.startsWith("--filter=")) {
            filterRegex = flag.substring("--filter=".length());
            continue;
          }

          sendError(source, "Unsupported flag for /ros param list: " + flag);
          return 0;
        }
      }
    }

    ParamCommands.ParamListOptions options = ParamCommands.ParamListOptions.builder()
        .nodeName(nodeName)
        .prefixes(prefixes.toArray(String[]::new))
        .depth(depth)
        .includeTypes(includeTypes)
        .filterRegex(filterRegex)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_LIST,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.list(ctx, options));
  }

  private static int executeParamGet(
      ServerCommandSource source, String nodeName, String paramName, String rawFlags) {
    boolean hideType = false;
    for (String flag : parseFlags(rawFlags)) {
      if ("--hide-type".equals(flag)) {
        hideType = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros param get: " + flag);
      return 0;
    }

    ParamCommands.ParamGetOptions options = ParamCommands.ParamGetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .hideType(hideType)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_GET,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.get(ctx, options));
  }

  private static int executeParamSet(
      ServerCommandSource source,
      String nodeName,
      String paramName,
      String valueText,
      String rawFlags) {
    double timeoutSeconds = 0.0;

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--timeout".equals(flag)) {
        if (i + 1 >= flags.size()) {
          sendError(source, "Missing value for --timeout");
          return 0;
        }
        String token = flags.get(++i);
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      if (flag.startsWith("--timeout=")) {
        String token = flag.substring("--timeout=".length());
        try {
          timeoutSeconds = Double.parseDouble(token);
        } catch (NumberFormatException ex) {
          sendError(source, "Invalid timeout value: " + token);
          return 0;
        }
        if (timeoutSeconds < 0.0) {
          sendError(source, "Timeout must be non-negative.");
          return 0;
        }
        continue;
      }

      sendError(source, "Unsupported flag for /ros param set: " + flag);
      return 0;
    }

    ParamCommands.ParamSetOptions options = ParamCommands.ParamSetOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .valueText(valueText)
        .timeoutSeconds(timeoutSeconds)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_SET,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.set(ctx, options));
  }

  private static int executeParamDescribe(
      ServerCommandSource source, String nodeName, String paramName) {
    ParamCommands.ParamDescribeOptions options = ParamCommands.ParamDescribeOptions.builder()
        .nodeName(nodeName)
        .paramName(paramName)
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DESCRIBE,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.describe(ctx, options));
  }

  private static int executeParamDump(
      ServerCommandSource source, String nodeName, String rawFlags) {
    List<String> prefixes = new java.util.ArrayList<>();

    List<String> flags = parseFlags(rawFlags);
    for (int i = 0; i < flags.size(); i++) {
      String flag = flags.get(i);
      if ("--param-prefixes".equals(flag)) {
        while (i + 1 < flags.size() && !flags.get(i + 1).startsWith("-")) {
          prefixes.add(flags.get(++i));
        }
        continue;
      }
      sendError(source, "Unsupported flag for /ros param dump: " + flag);
      return 0;
    }

    ParamCommands.ParamDumpOptions options = ParamCommands.ParamDumpOptions.builder()
        .nodeName(nodeName)
        .prefixes(prefixes.toArray(String[]::new))
        .build();
    return executeLogicCommand(
        source,
        PendingRequestKind.PARAM_DUMP,
        options.encodeTrackingMetadata(),
        ctx -> ParamCommands.dump(ctx, options));
  }

  private static int executeInterfaceShow(
      ServerCommandSource source, String interfaceType, String rawFlags) {
    boolean noComments = false;
    for (String flag : parseFlags(rawFlags)) {
      if ("--no-comments".equals(flag)) {
        noComments = true;
        continue;
      }
      sendError(source, "Unsupported flag for /ros interface show: " + flag);
      return 0;
    }

    InterfaceCommands.InterfaceShowOptions options =
        InterfaceCommands.InterfaceShowOptions.builder().noComments(noComments).build();
    return executeLogicCommand(
        source,
        PendingRequestKind.INTERFACE_SHOW,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.show(ctx, interfaceType, options));
  }

  private static int executeInterfaceList(ServerCommandSource source, String rawFlags) {
    boolean onlyMsgs = false;
    boolean onlySrvs = false;
    boolean onlyActions = false;

    for (String flag : parseFlags(rawFlags)) {
      switch (flag) {
        case "-m", "--only-msgs" -> onlyMsgs = true;
        case "-s", "--only-srvs" -> onlySrvs = true;
        case "-a", "--only-actions" -> onlyActions = true;
        default -> {
          sendError(source, "Unsupported flag for /ros interface list: " + flag);
          return 0;
        }
      }
    }

    int selected = (onlyMsgs ? 1 : 0) + (onlySrvs ? 1 : 0) + (onlyActions ? 1 : 0);
    if (selected > 1) {
      sendError(source, "Choose at most one of -m, -s, or -a for /ros interface list.");
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
    return executeLogicCommand(
        source,
        PendingRequestKind.INTERFACE_LIST,
        options.encodeTrackingMetadata(),
        ctx -> InterfaceCommands.list(ctx, options));
  }

  private static int executeLogicCommand(
      ServerCommandSource source,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    return executeLogicCommand(source, null, null, commandLogic);
  }

  private static int executeLogicCommand(
      ServerCommandSource source,
      PendingRequestKind pendingKind,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    return executeLogicCommand(source, pendingKind, null, commandLogic);
  }

  private static int executeLogicCommand(
      ServerCommandSource source,
      PendingRequestKind pendingKind,
      String trackingMetadata,
      java.util.function.Function<CommandContext, CommandResult> commandLogic) {
    BridgeManager manager = getBridgeManager();
    if (manager == null) {
      sendError(source, "Roscraft is not ready yet.");
      return 0;
    }

    UUID requesterUuid = requesterUuid(source);
    CommandResult result;
    try {
      result = commandLogic.apply(new CommandContext(manager, requesterUuid));
    } catch (IllegalStateException e) {
      sendError(source, e.getMessage());
      return 0;
    }

    if (!result.success()) {
      sendError(source, result.message());
      return 0;
    }

    if (pendingKind != null && result.requestId() != 0L) {
      RoscraftMod mod = getMod();
      if (mod != null) {
        mod.trackRequest(result.requestId(), pendingKind, requesterUuid, trackingMetadata);
      }
    }

    sendStyled(source, prefix().append(Text.literal(result.message()).formatted(Formatting.GREEN)));
    return 1;
  }

  private static List<String> parseFlags(String rawFlags) {
    if (rawFlags == null || rawFlags.isBlank()) {
      return List.of();
    }
    return Arrays.stream(rawFlags.trim().split("\\s+"))
        .filter(token -> !token.isBlank())
        .toList();
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
              + "Use /ros probe or /ros node list to verify connectivity.");
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
        prefix()
            .append(Text.literal("Player list request #").formatted(Formatting.GOLD))
            .append(Text.literal(String.valueOf(requestId)).formatted(Formatting.YELLOW))
            .append(Text.literal(" sent. Waiting for reply...").formatted(Formatting.GRAY)));
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
    var mods = net.fabricmc.loader.api.FabricLoader.getInstance()
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
