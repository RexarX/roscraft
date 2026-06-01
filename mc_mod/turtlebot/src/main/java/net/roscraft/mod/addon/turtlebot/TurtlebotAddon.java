package net.roscraft.mod.addon.turtlebot;

import com.mojang.brigadier.arguments.StringArgumentType;
import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.addon.AbstractRoscraftAddon;
import net.roscraft.mod.addon.AddonContext;
import net.roscraft.mod.addon.minecraft.RoscraftAddonCommands;

public final class TurtlebotAddon extends AbstractRoscraftAddon implements RoscraftAddonCommands {

  private static final String CMD_VEL_SUFFIX = "roscraft/turtlebot/cmd_vel";
  private static final String MOVEMENT_STATE_SUFFIX = "roscraft/turtlebot/movement/state";
  private static final String LIFECYCLE_STATE_SUFFIX = "roscraft/turtlebot/lifecycle/state";
  private static final String SPAWN_SERVICE_SUFFIX = "roscraft/turtlebot/lifecycle/spawn";
  private static final String DESPAWN_SERVICE_SUFFIX = "roscraft/turtlebot/lifecycle/despawn";

  private static final String TWIST_TYPE = "geometry_msgs/msg/Twist";
  private static final String STRING_TYPE = "std_msgs/msg/String";
  private static final String BOOL_TYPE = "std_msgs/msg/Bool";
  private static final String TRIGGER_TYPE = "std_srvs/srv/Trigger";
  private static final byte[] TRIGGER_REQUEST = "{}".getBytes(StandardCharsets.UTF_8);

  private final Map<String, TurtleSession> sessions = new LinkedHashMap<>();
  private final Set<String> knownSpawnServices = ConcurrentHashMap.newKeySet();
  private final Map<Long, String> pendingLifecycleNamespaces = new ConcurrentHashMap<>();
  private volatile MinecraftServer server;
  private TurtleWorldOps worldOps;

  @Override
  public String addonId() {
    return "turtlebot";
  }

  @Override
  protected void configure() {
    RoscraftMod.LOGGER.info("Turtlebot addon initialised.");
    worldOps = new TurtleWorldOps(() -> server);

    ServerLifecycleEvents.SERVER_STARTED.register(activeServer -> server = activeServer);
    ServerLifecycleEvents.SERVER_STOPPING.register(activeServer -> server = null);

    on(BridgeEvent.GraphSnapshot.class, this::handleGraphSnapshot);
    on(BridgeEvent.TopicPayload.class, this::handleTopicPayload);
    on(BridgeEvent.ServiceCallResponse.class, this::handleServiceCallResponse);
    on(BridgeEvent.BridgeError.class, this::handleBridgeError);

    ctx.bridgeIfConnected().ifPresent(bridge -> bridge.graph().snapshot());
  }

