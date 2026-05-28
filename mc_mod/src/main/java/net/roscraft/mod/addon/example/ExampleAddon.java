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
import net.roscraft.mod.addon.AbstractRoscraftAddon;
import net.roscraft.mod.addon.minecraft.RoscraftAddonCommands;

/**
 * Reference addon demonstrating bridge operations, commands, and event handling.
 *
 * <p>Not registered in {@code fabric.mod.json} by default. To enable during development,
 * add {@code "net.roscraft.mod.addon.example.ExampleAddon"} to the {@code roscraft:addon}
 * entrypoint array.
 */
public final class ExampleAddon extends AbstractRoscraftAddon implements RoscraftAddonCommands {

  private Subscription topicSub;

  @Override
  public String addonId() {
    return "example";
  }

  @Override
  protected void configure() {
    RoscraftMod.LOGGER.info("ExampleAddon initialised.");

    on(
        BridgeEvent.TopicPayload.class,
        p -> RoscraftMod.LOGGER.info(
            "ExampleAddon topic: topic={} type={} bytes={}",
            p.topicName(),
            p.messageType(),
            p.payloadLength()));

    on(
        BridgeEvent.GraphSnapshot.class,
        s -> RoscraftMod.LOGGER.info(
            "ExampleAddon graph: {} nodes, {} topics, {} services, {} actions",
            s.nodes().size(),
            s.topics().size(),
            s.services().size(),
            s.actions().size()));

    on(
        BridgeEvent.PlayerList.class,
        pl -> RoscraftMod.LOGGER.info(
            "ExampleAddon players: requestId={} count={}", pl.requestId(), pl.size()));

    on(
        BridgeEvent.ServiceCallResponse.class,
        r -> RoscraftMod.LOGGER.info(
            "ExampleAddon service call: service={} requestId={} success={}",
            r.serviceName(),
            r.requestId(),
            r.success()));

    on(
        BridgeEvent.BridgeError.class,
        e -> RoscraftMod.LOGGER.warn(
            "ExampleAddon error: requestId={} code={} message={}",
            e.requestId(),
            e.errorCode(),
            e.errorMessage()));

    onSignal(
        "say",
        ae -> RoscraftMod.LOGGER.info(
            "ExampleAddon received 'say': {}", new String(ae.payload(), StandardCharsets.UTF_8)));
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
  protected void onShutdown() {
    if (topicSub != null) {
      topicSub.close();
    }
    RoscraftMod.LOGGER.info("ExampleAddon shutting down.");
  }

  private int executePing(ServerCommandSource source) {
    if (!ctx.isBridgeConnected()) {
      return bridgeNotConnected(source);
    }
    long requestId = ctx.sendEvent("ping", "hello from example addon", false);
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
    if (sub.isEmpty()) {
      return bridgeNotConnected(source);
    }
    topicSub = sub.get();
    source.sendMessage(Text.literal("[Roscraft] ")
        .formatted(Formatting.AQUA)
        .append(Text.literal("Subscribed to " + topic).formatted(Formatting.GREEN)));
    return 1;
  }

  private int executePlayers(ServerCommandSource source) {
    return ctx.mapBridge(bridge -> {
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
    return ctx.mapBridge(bridge -> {
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
    return ctx.mapBridge(bridge -> {
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
