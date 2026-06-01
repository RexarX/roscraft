package net.roscraft.mod.addon.turtlebot;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.entity.passive.TurtleEntity;
import net.roscraft.bridge.event.Subscription;

final class TurtleSession {

  enum SpawnTarget {
    WORLD_SPAWN,
    PLAYER
  }

  final String namespace;
  final List<Subscription> subscriptions = new ArrayList<>();

  boolean spawned;
  UUID entityId;
  SpawnTarget pendingSpawnTarget = SpawnTarget.WORLD_SPAWN;
  String pendingSpawnPlayerName = "";

  String lastMovementState = "";
  String lastEventSummary = "";

  TurtleSession(String namespace) {
    this.namespace = namespace;
  }

  void setPendingSpawnAtPlayer(String playerName) {
    pendingSpawnTarget = SpawnTarget.PLAYER;
    pendingSpawnPlayerName = playerName == null ? "" : playerName.trim();
  }

  void setPendingSpawnAtWorld() {
    pendingSpawnTarget = SpawnTarget.WORLD_SPAWN;
    pendingSpawnPlayerName = "";
  }

  PendingSpawn consumePendingSpawn() {
    PendingSpawn pending = new PendingSpawn(pendingSpawnTarget, pendingSpawnPlayerName);
    setPendingSpawnAtWorld();
    return pending;
  }

  record PendingSpawn(SpawnTarget target, String playerName) {}

  boolean hasEntity() {
    return entityId != null;
  }

  void clearEntity() {
    entityId = null;
  }

  String describePose(Optional<TurtleEntity> entity) {
    if (entity.isPresent()) {
      TurtleEntity turtle = entity.get();
      return String.format(
          Locale.ROOT,
          "pos=(%.2f, %.2f, %.2f) rot=(yaw=%.1f pitch=%.1f) spawned=%s entity=%s",
          turtle.getX(),
          turtle.getY(),
          turtle.getZ(),
          turtle.getYaw(),
          turtle.getPitch(),
          spawned,
          entityId);
    }

    return String.format(Locale.ROOT, "spawned=%s (no entity)", spawned);
  }

  String lastEventSummary() {
    return lastEventSummary.isBlank() ? "idle" : lastEventSummary;
  }

  void close() {
    for (int index = subscriptions.size() - 1; index >= 0; index--) {
      try {
        subscriptions.get(index).close();
      } catch (Exception ignored) {
        // Best-effort cleanup.
      }
    }
    subscriptions.clear();
  }
}