  @Override
  public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return List.of(CommandManager.literal("turtlebot")
        .executes(c -> executeStatus(c.getSource()))
        .then(CommandManager.literal("status").executes(c -> executeStatus(c.getSource())))
        .then(CommandManager.literal("refresh").executes(c -> executeRefresh(c.getSource())))
        .then(CommandManager.literal("watch")
            .then(CommandManager.argument("namespace", StringArgumentType.string())
                .executes(c ->
                    executeWatch(c.getSource(), StringArgumentType.getString(c, "namespace")))))
        .then(CommandManager.literal("forget")
            .then(CommandManager.argument("namespace", StringArgumentType.string())
                .executes(c ->
                    executeForget(c.getSource(), StringArgumentType.getString(c, "namespace")))))
        .then(CommandManager.literal("spawn")
            .then(CommandManager.argument("namespace", StringArgumentType.string())
                .executes(c ->
                    executeSpawn(c.getSource(), StringArgumentType.getString(c, "namespace"), null))
                .then(CommandManager.literal("at")
                    .then(CommandManager.argument("player", StringArgumentType.string())
                        .executes(c -> executeSpawn(
                            c.getSource(),
                            StringArgumentType.getString(c, "namespace"),
                            StringArgumentType.getString(c, "player")))))))
        .then(CommandManager.literal("despawn")
            .then(CommandManager.argument("namespace", StringArgumentType.string())
                .executes(c ->
                    executeDespawn(c.getSource(), StringArgumentType.getString(c, "namespace"))))));
  }

  @Override
  protected void onShutdown() {
    for (TurtleSession session : sessions.values()) {
      worldOps.despawn(session);
      session.close();
    }
    sessions.clear();
    knownSpawnServices.clear();
    pendingLifecycleNamespaces.clear();
    server = null;
  }

  private int executeStatus(ServerCommandSource source) {
    if (sessions.isEmpty()) {
      source.sendMessage(RoscraftMod.prefix()
          .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
          .append(Text.literal("No turtles are being tracked yet.").formatted(Formatting.YELLOW)));
      return 1;
    }

    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Tracked turtles: " + sessions.size()).formatted(Formatting.YELLOW)));

    for (var entry : sessions.entrySet()) {
      source.sendMessage(renderStatusLine(entry.getKey(), entry.getValue()));
    }
    return 1;
  }

  private int executeRefresh(ServerCommandSource source) {
    if (!ctx.isBridgeConnected()) {
      return bridgeNotConnected(source);
    }

    ctx.bridgeIfConnected().ifPresent(bridge -> bridge.graph().snapshot());
    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Graph refresh requested.").formatted(Formatting.GREEN)));
    return 1;
  }

  private int executeWatch(ServerCommandSource source, String rawNamespace) {
    if (!ctx.isBridgeConnected()) {
      return bridgeNotConnected(source);
    }

    String namespace = normalizeNamespace(rawNamespace);
    if (sessions.containsKey(namespace)) {
      source.sendMessage(RoscraftMod.prefix()
          .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
          .append(Text.literal("Already tracking ").formatted(Formatting.YELLOW))
          .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA)));
      return 1;
    }

    if (!trackNamespace(namespace)) {
      source.sendMessage(errorText(
          "Unable to subscribe to turtle topics for " + displayNamespace(namespace) + "."));
      return 0;
    }

    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Tracking ").formatted(Formatting.GREEN))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA)));
    source.sendMessage(renderStatusLine(namespace, sessions.get(namespace)));
    return 1;
  }

  private int executeForget(ServerCommandSource source, String rawNamespace) {
    String namespace = normalizeNamespace(rawNamespace);
    TurtleSession session = sessions.remove(namespace);
    if (session == null) {
      source.sendMessage(errorText("No tracked turtle named " + displayNamespace(namespace) + "."));
      return 0;
    }

    worldOps.despawn(session);
    session.close();
    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Stopped tracking ").formatted(Formatting.YELLOW))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA)));
    return 1;
  }

  private int executeSpawn(ServerCommandSource source, String rawNamespace, String playerName) {
    if (!ctx.isBridgeConnected()) {
      return bridgeNotConnected(source);
    }

    String namespace = normalizeNamespace(rawNamespace);
    TurtleSession session = sessions.computeIfAbsent(namespace, TurtleSession::new);
    if (playerName == null || playerName.isBlank()) {
      session.setPendingSpawnAtWorld();
    } else {
      session.setPendingSpawnAtPlayer(playerName);
    }

    if (!trackNamespace(namespace)) {
      source.sendMessage(errorText(
          "Unable to subscribe to turtle topics for " + displayNamespace(namespace) + "."));
      return 0;
    }

    String spawnService = spawnServiceTopic(namespace);
    if (!isSpawnServiceKnown(spawnService)) {
      source.sendMessage(spawnServiceUnavailableText(namespace, spawnService));
      return 0;
    }

    long requestId = callLifecycleService(spawnService);
    if (requestId == AddonContext.DISCONNECTED) {
      return bridgeNotConnected(source);
    }
    pendingLifecycleNamespaces.put(requestId, namespace);

    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Spawn requested for ").formatted(Formatting.GREEN))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
        .append(Text.literal(" (requestId=" + requestId + ")").formatted(Formatting.GRAY)));
    return 1;
  }

  private int executeDespawn(ServerCommandSource source, String rawNamespace) {
    if (!ctx.isBridgeConnected()) {
      return bridgeNotConnected(source);
    }

    String namespace = normalizeNamespace(rawNamespace);
    if (!sessions.containsKey(namespace) && !trackNamespace(namespace)) {
      source.sendMessage(errorText(
          "Unable to subscribe to turtle topics for " + displayNamespace(namespace) + "."));
      return 0;
    }

    long requestId = callLifecycleService(despawnServiceTopic(namespace));
    if (requestId == AddonContext.DISCONNECTED) {
      return bridgeNotConnected(source);
    }
    pendingLifecycleNamespaces.put(requestId, namespace);

    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Despawn requested for ").formatted(Formatting.YELLOW))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
        .append(Text.literal(" (requestId=" + requestId + ")").formatted(Formatting.GRAY)));
    return 1;
  }

  private long callLifecycleService(String serviceName) {
    var options = new BridgeOperations.ServiceOps.ServiceCallOptions(10.0, 0, 0.0);
    return ctx.mapBridge(
            bridge -> bridge.services().call(serviceName, TRIGGER_TYPE, TRIGGER_REQUEST, options))
        .orElse(AddonContext.DISCONNECTED);
  }

  private void handleGraphSnapshot(BridgeEvent.GraphSnapshot snapshot) {
    knownSpawnServices.clear();
    for (var service : snapshot.services()) {
      String normalized = normalizeTopicName(service.name());
      if (normalized.endsWith(SPAWN_SERVICE_SUFFIX)) {
        knownSpawnServices.add("/" + normalized);
      }
    }

    List<String> discovered = new ArrayList<>();
    for (var topic : snapshot.topics()) {
      Optional<String> namespace = namespaceFromTopic(topic.name(), topic.type());
      if (namespace.isEmpty() || sessions.containsKey(namespace.get())) {
        continue;
      }
      if (trackNamespace(namespace.get())) {
        discovered.add(displayNamespace(namespace.get()));
      }
    }

    if (!discovered.isEmpty()) {
      sendChat(RoscraftMod.prefix()
          .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
          .append(Text.literal("Tracking turtles: ").formatted(Formatting.GREEN))
          .append(Text.literal(String.join(", ", discovered)).formatted(Formatting.AQUA)));
    }
  }

  private void handleTopicPayload(BridgeEvent.TopicPayload payload) {
    String topicName = normalizeTopicName(payload.topicName());
    Optional<String> namespace = namespaceFromTopic(topicName, payload.messageType());
    if (namespace.isEmpty()) {
      return;
    }

    TurtleSession session = sessions.get(namespace.get());
    if (session == null) {
      return;
    }

    if (topicName.endsWith(CMD_VEL_SUFFIX)) {
      Optional<RosCdrDecoder.Twist> twist = RosCdrDecoder.decodeTwist(payload.payload());
      if (twist.isEmpty()) {
        sendChat(renderEventLine(namespace.get(), "cmd_vel", "ignored invalid payload"));
        return;
      }

      if (session.spawned) {
        worldOps.applyTwist(session, twist.get());
      }
      sendChat(renderEventLine(
          namespace.get(),
          "cmd_vel",
          formatTwist(twist.get()) + " -> " + session.describePose(worldOps.findEntity(session))));
      return;
    }

    if (topicName.endsWith(MOVEMENT_STATE_SUFFIX)) {
      RosCdrDecoder.decodeString(payload.payload())
          .ifPresent(value -> session.lastMovementState = value);
      session.lastEventSummary = "state=" + session.lastMovementState;
      sendChat(renderEventLine(
          namespace.get(),
          "movement state",
          session.lastMovementState + " -> " + session.describePose(worldOps.findEntity(session))));
      return;
    }

    if (topicName.endsWith(LIFECYCLE_STATE_SUFFIX)) {
      Optional<Boolean> lifecycle = RosCdrDecoder.decodeBool(payload.payload());
      if (lifecycle.isEmpty()) {
        sendChat(renderEventLine(namespace.get(), "lifecycle state", "ignored invalid payload"));
        return;
      }

      boolean wasSpawned = session.spawned;
      boolean isSpawned = lifecycle.get();
      session.spawned = isSpawned;
      session.lastEventSummary = "lifecycle=" + isSpawned;

      if (!wasSpawned && isSpawned) {
        worldOps.spawn(session);
      } else if (wasSpawned && !isSpawned) {
        worldOps.despawn(session);
      } else if (isSpawned && !session.hasEntity()) {
        worldOps.spawn(session);
      }

      sendChat(renderEventLine(
          namespace.get(),
          "lifecycle state",
          (isSpawned ? "spawned" : "despawned")
              + " -> "
              + session.describePose(worldOps.findEntity(session))));
    }
  }

  private void handleServiceCallResponse(BridgeEvent.ServiceCallResponse response) {
    String namespace = pendingLifecycleNamespaces.remove(response.requestId());
    if (namespace == null) {
      return;
    }

    TurtleSession session = sessions.get(namespace);
    if (session == null) {
      return;
    }

    String normalizedService = normalizeTopicName(response.serviceName());
    if (normalizedService.endsWith(SPAWN_SERVICE_SUFFIX)) {
      if (!response.success()) {
        sendChat(RoscraftMod.prefix()
            .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
            .append(Text.literal("Spawn service failed for ").formatted(Formatting.RED))
            .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
            .append(Text.literal(": " + response.resultText()).formatted(Formatting.GRAY)));
        return;
      }

      session.spawned = true;
      if (!session.hasEntity()) {
        worldOps.spawn(session);
      }
      sendChat(renderEventLine(
          namespace,
          "spawn service",
          "ok -> " + session.describePose(worldOps.findEntity(session))));
      return;
    }

    if (normalizedService.endsWith(DESPAWN_SERVICE_SUFFIX)) {
      if (!response.success()) {
        sendChat(RoscraftMod.prefix()
            .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
            .append(Text.literal("Despawn service failed for ").formatted(Formatting.RED))
            .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
            .append(Text.literal(": " + response.resultText()).formatted(Formatting.GRAY)));
        return;
      }

      session.spawned = false;
      worldOps.despawn(session);
      sendChat(renderEventLine(
          namespace,
          "despawn service",
          "ok -> " + session.describePose(worldOps.findEntity(session))));
    }
  }

  private void handleBridgeError(BridgeEvent.BridgeError error) {
    RoscraftMod.LOGGER.warn(
        "Turtlebot bridge error: requestId={} code={} message={}",
        error.requestId(),
        error.errorCode(),
        error.errorMessage());

    String namespace = pendingLifecycleNamespaces.remove(error.requestId());
    MutableText message = RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Bridge error: ").formatted(Formatting.RED))
        .append(Text.literal(error.errorCode() + " - " + error.errorMessage())
            .formatted(Formatting.GRAY));

    if (namespace != null && isServiceUnavailableError(error)) {
      message
          .append(Text.literal(" Hint: ").formatted(Formatting.YELLOW))
          .append(spawnNamespaceHint(namespace, spawnServiceTopic(namespace))
              .formatted(Formatting.GRAY));
    }

    sendChat(message);
  }

  private boolean isSpawnServiceKnown(String spawnService) {
    if (knownSpawnServices.isEmpty()) {
      return true;
    }
    String normalized = normalizeTopicName(spawnService);
    return knownSpawnServices.contains("/" + normalized);
  }

  private static boolean isServiceUnavailableError(BridgeEvent.BridgeError error) {
    String message =
        error.errorMessage() == null ? "" : error.errorMessage().toLowerCase(Locale.ROOT);
    return message.contains("service unavailable") || message.contains("not available");
  }

  private static Text spawnServiceUnavailableText(String namespace, String spawnService) {
    return RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("No spawn service at ").formatted(Formatting.RED))
        .append(Text.literal(spawnService).formatted(Formatting.GRAY))
        .append(Text.literal(". ").formatted(Formatting.GRAY))
        .append(spawnNamespaceHint(namespace, spawnService).formatted(Formatting.YELLOW));
  }

  private static String spawnNamespaceHint(String namespace, String spawnService) {
    StringBuilder hint = new StringBuilder();
    hint.append("Use /ros turtlebot spawn root for root-namespace nodes, or ");
    hint.append("ros2 launch roscraft_turtlebot turtlebot.launch.py for ");
    hint.append(displayNamespace(namespace.isBlank() ? "turtle1" : namespace));
    hint.append(". Expected service: ").append(spawnService);
    return hint.toString();
  }

  private boolean trackNamespace(String namespace) {
    if (sessions.containsKey(namespace)) {
      return true;
    }

    TurtleSession session = new TurtleSession(namespace);
    if (!subscribe(session, cmdVelTopic(namespace), TWIST_TYPE)) {
      session.close();
      return false;
    }
    if (!subscribe(session, movementStateTopic(namespace), STRING_TYPE)) {
      session.close();
      return false;
    }
    if (!subscribe(session, lifecycleStateTopic(namespace), BOOL_TYPE)) {
      session.close();
      return false;
    }

    sessions.put(namespace, session);
    return true;
  }

  private boolean subscribe(TurtleSession session, String topicName, String messageType) {
    Optional<Subscription> subscription = ctx.subscribeTopic(topicName, messageType);
    if (subscription.isEmpty()) {
      return false;
    }

    session.subscriptions.add(subscription.get());
    return true;
  }

  private static Optional<String> namespaceFromTopic(String topicName, String messageType) {
    String normalizedTopic = normalizeTopicName(topicName);
    for (TopicDescriptor descriptor : TopicDescriptor.values()) {
      if (!normalizedTopic.endsWith(descriptor.suffix)) {
        continue;
      }
      if (!descriptor.messageType.isBlank()
          && !messageType.isBlank()
          && !descriptor.messageType.equals(messageType)) {
        continue;
      }
      String namespace =
          normalizedTopic.substring(0, normalizedTopic.length() - descriptor.suffix.length());
      return Optional.of(normalizeNamespace(namespace));
    }
    return Optional.empty();
  }

  private static String cmdVelTopic(String namespace) {
    return topicName(namespace, CMD_VEL_SUFFIX);
  }

  private static String movementStateTopic(String namespace) {
    return topicName(namespace, MOVEMENT_STATE_SUFFIX);
  }

  private static String lifecycleStateTopic(String namespace) {
    return topicName(namespace, LIFECYCLE_STATE_SUFFIX);
  }

  private static String spawnServiceTopic(String namespace) {
    return topicName(namespace, SPAWN_SERVICE_SUFFIX);
  }

  private static String despawnServiceTopic(String namespace) {
    return topicName(namespace, DESPAWN_SERVICE_SUFFIX);
  }

  private static String topicName(String namespace, String suffix) {
    String normalizedNamespace = normalizeNamespace(namespace);
    if (normalizedNamespace.isEmpty()) {
      return "/" + suffix;
    }
    return "/" + normalizedNamespace + "/" + suffix;
  }

  private static String normalizeTopicName(String topicName) {
    String normalized = topicName == null ? "" : topicName.trim();
    while (normalized.startsWith("/")) {
      normalized = normalized.substring(1);
    }
    while (normalized.endsWith("/")) {
      normalized = normalized.substring(0, normalized.length() - 1);
    }
    return normalized;
  }

  private static String normalizeNamespace(String namespace) {
    String normalized = namespace == null ? "" : namespace.trim();
    while (normalized.startsWith("/")) {
      normalized = normalized.substring(1);
    }
    while (normalized.endsWith("/")) {
      normalized = normalized.substring(0, normalized.length() - 1);
    }
    if (normalized.equalsIgnoreCase("root") || normalized.equals("_")) {
      return "";
    }
    return normalized;
  }

  private static String formatTwist(RosCdrDecoder.Twist twist) {
    return String.format(
        "linear_x=%.3f linear_z=%.3f angular_z=%.3f",
        twist.linearX(), twist.linearZ(), twist.angularZ());
  }

  private static void sendChat(Text message) {
    RoscraftMod mod = RoscraftMod.getInstance();
    if (mod != null) {
      mod.sendToRequesterOrOperators(null, message);
      return;
    }
    RoscraftMod.LOGGER.info(message.getString());
  }

  private static int bridgeNotConnected(ServerCommandSource source) {
    source.sendMessage(errorText("Bridge not connected. Run /ros connection connect first."));
    return 0;
  }

  private static Text errorText(String message) {
    return RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal(message).formatted(Formatting.RED));
  }

  private MutableText renderStatusLine(String namespace, TurtleSession session) {
    return RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
        .append(Text.literal(" ").formatted(Formatting.GRAY))
        .append(Text.literal(session.describePose(worldOps.findEntity(session)))
            .formatted(Formatting.GREEN))
        .append(Text.literal(" | last=").formatted(Formatting.DARK_GRAY))
        .append(Text.literal(session.lastEventSummary()).formatted(Formatting.GRAY));
  }

  private static MutableText renderEventLine(String namespace, String label, String detail) {
    return RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
        .append(Text.literal(" ").formatted(Formatting.GRAY))
        .append(Text.literal(label + ": ").formatted(Formatting.YELLOW))
        .append(Text.literal(detail).formatted(Formatting.GREEN));
  }

  private static String displayNamespace(String namespace) {
    return namespace == null || namespace.isBlank() ? "<root>" : namespace;
  }

  private enum TopicDescriptor {
    CMD_VEL(CMD_VEL_SUFFIX, TWIST_TYPE),
    MOVEMENT_STATE(MOVEMENT_STATE_SUFFIX, STRING_TYPE),
    LIFECYCLE_STATE(LIFECYCLE_STATE_SUFFIX, BOOL_TYPE);

    private final String suffix;
    private final String messageType;

    TopicDescriptor(String suffix, String messageType) {
      this.suffix = suffix;
      this.messageType = messageType;
    }
  }
}
