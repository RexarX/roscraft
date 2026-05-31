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
import net.minecraft.server.command.CommandManager;
import net.minecraft.server.command.ServerCommandSource;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.Subscription;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.addon.AbstractRoscraftAddon;
import net.roscraft.mod.addon.minecraft.RoscraftAddonCommands;

public final class TurtlebotAddon extends AbstractRoscraftAddon implements RoscraftAddonCommands {

  private static final String MOVEMENT_COMMAND_SUFFIX = "roscraft/turtlebot/movement/cmd";
  private static final String MOVEMENT_STATE_SUFFIX = "roscraft/turtlebot/movement/state";
  private static final String LIFECYCLE_STATE_SUFFIX = "roscraft/turtlebot/lifecycle/state";
  private static final String STRING_TYPE = "std_msgs/msg/String";
  private static final String BOOL_TYPE = "std_msgs/msg/Bool";

  private final Map<String, TurtleSession> sessions = new LinkedHashMap<>();

  @Override
  public String addonId() {
    return "turtlebot";
  }

  @Override
  protected void configure() {
    RoscraftMod.LOGGER.info("Turtlebot addon initialised.");

    on(BridgeEvent.GraphSnapshot.class, this::handleGraphSnapshot);
    on(BridgeEvent.TopicPayload.class, this::handleTopicPayload);
    on(BridgeEvent.BridgeError.class, this::handleBridgeError);

    ctx.bridgeIfConnected().ifPresent(bridge -> bridge.graph().snapshot());
  }

