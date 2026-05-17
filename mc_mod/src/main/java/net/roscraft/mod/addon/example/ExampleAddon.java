package net.roscraft.mod.addon.example;

import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.nio.charset.StandardCharsets;
import java.util.List;
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.addon.AddonContext;
import net.roscraft.mod.addon.RoscraftAddon;

public final class ExampleAddon implements RoscraftAddon {

  private AddonContext ctx;
  private Subscription topicSub;

  @Override
  public String addonId() {
    return "example";
  }

  @Override
  public void init(AddonContext ctx) {
    this.ctx = ctx;
    RoscraftMod.LOGGER.info("ExampleAddon initialised.");
  }

  @Override
  public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return List.of(CommandManager.literal("example")
        .then(CommandManager.literal("ping").executes(c -> executePing(c.getSource())))
        .then(CommandManager.literal("hello")
            .then(CommandManager.argument("name", StringArgumentType.word())
                .executes(
                    c -> executeHello(c.getSource(), StringArgumentType.getString(c, "name")))))
        .then(CommandManager.literal("sub")
            .then(CommandManager.argument("topic", StringArgumentType.string())
                .executes(c ->
                    executeSubscribe(c.getSource(), StringArgumentType.getString(c, "topic")))))
        .then(CommandManager.literal("players").executes(c -> executePlayers(c.getSource())))
        .then(CommandManager.literal("graph").executes(c -> executeGraph(c.getSource())))
        .then(CommandManager.literal("call")
            .then(CommandManager.argument("service", StringArgumentType.string())
                .then(CommandManager.argument("type", StringArgumentType.string())
                    .then(CommandManager.argument("request", StringArgumentType.string())
                        .executes(c -> executeServiceCall(
                            c.getSource(),
                            StringArgumentType.getString(c, "service"),
                            StringArgumentType.getString(c, "type"),
                            StringArgumentType.getString(c, "request"))))))));
  }

  @Override
  public void onBridgeEvent(BridgeEvent event) {
    if (event instanceof BridgeEvent.AddonEvent ae && "say".equals(ae.eventType())) {
      RoscraftMod.LOGGER.info(
          "ExampleAddon received 'say': {}", new String(ae.payload(), StandardCharsets.UTF_8));
      return;
    }

    switch (event) {
      case BridgeEvent.TopicPayload p -> RoscraftMod.LOGGER.info(
          "ExampleAddon topic: topic={} type={} bytes={}",
          p.topicName(),
          p.messageType(),
          p.payloadLength());

      case BridgeEvent.GraphSnapshot s -> RoscraftMod.LOGGER.info(
          "ExampleAddon graph: {} nodes, {} topics, {} services, {} actions",
          s.nodes().size(),
          s.topics().size(),
          s.services().size(),
          s.actions().size());

      case BridgeEvent.PlayerList pl -> RoscraftMod.LOGGER.info(
          "ExampleAddon players: requestId={} count={}", pl.requestId(), pl.size());

      case BridgeEvent.ServiceCallResponse r -> RoscraftMod.LOGGER.info(
          "ExampleAddon service call: service={} requestId={} success={}",
          r.serviceName(),
          r.requestId(),
          r.success());

      case BridgeEvent.BridgeError e -> RoscraftMod.LOGGER.warn(
          "ExampleAddon error: requestId={} code={} message={}",
          e.requestId(),
          e.errorCode(),
          e.errorMessage());

      default -> {}
    }
  }

  @Override
  public void shutdown() {
    if (topicSub != null) {
      topicSub.close();
    }
    RoscraftMod.LOGGER.info("ExampleAddon shutting down.");
  }

  private int executePing(ServerCommandSource source) {
    if (!ctx.isBridgeConnected()) return bridgeNotConnected(source);
    long requestId =
        ctx.sendEvent("ping", "hello from example addon".getBytes(StandardCharsets.UTF_8), false);
    source.sendMessage(Text.literal("[Roscraft] ")
        .formatted(Formatting.AQUA)
        .append(Text.literal("Ping sent, requestId=" + requestId).formatted(Formatting.GREEN)));
    return 1;
  }

  private int executeHello(ServerCommandSource source, String name) {
    source.sendMessage(Text.literal("[Roscraft] ")
        .formatted(Formatting.AQUA)
        .append(
            Text.literal("Hello, " + name + " from ExampleAddon!").formatted(Formatting.YELLOW)));
    return 1;
  }

  private int executeSubscribe(ServerCommandSource source, String topic) {
    var sub = ctx.subscribeTopic(topic, "");
    if (sub.isEmpty()) return bridgeNotConnected(source);
    topicSub = sub.get();
    source.sendMessage(Text.literal("[Roscraft] ")
        .formatted(Formatting.AQUA)
        .append(Text.literal("Subscribed to " + topic).formatted(Formatting.GREEN)));
    return 1;
  }

  private int executePlayers(ServerCommandSource source) {
    return ctx.bridgeIfConnected()
        .map(bridge -> {
          long rid = bridge.queryPlayers();
          source.sendMessage(Text.literal("[Roscraft] ")
              .formatted(Formatting.AQUA)
              .append(
                  Text.literal("Player query sent, requestId=" + rid).formatted(Formatting.GREEN)));
          return 1;
        })
        .orElseGet(() -> bridgeNotConnected(source));
  }

  private int executeGraph(ServerCommandSource source) {
    return ctx.bridgeIfConnected()
        .map(bridge -> {
          long rid = bridge.graph().snapshot();
          source.sendMessage(Text.literal("[Roscraft] ")
              .formatted(Formatting.AQUA)
              .append(
                  Text.literal("Graph query sent, requestId=" + rid).formatted(Formatting.GREEN)));
          return 1;
        })
        .orElseGet(() -> bridgeNotConnected(source));
  }

  private int executeServiceCall(
      ServerCommandSource source, String service, String type, String request) {
    return ctx.bridgeIfConnected()
        .map(bridge -> {
          long rid = bridge
              .services()
              .call(
                  service,
                  type,
                  request.getBytes(StandardCharsets.UTF_8),
                  BridgeOperations.ServiceOps.ServiceCallOptions.defaults());
          source.sendMessage(Text.literal("[Roscraft] ")
              .formatted(Formatting.AQUA)
              .append(Text.literal("Service call sent to " + service + ", requestId=" + rid)
                  .formatted(Formatting.GREEN)));
          return 1;
        })
        .orElseGet(() -> bridgeNotConnected(source));
  }

  private static int bridgeNotConnected(ServerCommandSource source) {
    source.sendMessage(Text.literal("[Roscraft] ")
        .formatted(Formatting.AQUA)
        .append(Text.literal("Bridge not connected. Run /ros connection connect first.")
            .formatted(Formatting.RED)));
    return 0;
  }
}
