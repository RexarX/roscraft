package net.roscraft.mod.command.request;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.RoscraftMod;

/**
 * Tracks {@code /ros} command request IDs and handles timeouts and streaming-session cleanup.
 */
public final class CommandRequestTracker {

  private static final long REQUEST_TIMEOUT_MILLIS = 5_000L;

  private final RoscraftMod mod;
  private final Map<Long, CommandPendingRequest> pending = new HashMap<>();

  public CommandRequestTracker(RoscraftMod mod) {
    this.mod = mod;
  }

  public synchronized void track(long requestId, CommandRequestKind kind, UUID requesterUuid) {
    track(requestId, kind, requesterUuid, null);
  }

  public synchronized void track(
      long requestId, CommandRequestKind kind, UUID requesterUuid, String metadata) {
    if (mod.requestHub().addons().isTracked(requestId)) {
      RoscraftMod.LOGGER.warn(
          "Request #{} is already tracked by an addon; refusing command track for {}",
          requestId,
          kind);
      return;
    }
    pending.put(
        requestId,
        new CommandPendingRequest(kind, requesterUuid, System.currentTimeMillis(), metadata));
  }

  public synchronized CommandPendingRequest complete(long requestId) {
    return pending.remove(requestId);
  }

  public synchronized CommandPendingRequest peek(long requestId) {
    return pending.get(requestId);
  }

  public synchronized boolean has(long requestId) {
    return pending.containsKey(requestId);
  }

  public void processTimeouts() {
    long now = System.currentTimeMillis();
    List<Map.Entry<Long, CommandPendingRequest>> timedOut = new ArrayList<>();

    synchronized (this) {
      var iterator = pending.entrySet().iterator();
      while (iterator.hasNext()) {
        var entry = iterator.next();
        CommandPendingRequest request = entry.getValue();
        if (isPersistentStreamingKind(request.kind())) {
          continue;
        }
        if (now - request.createdAtMillis() >= REQUEST_TIMEOUT_MILLIS) {
          timedOut.add(Map.entry(entry.getKey(), request));
          iterator.remove();
        }
      }
    }

    for (var entry : timedOut) {
      long requestId = entry.getKey();
      CommandPendingRequest request = entry.getValue();
      RoscraftMod.LOGGER.warn(
          "{} request #{} timed out after {} ms",
          displayName(request.kind()),
          requestId,
          REQUEST_TIMEOUT_MILLIS);
      mod.sendToRequesterOrOperators(
          request.requesterUuid(),
          RoscraftMod.prefix()
              .append(Text.literal(displayName(request.kind())
                      + " request #"
                      + requestId
                      + " timed out. Verify bridge host/port"
                      + " and network routing/firewall.")
                  .formatted(Formatting.RED)));
    }
  }

  public void stopRunningSessions(RoscraftBridge activeBridge) {
    if (activeBridge == null) {
      return;
    }

    List<Map.Entry<Long, CommandPendingRequest>> toStop = new ArrayList<>();
    synchronized (this) {
      for (var entry : pending.entrySet()) {
        CommandPendingRequest request = entry.getValue();
        switch (request.kind()) {
          case TOPIC_HZ, TOPIC_BW, TOPIC_DELAY, TOPIC_ECHO -> {
            if (request.metadata() != null) {
              toStop.add(entry);
            }
          }
          default -> {}
        }
      }
    }

    for (var entry : toStop) {
      long requestId = entry.getKey();
      CommandPendingRequest request = entry.getValue();
      String topicName = extractTopicName(request.metadata());
      synchronized (this) {
        if (topicName == null || topicName.isBlank()) {
          pending.remove(requestId);
          continue;
        }
      }

      try {
        switch (request.kind()) {
          case TOPIC_HZ -> activeBridge.topics().hz(topicName, "", 0);
          case TOPIC_BW -> activeBridge.topics().bw(topicName, "", 0);
          case TOPIC_DELAY -> activeBridge.topics().delay(topicName, "", 0);
          case TOPIC_ECHO -> activeBridge.topics().unsubscribe(topicName);
          default -> {}
        }
      } catch (RuntimeException e) {
        RoscraftMod.LOGGER.warn(
            "Failed to stop running session #{} for {}: {}", requestId, topicName, e.getMessage());
      }

      synchronized (this) {
        pending.remove(requestId);
      }
    }
  }

  private static boolean isPersistentStreamingKind(CommandRequestKind kind) {
    return switch (kind) {
      case TOPIC_ECHO,
          TOPIC_PUB,
          TOPIC_ECHO_STOP,
          TOPIC_HZ,
          TOPIC_HZ_STOP,
          TOPIC_BW,
          TOPIC_BW_STOP,
          TOPIC_DELAY,
          TOPIC_DELAY_STOP -> true;
      default -> false;
    };
  }

  private static String extractTopicName(String metadata) {
    if (metadata == null || metadata.isBlank()) {
      return null;
    }
    if (metadata.startsWith("topic_name=")) {
      int semicolonIdx = metadata.indexOf(';', "topic_name=".length());
      if (semicolonIdx < 0) {
        return metadata.substring("topic_name=".length());
      }
      return metadata.substring("topic_name=".length(), semicolonIdx);
    }
    return metadata;
  }

  private static String displayName(CommandRequestKind kind) {
    return switch (kind) {
      case PLAYERS -> "Player list";
      case CONNECTION_CHECK -> "Connection";
      case NODE_LIST -> "Node list";
      case NODE_INFO -> "Node info";
      case TOPIC_LIST -> "Topic list";
      case TOPIC_TYPE -> "Topic type";
      case TOPIC_FIND -> "Topic find";
      case TOPIC_ECHO -> "Topic echo";
      case TOPIC_ECHO_STOP -> "Topic echo stop";
      case TOPIC_PUB -> "Topic pub";
      case TOPIC_HZ -> "Topic hz";
      case TOPIC_HZ_STOP -> "Topic hz stop";
      case TOPIC_BW -> "Topic bw";
      case TOPIC_BW_STOP -> "Topic bw stop";
      case TOPIC_DELAY -> "Topic delay";
      case TOPIC_DELAY_STOP -> "Topic delay stop";
      case TOPIC_INFO -> "Topic info";
      case SERVICE_LIST -> "Service list";
      case SERVICE_TYPE -> "Service type";
      case SERVICE_FIND -> "Service find";
      case SERVICE_INFO -> "Service info";
      case SERVICE_CALL -> "Service call";
      case ACTION_LIST -> "Action list";
      case ACTION_TYPE -> "Action type";
      case ACTION_INFO -> "Action info";
      case ACTION_SEND_GOAL -> "Action send_goal";
      case PARAM_LIST -> "Param list";
      case PARAM_GET -> "Param get";
      case PARAM_SET -> "Param set";
      case PARAM_DESCRIBE -> "Param describe";
      case PARAM_DUMP -> "Param dump";
      case PARAM_LOAD -> "Param load";
      case INTERFACE_LIST -> "Interface list";
      case INTERFACE_SHOW -> "Interface show";
    };
  }
}