  @Override
  public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return List.of(
        CommandManager.literal("turtlebot")
            .executes(c -> executeStatus(c.getSource()))
            .then(CommandManager.literal("status").executes(c -> executeStatus(c.getSource())))
            .then(CommandManager.literal("refresh").executes(c -> executeRefresh(c.getSource())))
            .then(CommandManager.literal("watch")
                .then(CommandManager.argument("namespace", StringArgumentType.string())
                    .executes(c -> executeWatch(
                        c.getSource(), StringArgumentType.getString(c, "namespace")))))
            .then(CommandManager.literal("forget")
                .then(CommandManager.argument("namespace", StringArgumentType.string())
                    .executes(c -> executeForget(
                        c.getSource(), StringArgumentType.getString(c, "namespace"))))));
  }

  @Override
  protected void onShutdown() {
    for (TurtleSession session : sessions.values()) {
      session.close();
    }
    sessions.clear();
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

    session.close();
    source.sendMessage(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Stopped tracking ").formatted(Formatting.YELLOW))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA)));
    return 1;
  }

  private void handleGraphSnapshot(BridgeEvent.GraphSnapshot snapshot) {
    List<String> discovered = new ArrayList<>();
    for (var topic : snapshot.topics()) {
      Optional<String> namespace = namespaceFromTopic(topic.name(), topic.type());
      if (namespace.isEmpty()) {
        continue;
      }
      if (sessions.containsKey(namespace.get())) {
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

    String payloadText = decodePayloadText(payload.payload());
    if (topicName.endsWith(MOVEMENT_COMMAND_SUFFIX)) {
      MovementCommand command = parseMovementCommand(payloadText);
      if (command == null) {
        session.lastMovementCommand = payloadText;
        sendChat(renderEventLine(
            namespace.get(), "movement command", "ignored invalid payload: " + payloadText));
        return;
      }

      session.lastMovementCommand = command.rawText();
      if (session.spawned) {
        session.apply(command);
      }
      sendChat(renderEventLine(
          namespace.get(), "movement command", command.rawText() + " -> " + session.describePose()));
      return;
    }

    if (topicName.endsWith(MOVEMENT_STATE_SUFFIX)) {
      session.lastMovementState = payloadText;
      session.lastEventSummary = "state=" + payloadText;
      sendChat(renderEventLine(
          namespace.get(), "movement state", payloadText + " -> " + session.describePose()));
      return;
    }

    if (topicName.endsWith(LIFECYCLE_STATE_SUFFIX)) {
      session.spawned = parseBooleanPayload(payloadText);
      session.lastLifecycleState = payloadText;
      session.lastEventSummary = "lifecycle=" + payloadText;
      sendChat(renderEventLine(
          namespace.get(),
          "lifecycle state",
          (session.spawned ? "spawned" : "despawned") + " -> " + session.describePose()));
    }
  }

  private void handleBridgeError(BridgeEvent.BridgeError error) {
    RoscraftMod.LOGGER.warn(
        "Turtlebot bridge error: requestId={} code={} message={}",
        error.requestId(),
        error.errorCode(),
        error.errorMessage());

    sendChat(RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal("Bridge error: ").formatted(Formatting.RED))
        .append(Text.literal(error.errorCode() + " - " + error.errorMessage())
            .formatted(Formatting.GRAY)));
  }

  private boolean trackNamespace(String namespace) {
    TurtleSession session = new TurtleSession(namespace);
    if (!subscribe(session, movementCommandTopic(namespace), STRING_TYPE)) {
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
      String namespace = normalizedTopic.substring(
          0, normalizedTopic.length() - descriptor.suffix.length());
      return Optional.of(normalizeNamespace(namespace));
    }
    return Optional.empty();
  }

  private static String movementCommandTopic(String namespace) {
    return topicName(namespace, MOVEMENT_COMMAND_SUFFIX);
  }

  private static String movementStateTopic(String namespace) {
    return topicName(namespace, MOVEMENT_STATE_SUFFIX);
  }

  private static String lifecycleStateTopic(String namespace) {
    return topicName(namespace, LIFECYCLE_STATE_SUFFIX);
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
    return normalized;
  }

  private static String decodePayloadText(byte[] payload) {
    String text = new String(payload, StandardCharsets.UTF_8).trim();
    int dataIndex = text.indexOf("data:");
    if (dataIndex >= 0) {
      text = text.substring(dataIndex + 5).trim();
    }
    if ((text.startsWith("'") && text.endsWith("'"))
        || (text.startsWith("\"") && text.endsWith("\""))) {
      text = text.substring(1, text.length() - 1);
    }
    return text.trim();
  }

  private static boolean parseBooleanPayload(String payloadText) {
    String normalized = decodePayloadText(payloadText.getBytes(StandardCharsets.UTF_8))
        .toLowerCase(Locale.ROOT);
    return normalized.equals("true")
        || normalized.equals("1")
        || normalized.equals("yes")
        || normalized.equals("on")
        || normalized.equals("spawned");
  }

  private static MovementCommand parseMovementCommand(String payloadText) {
    String normalized = decodePayloadText(payloadText.getBytes(StandardCharsets.UTF_8))
        .toLowerCase(Locale.ROOT);
    if (normalized.isBlank()) {
      return null;
    }

    String[] parts = normalized.split("\\s+", 2);
    String action = parts[0];
    if (!action.equals("backward")
        && !action.equals("down")
        && !action.equals("forward")
        && !action.equals("left")
        && !action.equals("right")
        && !action.equals("stop")
        && !action.equals("up")) {
      return null;
    }

    double scale = 1.0;
    if (parts.length == 2) {
      try {
        scale = Double.parseDouble(parts[1]);
      } catch (NumberFormatException ex) {
        return null;
      }
    }

    return new MovementCommand(action, scale, normalized);
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

  private static MutableText renderStatusLine(String namespace, TurtleSession session) {
    return RoscraftMod.prefix()
        .append(Text.literal("turtlebot ").formatted(Formatting.GOLD))
        .append(Text.literal(displayNamespace(namespace)).formatted(Formatting.AQUA))
        .append(Text.literal(" ").formatted(Formatting.GRAY))
        .append(Text.literal(session.describePose()).formatted(Formatting.GREEN))
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
    MOVEMENT_COMMAND(MOVEMENT_COMMAND_SUFFIX, STRING_TYPE),
    MOVEMENT_STATE(MOVEMENT_STATE_SUFFIX, STRING_TYPE),
    LIFECYCLE_STATE(LIFECYCLE_STATE_SUFFIX, BOOL_TYPE);

    private final String suffix;
    private final String messageType;

    TopicDescriptor(String suffix, String messageType) {
      this.suffix = suffix;
      this.messageType = messageType;
    }
  }

  private record MovementCommand(String action, double scale, String rawText) {}

  private static final class TurtleSession {
    private final String namespace;
    private final List<Subscription> subscriptions = new ArrayList<>();
    private double x;
    private double y;
    private double z;
    private double yaw;
    private double pitch;
    private double roll;
    private boolean spawned;
    private String lastMovementCommand = "";
    private String lastMovementState = "";
    private String lastLifecycleState = "";
    private String lastEventSummary = "";

    private TurtleSession(String namespace) {
      this.namespace = namespace;
    }

    private void apply(MovementCommand command) {
      double scale = command.scale();
      switch (command.action()) {
        case "forward" -> moveForward(scale);
        case "backward" -> moveForward(-scale);
        case "left" -> yaw = normalizeAngle(yaw + 90.0 * scale);
        case "right" -> yaw = normalizeAngle(yaw - 90.0 * scale);
        case "up" -> y += scale;
        case "down" -> y -= scale;
        case "stop" -> {
          return;
        }
        default -> {
          return;
        }
      }

      lastMovementCommand = command.rawText();
      lastEventSummary = "command=" + command.rawText();
    }

    private void moveForward(double distance) {
      double radians = Math.toRadians(yaw);
      x += -Math.sin(radians) * distance;
      z += Math.cos(radians) * distance;
    }

    private String describePose() {
      return String.format(
          Locale.ROOT,
          "pos=(%.2f, %.2f, %.2f) rot=(yaw=%.1f pitch=%.1f roll=%.1f) spawned=%s",
          x,
          y,
          z,
          yaw,
          pitch,
          roll,
          spawned);
    }

    private String lastEventSummary() {
      return lastEventSummary.isBlank() ? "idle" : lastEventSummary;
    }

    private void close() {
      for (int index = subscriptions.size() - 1; index >= 0; index--) {
        try {
          subscriptions.get(index).close();
        } catch (Exception ignored) {
          // Best-effort cleanup.
        }
      }
      subscriptions.clear();
    }

    private static double normalizeAngle(double angle) {
      double normalized = angle % 360.0;
      return normalized < 0.0 ? normalized + 360.0 : normalized;
    }
  }
}