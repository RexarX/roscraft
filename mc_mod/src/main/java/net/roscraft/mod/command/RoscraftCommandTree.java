package net.roscraft.mod.command;

import com.mojang.brigadier.arguments.IntegerArgumentType;
import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;

final class RoscraftCommandTree {

  private RoscraftCommandTree() {}

  static LiteralArgumentBuilder<ServerCommandSource> buildRoot() {
    return CommandManager.literal("ros")
        .requires(src -> src.hasPermissionLevel(2))
        .executes(ctx -> RoscraftCommandActions.executeHelp(ctx.getSource()))
        .then(buildConnectionCommands())
        .then(buildNodeCommands())
        .then(buildTopicCommands())
        .then(buildServiceCommands())
        .then(buildActionCommands())
        .then(buildParamCommands())
        .then(buildInterfaceCommands())
        .then(CommandManager.literal("players")
            .executes(ctx -> RoscraftCommandActions.executePlayers(ctx.getSource())));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildConnectionCommands() {
    return CommandManager.literal("connection")
        .executes(ctx -> RoscraftCommandActions.executeConnectionStatus(ctx.getSource()))
        .then(CommandManager.literal("status")
            .executes(ctx -> RoscraftCommandActions.executeConnectionStatus(ctx.getSource())))
        .then(CommandManager.literal("connect")
            .executes(ctx -> RoscraftCommandActions.executeConnect(ctx.getSource())))
        .then(CommandManager.literal("disconnect")
            .executes(ctx -> RoscraftCommandActions.executeDisconnect(ctx.getSource())))
        .then(CommandManager.literal("mode")
            .then(CommandManager.argument("type", StringArgumentType.word())
                .suggests((context, builder) -> {
                  builder.suggest("network");
                  builder.suggest("jni");
                  return builder.buildFuture();
                })
                .executes(ctx -> RoscraftCommandActions.executeSetMode(
                    ctx.getSource(), StringArgumentType.getString(ctx, "type")))))
        .then(CommandManager.literal("host")
            .then(CommandManager.argument("ip", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeSetHost(
                    ctx.getSource(), StringArgumentType.getString(ctx, "ip")))))
        .then(CommandManager.literal("port")
            .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                .executes(ctx -> RoscraftCommandActions.executeSetPort(
                    ctx.getSource(), IntegerArgumentType.getInteger(ctx, "port")))))
        .then(CommandManager.literal("endpoint")
            .then(CommandManager.argument("ip", StringArgumentType.string())
                .then(CommandManager.argument("port", IntegerArgumentType.integer(1, 65535))
                    .executes(ctx -> RoscraftCommandActions.executeSetEndpoint(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "ip"),
                        IntegerArgumentType.getInteger(ctx, "port"))))))
        .then(CommandManager.literal("probe")
            .executes(ctx -> RoscraftCommandActions.executeProbe(ctx.getSource())));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildNodeCommands() {
    return CommandManager.literal("node")
        .then(CommandManager.literal("list")
            .executes(ctx -> RoscraftCommandActions.executeNodeList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-a");
                  builder.suggest("--all");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  return builder.buildFuture();
                })
                .executes(ctx -> RoscraftCommandActions.executeNodeList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeNodeInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--include-hidden");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeNodeInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildTopicCommands() {
    return CommandManager.literal("topic")
        .then(CommandManager.literal("list")
            .executes(ctx -> RoscraftCommandActions.executeTopicList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  builder.suggest("--include-hidden-topics");
                  return builder.buildFuture();
                })
                .executes(ctx -> RoscraftCommandActions.executeTopicList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name")))))
        .then(CommandManager.literal("find")
            .then(CommandManager.argument("topic_type", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicFind(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_type")))))
        .then(CommandManager.literal("echo")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicEcho(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), "", ""))
                .then(CommandManager.argument("tail", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("std_msgs/msg/String");
                      builder.suggest("--once");
                      builder.suggest("--timeout 5");
                      builder.suggest("--raw");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeTopicEchoTail(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "tail"))))))
        .then(CommandManager.literal("pub")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .then(CommandManager.argument("message_type", StringArgumentType.string())
                    .then(CommandManager.argument("payload", StringArgumentType.string())
                        .executes(ctx -> RoscraftCommandActions.executeTopicPub(
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
                            .executes(ctx -> RoscraftCommandActions.executeTopicPub(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "topic_name"),
                                StringArgumentType.getString(ctx, "message_type"),
                                StringArgumentType.getString(ctx, "payload"),
                                StringArgumentType.getString(ctx, "flags"))))))))
        .then(CommandManager.literal("hz")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicHz(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), "", ""))
                .then(CommandManager.argument("tail", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("std_msgs/msg/String");
                      builder.suggest("--window 10000");
                      builder.suggest("--wall-time");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeTopicHzTail(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "tail"))))))
        .then(CommandManager.literal("bw")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicBw(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), "", ""))
                .then(CommandManager.argument("tail", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("std_msgs/msg/String");
                      builder.suggest("--window 100");
                      builder.suggest("--wall-time");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeTopicBwTail(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "tail"))))))
        .then(CommandManager.literal("delay")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicDelay(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), "", ""))
                .then(CommandManager.argument("tail", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("std_msgs/msg/Header");
                      builder.suggest("--window 10");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeTopicDelayTail(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "tail"))))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("topic_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeTopicInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "topic_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-v");
                      builder.suggest("--verbose");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeTopicInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "topic_name"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildServiceCommands() {
    return CommandManager.literal("service")
        .then(CommandManager.literal("list")
            .executes(ctx -> RoscraftCommandActions.executeServiceList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  builder.suggest("-c");
                  builder.suggest("--count");
                  builder.suggest("--include-hidden-services");
                  return builder.buildFuture();
                })
                .executes(ctx -> RoscraftCommandActions.executeServiceList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeServiceType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_name")))))
        .then(CommandManager.literal("find")
            .then(CommandManager.argument("service_type", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeServiceFind(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_type")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeServiceInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "service_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("-v");
                      builder.suggest("--verbose");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeServiceInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "service_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("call")
            .then(CommandManager.argument("service_name", StringArgumentType.string())
                .then(CommandManager.argument("service_type", StringArgumentType.string())
                    .then(CommandManager.argument("request", StringArgumentType.string())
                        .executes(ctx -> RoscraftCommandActions.executeServiceCall(
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
                            .executes(ctx -> RoscraftCommandActions.executeServiceCall(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "service_name"),
                                StringArgumentType.getString(ctx, "service_type"),
                                StringArgumentType.getString(ctx, "request"),
                                StringArgumentType.getString(ctx, "flags"))))))));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildActionCommands() {
    return CommandManager.literal("action")
        .then(CommandManager.literal("list")
            .executes(ctx -> RoscraftCommandActions.executeActionList(ctx.getSource(), ""))
            .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                .suggests((context, builder) -> {
                  builder.suggest("-t");
                  builder.suggest("--show-types");
                  return builder.buildFuture();
                })
                .executes(ctx -> RoscraftCommandActions.executeActionList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("type")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeActionType(
                    ctx.getSource(), StringArgumentType.getString(ctx, "action_name")))))
        .then(CommandManager.literal("info")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeActionInfo(
                    ctx.getSource(), StringArgumentType.getString(ctx, "action_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--include-hidden");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeActionInfo(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "action_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("send_goal")
            .then(CommandManager.argument("action_name", StringArgumentType.string())
                .then(CommandManager.argument("action_type", StringArgumentType.string())
                    .then(CommandManager.argument("goal", StringArgumentType.string())
                        .executes(ctx -> RoscraftCommandActions.executeActionSendGoal(
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
                            .executes(ctx -> RoscraftCommandActions.executeActionSendGoal(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "action_name"),
                                StringArgumentType.getString(ctx, "action_type"),
                                StringArgumentType.getString(ctx, "goal"),
                                StringArgumentType.getString(ctx, "flags"))))))));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildParamCommands() {
    return CommandManager.literal("param")
        .then(CommandManager.literal("list")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeParamList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--depth 0");
                      builder.suggest("--param-prefixes foo bar");
                      builder.suggest("--param-type");
                      builder.suggest("--filter .*rate.*");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeParamList(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("get")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .executes(ctx -> RoscraftCommandActions.executeParamGet(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "param_name"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--hide-type");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> RoscraftCommandActions.executeParamGet(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "node_name"),
                            StringArgumentType.getString(ctx, "param_name"),
                            StringArgumentType.getString(ctx, "flags")))))))
        .then(CommandManager.literal("set")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .then(CommandManager.argument("value", StringArgumentType.string())
                        .executes(ctx -> RoscraftCommandActions.executeParamSet(
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
                            .executes(ctx -> RoscraftCommandActions.executeParamSet(
                                ctx.getSource(),
                                StringArgumentType.getString(ctx, "node_name"),
                                StringArgumentType.getString(ctx, "param_name"),
                                StringArgumentType.getString(ctx, "value"),
                                StringArgumentType.getString(ctx, "flags"))))))))
        .then(CommandManager.literal("describe")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("param_name", StringArgumentType.string())
                    .executes(ctx -> RoscraftCommandActions.executeParamDescribe(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "param_name"))))))
        .then(CommandManager.literal("dump")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeParamDump(
                    ctx.getSource(), StringArgumentType.getString(ctx, "node_name"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--param-prefixes foo bar");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeParamDump(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "flags"))))))
        .then(CommandManager.literal("load")
            .then(CommandManager.argument("node_name", StringArgumentType.string())
                .then(CommandManager.argument("parameter_file", StringArgumentType.string())
                    .executes(ctx -> RoscraftCommandActions.executeParamLoad(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "node_name"),
                        StringArgumentType.getString(ctx, "parameter_file"),
                        ""))
                    .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                        .suggests((context, builder) -> {
                          builder.suggest("--timeout 5");
                          builder.suggest("--no-use-wildcard");
                          return builder.buildFuture();
                        })
                        .executes(ctx -> RoscraftCommandActions.executeParamLoad(
                            ctx.getSource(),
                            StringArgumentType.getString(ctx, "node_name"),
                            StringArgumentType.getString(ctx, "parameter_file"),
                            StringArgumentType.getString(ctx, "flags")))))));
  }

  static LiteralArgumentBuilder<ServerCommandSource> buildInterfaceCommands() {
    return CommandManager.literal("interface")
        .then(CommandManager.literal("list")
            .executes(ctx -> RoscraftCommandActions.executeInterfaceList(ctx.getSource(), ""))
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
                .executes(ctx -> RoscraftCommandActions.executeInterfaceList(
                    ctx.getSource(), StringArgumentType.getString(ctx, "flags")))))
        .then(CommandManager.literal("show")
            .then(CommandManager.argument("interface_type", StringArgumentType.string())
                .executes(ctx -> RoscraftCommandActions.executeInterfaceShow(
                    ctx.getSource(), StringArgumentType.getString(ctx, "interface_type"), ""))
                .then(CommandManager.argument("flags", StringArgumentType.greedyString())
                    .suggests((context, builder) -> {
                      builder.suggest("--no-comments");
                      return builder.buildFuture();
                    })
                    .executes(ctx -> RoscraftCommandActions.executeInterfaceShow(
                        ctx.getSource(),
                        StringArgumentType.getString(ctx, "interface_type"),
                        StringArgumentType.getString(ctx, "flags"))))));
  }
}
