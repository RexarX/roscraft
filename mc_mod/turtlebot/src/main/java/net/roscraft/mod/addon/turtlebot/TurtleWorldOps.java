package net.roscraft.mod.addon.turtlebot;

import java.util.Locale;
import java.util.Optional;
import java.util.function.Supplier;
import net.minecraft.entity.Entity;
import net.minecraft.entity.EntityType;
import net.minecraft.entity.SpawnReason;
import net.minecraft.entity.passive.TurtleEntity;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.server.world.ServerWorld;
import net.minecraft.text.Text;
import net.minecraft.util.math.BlockPos;
import net.minecraft.world.Heightmap;
import net.roscraft.mod.RoscraftMod;

final class TurtleWorldOps {

  private final Supplier<MinecraftServer> serverSupplier;

  TurtleWorldOps(Supplier<MinecraftServer> serverSupplier) {
    this.serverSupplier = serverSupplier;
  }

  void spawn(TurtleSession session) {
    runOnServer(() -> spawnOnServerThread(session));
  }

  void despawn(TurtleSession session) {
    runOnServer(() -> despawnOnServerThread(session));
  }

  void applyTwist(TurtleSession session, RosCdrDecoder.Twist twist) {
    if (!session.spawned) {
      return;
    }
    if (RosCdrDecoder.isStop(twist)) {
      session.lastEventSummary = "cmd_vel stop";
      return;
    }
    runOnServer(() -> applyTwistOnServerThread(session, twist));
  }

  Optional<TurtleEntity> findEntity(TurtleSession session) {
    MinecraftServer server = serverSupplier.get();
    if (server == null || session.entityId == null) {
      return Optional.empty();
    }

    for (ServerWorld world : server.getWorlds()) {
      Entity entity = world.getEntity(session.entityId);
      if (entity instanceof TurtleEntity turtle) {
        return Optional.of(turtle);
      }
    }
    return Optional.empty();
  }

  private void runOnServer(Runnable action) {
    MinecraftServer server = serverSupplier.get();
    if (server == null) {
      RoscraftMod.LOGGER.warn("Turtlebot world action skipped: server not running.");
      return;
    }
    server.execute(action);
  }

  private void spawnOnServerThread(TurtleSession session) {
    MinecraftServer server = serverSupplier.get();
    if (server == null) {
      return;
    }

    despawnOnServerThread(session);

    ServerWorld world = server.getOverworld();
    TurtleSession.PendingSpawn pendingSpawn = session.consumePendingSpawn();
    BlockPos spawnPos = resolveSpawnPos(server, world, session.namespace, pendingSpawn);
    TurtleEntity turtle =
        EntityType.TURTLE.create(world, entity -> {}, spawnPos, SpawnReason.COMMAND, false, false);
    if (turtle == null) {
      RoscraftMod.LOGGER.warn("Failed to create turtle entity for namespace {}", session.namespace);
      return;
    }

    turtle.refreshPositionAndAngles(
        spawnPos.getX() + 0.5, spawnPos.getY(), spawnPos.getZ() + 0.5, 0.0F, 0.0F);
    turtle.setCustomName(Text.literal(session.namespace));
    turtle.setCustomNameVisible(true);

    if (!world.spawnEntity(turtle)) {
      RoscraftMod.LOGGER.warn("Failed to spawn turtle entity for namespace {}", session.namespace);
      return;
    }

    session.entityId = turtle.getUuid();
    session.lastEventSummary = "spawned at " + formatBlockPos(spawnPos);
  }

  private void despawnOnServerThread(TurtleSession session) {
    if (session.entityId == null) {
      return;
    }

    MinecraftServer server = serverSupplier.get();
    if (server == null) {
      session.clearEntity();
      return;
    }

    for (ServerWorld world : server.getWorlds()) {
      Entity entity = world.getEntity(session.entityId);
      if (entity != null) {
        entity.discard();
        break;
      }
    }

    session.clearEntity();
    session.lastEventSummary = "despawned";
  }

  private void applyTwistOnServerThread(TurtleSession session, RosCdrDecoder.Twist twist) {
    Optional<TurtleEntity> entityOptional = findEntity(session);
    if (entityOptional.isEmpty()) {
      return;
    }

    TurtleEntity turtle = entityOptional.get();
    double yawRadians = Math.toRadians(turtle.getYaw());
    double deltaX = -Math.sin(yawRadians) * twist.linearX();
    double deltaZ = Math.cos(yawRadians) * twist.linearX();
    double deltaY = twist.linearZ();
    float deltaYaw = (float) Math.toDegrees(twist.angularZ());

    turtle.refreshPositionAndAngles(
        turtle.getX() + deltaX,
        turtle.getY() + deltaY,
        turtle.getZ() + deltaZ,
        turtle.getYaw() + deltaYaw,
        turtle.getPitch());
    turtle.setVelocity(0.0, 0.0, 0.0);

    session.lastEventSummary = String.format(
        Locale.ROOT,
        "cmd_vel linear_x=%.3f linear_z=%.3f angular_z=%.3f",
        twist.linearX(),
        twist.linearZ(),
        twist.angularZ());
  }

  private static BlockPos resolveSpawnPos(
      MinecraftServer server,
      ServerWorld world,
      String namespace,
      TurtleSession.PendingSpawn pendingSpawn) {
    if (pendingSpawn.target() == TurtleSession.SpawnTarget.PLAYER
        && !pendingSpawn.playerName().isBlank()) {
      ServerPlayerEntity player = server.getPlayerManager().getPlayer(pendingSpawn.playerName());
      if (player != null) {
        return player.getBlockPos();
      }
    }

    BlockPos worldSpawn = world.getSpawnPos();
    int index = namespaceIndex(namespace);
    int offsetX = index * 2;
    int x = worldSpawn.getX() + offsetX;
    int z = worldSpawn.getZ();
    int y = world.getTopY(Heightmap.Type.MOTION_BLOCKING_NO_LEAVES, x, z);
    return new BlockPos(x, y, z);
  }

  private static int namespaceIndex(String namespace) {
    int digitsStart = -1;
    for (int index = namespace.length() - 1; index >= 0; index--) {
      if (!Character.isDigit(namespace.charAt(index))) {
        digitsStart = index;
        break;
      }
    }
    if (digitsStart == namespace.length() - 1) {
      return 0;
    }

    try {
      return Integer.parseInt(namespace.substring(digitsStart + 1)) - 1;
    } catch (NumberFormatException ex) {
      return Math.floorMod(namespace.hashCode(), 8);
    }
  }

  private static String formatBlockPos(BlockPos pos) {
    return pos.getX() + ", " + pos.getY() + ", " + pos.getZ();
  }
}
