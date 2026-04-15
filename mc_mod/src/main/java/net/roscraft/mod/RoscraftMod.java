package net.roscraft.mod;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerLifecycleEvents;
import net.fabricmc.fabric.api.event.lifecycle.v1.ServerTickEvents;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.network.ServerPlayerEntity;
import net.minecraft.text.MutableText;
import net.minecraft.text.Text;
import net.minecraft.util.Formatting;
import net.roscraft.bridge.BridgeCallback;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.bridge.data.BridgeError;
import net.roscraft.bridge.data.GraphSnapshot;
import net.roscraft.bridge.data.InterfaceListResponse;
import net.roscraft.bridge.data.InterfaceShowResponse;
import net.roscraft.bridge.data.NodeEntry;
import net.roscraft.bridge.data.NodeInfoResponse;
import net.roscraft.bridge.data.Player;
import net.roscraft.bridge.data.PlayerList;
import net.roscraft.bridge.data.ServiceInfoResponse;
import net.roscraft.bridge.data.TopicBwResponse;
import net.roscraft.bridge.data.TopicHzResponse;
import net.roscraft.bridge.data.TopicInfoResponse;
import net.roscraft.bridge.data.TopicPayload;
import net.roscraft.mod.bridge.BridgeManager;
import net.roscraft.mod.command.RoscraftCommands;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Fabric mod initialiser for Roscraft.
 *
 * <p>
 * Responsible for:
 * <ul>
 * <li>Creating and configuring the active bridge via
 * {@link BridgeManager}.</li>
 * <li>Registering Fabric lifecycle and tick event listeners.</li>
 * <li>Registering mod commands.</li>
 * </ul>
 */
public final class RoscraftMod implements ModInitializer {

  public static final String MOD_ID = "roscraft";
  public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

  private static final int MAX_GRAPH_ITEMS_TO_SHOW = 8;
  private static final int MAX_COMMAND_ITEMS_TO_SHOW = 64;
  private static final int MAX_PLAYERS_TO_SHOW = 10;
  private static final long REQUEST_TIMEOUT_MILLIS = 5_000L;

  /**
   * Runtime bridge lifecycle/controller, initialised in {@link #onInitialize}.
   */
  private BridgeManager bridgeManager;

  private MinecraftServer server;
  private final Map<Long, PendingRequest> pendingRequests = new HashMap<>();

  public enum PendingRequestKind {
    PLAYERS,
    CONNECTION_CHECK,
    NODE_LIST,
    NODE_INFO,
    TOPIC_LIST,
    TOPIC_TYPE,
    TOPIC_FIND,
    TOPIC_ECHO,
    TOPIC_PUB,
    TOPIC_HZ,
    TOPIC_BW,
    TOPIC_INFO,
    SERVICE_LIST,
    SERVICE_TYPE,
    SERVICE_FIND,
    SERVICE_INFO,
    SERVICE_CALL,
    ACTION_LIST,
    ACTION_TYPE,
    ACTION_INFO,
    ACTION_SEND_GOAL,
    PARAM_LIST,
    PARAM_GET,
    PARAM_SET,
    PARAM_DESCRIBE,
    PARAM_DUMP,
    INTERFACE_LIST,
    INTERFACE_SHOW,
  }

  private record PendingRequest(
      PendingRequestKind kind, UUID requesterUuid, long createdAtMillis, String metadata) {}

  @Override
  public void onInitialize() {
    LOGGER.info("Roscraft initialising...");

    var config = RoscraftConfig.load();
    bridgeManager = new BridgeManager(config, new ModBridgeCallback());

    var connectResult = bridgeManager.connect();
    if (connectResult.success()) {
      LOGGER.info(connectResult.message());
    } else {
      LOGGER.error(connectResult.message());
    }

    RoscraftCommands.register();

    ServerTickEvents.END_SERVER_TICK.register(server -> onServerTick());

    ServerLifecycleEvents.SERVER_STARTED.register(server -> this.server = server);

    ServerLifecycleEvents.SERVER_STOPPING.register(server -> {
      LOGGER.info("Roscraft shutting down...");
      this.server = null;
      bridgeManager.close();
    });

    LOGGER.info(
        "Roscraft initialised (selected mode: {}, JNI available: {})",
        bridgeManager.selectedBridgeType(),
        bridgeManager.isJniAvailable());
  }

  /**
   * Returns the bridge manager. May be {@code null} before initialisation
   * completes.
   */
  public BridgeManager bridgeManager() {
    return bridgeManager;
  }

  public synchronized void trackRequest(
      long requestId, PendingRequestKind kind, UUID requesterUuid) {
    trackRequest(requestId, kind, requesterUuid, null);
  }

  public synchronized void trackRequest(
      long requestId, PendingRequestKind kind, UUID requesterUuid, String metadata) {
    pendingRequests.put(
        requestId, new PendingRequest(kind, requesterUuid, System.currentTimeMillis(), metadata));
  }

  private synchronized PendingRequest completeRequest(long requestId) {
    return pendingRequests.remove(requestId);
  }

  private synchronized PendingRequest pendingRequest(long requestId) {
    return pendingRequests.get(requestId);
  }

  private void onServerTick() {
    bridgeManager.tick();
    processTimedOutRequests();
  }

  private void processTimedOutRequests() {
    long now = System.currentTimeMillis();
    List<Map.Entry<Long, PendingRequest>> timedOut = new ArrayList<>();

    synchronized (this) {
      var iterator = pendingRequests.entrySet().iterator();
      while (iterator.hasNext()) {
        var entry = iterator.next();
        PendingRequest pending = entry.getValue();
        if (pending.kind() == PendingRequestKind.TOPIC_ECHO
            || pending.kind() == PendingRequestKind.TOPIC_HZ
            || pending.kind() == PendingRequestKind.TOPIC_BW) {
          continue;
        }
        if (now - pending.createdAtMillis() >= REQUEST_TIMEOUT_MILLIS) {
          timedOut.add(Map.entry(entry.getKey(), pending));
          iterator.remove();
        }
      }
    }

    for (var entry : timedOut) {
      long requestId = entry.getKey();
      PendingRequest pending = entry.getValue();

      String kindName =
          switch (pending.kind()) {
            case PLAYERS -> "Player list";
            case CONNECTION_CHECK -> "Connection";
            case NODE_LIST -> "Node list";
            case NODE_INFO -> "Node info";
            case TOPIC_LIST -> "Topic list";
            case TOPIC_TYPE -> "Topic type";
            case TOPIC_FIND -> "Topic find";
            case TOPIC_ECHO -> "Topic echo";
            case TOPIC_PUB -> "Topic pub";
            case TOPIC_HZ -> "Topic hz";
            case TOPIC_BW -> "Topic bw";
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
            case INTERFACE_LIST -> "Interface list";
            case INTERFACE_SHOW -> "Interface show";
          };

      LOGGER.warn(
          "{} request #{} timed out after {} ms", kindName, requestId, REQUEST_TIMEOUT_MILLIS);

      sendToRequesterOrOperators(
          pending.requesterUuid(),
          prefix()
              .append(Text.literal(kindName + " request #"
                      + requestId
                      + " timed out. Verify bridge host/port"
                      + " and network routing/firewall.")
                  .formatted(Formatting.RED)));
    }
  }

  // -------------------------------------------------------------------------
  // Inner callback — forwards ROS2 events to Minecraft
  // -------------------------------------------------------------------------

  /**
   * Default {@link BridgeCallback} that handles ROS2 events during normal mod
   * operation. Commands and subsystem handlers can register their own callbacks
   * via {@link RoscraftBridge#registerCallback}.
   */
  private final class ModBridgeCallback implements BridgeCallback {

    @Override
    public void onGraphSnapshot(GraphSnapshot snapshot) {
      LOGGER.debug(
          "Graph snapshot received: {} nodes, {} topics, {} services, {} actions",
          snapshot.nodes().size(),
          snapshot.topics().size(),
          snapshot.services().size(),
          snapshot.actions().size());

      PendingRequest pending = completeRequest(snapshot.requestId());
      if (pending == null) {
        sendGraphSnapshotPreview(snapshot, null);
        return;
      }

      UUID requesterUuid = pending.requesterUuid();
      switch (pending.kind()) {
        case CONNECTION_CHECK -> {
          sendToRequesterOrOperators(
              requesterUuid,
              prefix()
                  .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
                  .append(Text.literal(String.valueOf(snapshot.requestId()))
                      .formatted(Formatting.YELLOW))
                  .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                  .append(Text.literal(
                          "Bridge replied with nodes=" + snapshot.nodes().size()
                              + ", topics="
                              + snapshot.topics().size()
                              + ", services="
                              + snapshot.services().size()
                              + ", actions="
                              + snapshot.actions().size())
                      .formatted(Formatting.GRAY)));
        }
        case NODE_LIST -> sendNodeList(snapshot, requesterUuid, pending.metadata());
        case NODE_INFO, INTERFACE_LIST -> {
          return;
        }
        case TOPIC_LIST -> sendTopicList(snapshot, requesterUuid, pending.metadata());
        case TOPIC_TYPE -> sendTopicType(snapshot, requesterUuid, pending.metadata());
        case TOPIC_FIND -> sendTopicFind(snapshot, requesterUuid, pending.metadata());
        case TOPIC_PUB -> {
          return;
        }
        case SERVICE_LIST -> sendServiceList(snapshot, requesterUuid, pending.metadata());
        case SERVICE_TYPE -> sendServiceType(snapshot, requesterUuid, pending.metadata());
        case SERVICE_FIND -> sendServiceFind(snapshot, requesterUuid, pending.metadata());
        case ACTION_LIST -> sendActionList(snapshot, requesterUuid, pending.metadata());
        case ACTION_TYPE -> sendActionType(snapshot, requesterUuid, pending.metadata());
        case ACTION_INFO -> sendActionInfo(snapshot, requesterUuid, pending.metadata());
        case ACTION_SEND_GOAL -> sendActionSendGoal(snapshot, requesterUuid, pending.metadata());
        case PARAM_LIST -> sendParamList(snapshot, requesterUuid, pending.metadata());
        case PARAM_GET -> sendParamGet(snapshot, requesterUuid, pending.metadata());
        case PARAM_SET -> sendParamSet(snapshot, requesterUuid, pending.metadata());
        case PARAM_DESCRIBE -> sendParamDescribe(snapshot, requesterUuid, pending.metadata());
        case PARAM_DUMP -> sendParamDump(snapshot, requesterUuid, pending.metadata());
        case SERVICE_CALL -> sendServiceCall(snapshot, requesterUuid, pending.metadata());
        case PLAYERS, TOPIC_INFO, SERVICE_INFO, INTERFACE_SHOW, TOPIC_HZ, TOPIC_BW -> {
          return;
        }
      }
    }

    @Override
    public void onNodeInfoResponse(NodeInfoResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      if (pending != null && pending.kind() != PendingRequestKind.NODE_INFO) {
        return;
      }

      if (!response.found()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Node not found: ").formatted(Formatting.RED))
                .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Node info for ").formatted(Formatting.GOLD))
              .append(Text.literal(response.nodeName()).formatted(Formatting.YELLOW)));

      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Publishers",
          response.publishers(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.AQUA);
      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Subscribers",
          response.subscribers(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.GREEN);
      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Services",
          response.services(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.BLUE);
    }

    @Override
    public void onInterfaceListResponse(InterfaceListResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      if (pending != null && pending.kind() != PendingRequestKind.INTERFACE_LIST) {
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Interface list #").formatted(Formatting.GOLD))
              .append(
                  Text.literal(String.valueOf(response.requestId())).formatted(Formatting.YELLOW)));

      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Messages", response.messages(), Formatting.GRAY);
      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Services", response.services(), Formatting.GRAY);
      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Actions", response.actions(), Formatting.GRAY);
    }

    private void sendGraphSnapshotPreview(GraphSnapshot snapshot, UUID requesterUuid) {
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Graph reply #" + snapshot.requestId() + " ")
                  .formatted(Formatting.GOLD))
              .append(Text.literal("nodes=" + snapshot.nodes().size()
                      + ", topics="
                      + snapshot.topics().size()
                      + ", services="
                      + snapshot.services().size()
                      + ", actions="
                      + snapshot.actions().size())
                  .formatted(Formatting.GREEN)));

      sendListPreviewToRequesterOrOperators(
          requesterUuid,
          "Nodes",
          snapshot.nodes().stream().map(NodeEntry::name).toList(),
          Formatting.LIGHT_PURPLE);
      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Topics",
          snapshot.topics(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.AQUA);
      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Services",
          snapshot.services(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.BLUE);
      sendEntryListPreviewToRequesterOrOperators(
          requesterUuid,
          "Actions",
          snapshot.actions(),
          entry -> entry.name() + " [" + entry.type() + "]",
          Formatting.DARK_AQUA);
    }

    private void sendNodeList(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");
      boolean countOnly = metadataFlagEnabled(metadata, "count_only");

      List<String> names = snapshot.nodes().stream()
          .map(NodeEntry::name)
          .filter(name -> includeHidden || !isHiddenName(name))
          .sorted()
          .toList();

      if (countOnly) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Node count: ").formatted(Formatting.GOLD))
                .append(Text.literal(String.valueOf(names.size())).formatted(Formatting.GREEN)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix().append(Text.literal("Nodes [" + names.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.LIGHT_PURPLE);
    }

    private void sendTopicList(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      boolean showTypes = metadataFlagEnabled(metadata, "show_types");
      boolean countOnly = metadataFlagEnabled(metadata, "count_only");
      boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");

      List<String> values;
      if (showTypes) {
        values = snapshot.topics().stream()
            .filter(entry -> includeHidden || !isHiddenName(entry.name()))
            .map(entry -> entry.name() + " [" + entry.type() + "]")
            .sorted()
            .toList();
      } else {
        values = snapshot.topics().stream()
            .map(entry -> entry.name())
            .filter(name -> includeHidden || !isHiddenName(name))
            .distinct()
            .sorted()
            .toList();
      }

      if (countOnly) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Topic count: ").formatted(Formatting.GOLD))
                .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topics [" + values.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.AQUA);
    }

    private void sendTopicType(GraphSnapshot snapshot, UUID requesterUuid, String topicName) {
      if (topicName == null || topicName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Topic name is missing.").formatted(Formatting.RED)));
        return;
      }

      List<String> types = snapshot.topics().stream()
          .filter(entry -> entry.name().equals(topicName))
          .map(entry -> entry.type())
          .filter(type -> !type.isBlank())
          .distinct()
          .sorted()
          .toList();

      if (types.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Topic not found: ").formatted(Formatting.RED))
                .append(Text.literal(topicName).formatted(Formatting.YELLOW)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topic type for ").formatted(Formatting.GOLD))
              .append(Text.literal(topicName).formatted(Formatting.YELLOW))
              .append(Text.literal(":").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
    }

    private void sendTopicFind(GraphSnapshot snapshot, UUID requesterUuid, String topicType) {
      if (topicType == null || topicType.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Topic type is missing.").formatted(Formatting.RED)));
        return;
      }

      List<String> names = snapshot.topics().stream()
          .filter(entry -> entry.type().equals(topicType))
          .map(entry -> entry.name())
          .distinct()
          .sorted()
          .toList();

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topics of type ").formatted(Formatting.GOLD))
              .append(Text.literal(topicType).formatted(Formatting.YELLOW))
              .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.AQUA);
    }

    private void sendServiceList(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      boolean showTypes = metadataFlagEnabled(metadata, "show_types");
      boolean countOnly = metadataFlagEnabled(metadata, "count_only");
      boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");

      List<String> values;
      if (showTypes) {
        values = snapshot.services().stream()
            .filter(entry -> includeHidden || !isHiddenName(entry.name()))
            .map(entry -> entry.name() + " [" + entry.type() + "]")
            .sorted()
            .toList();
      } else {
        values = snapshot.services().stream()
            .map(entry -> entry.name())
            .filter(name -> includeHidden || !isHiddenName(name))
            .distinct()
            .sorted()
            .toList();
      }

      if (countOnly) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Service count: ").formatted(Formatting.GOLD))
                .append(Text.literal(String.valueOf(values.size())).formatted(Formatting.GREEN)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Services [" + values.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.BLUE);
    }

    private void sendServiceType(GraphSnapshot snapshot, UUID requesterUuid, String serviceName) {
      if (serviceName == null || serviceName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Service name is missing.").formatted(Formatting.RED)));
        return;
      }

      List<String> types = snapshot.services().stream()
          .filter(entry -> entry.name().equals(serviceName))
          .map(entry -> entry.type())
          .filter(type -> !type.isBlank())
          .distinct()
          .sorted()
          .toList();

      if (types.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Service not found: ").formatted(Formatting.RED))
                .append(Text.literal(serviceName).formatted(Formatting.YELLOW)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Service type for ").formatted(Formatting.GOLD))
              .append(Text.literal(serviceName).formatted(Formatting.YELLOW))
              .append(Text.literal(":").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
    }

    private void sendServiceFind(GraphSnapshot snapshot, UUID requesterUuid, String serviceType) {
      if (serviceType == null || serviceType.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Service type is missing.").formatted(Formatting.RED)));
        return;
      }

      List<String> names = snapshot.services().stream()
          .filter(entry -> entry.type().equals(serviceType))
          .map(entry -> entry.name())
          .distinct()
          .sorted()
          .toList();

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Services of type ").formatted(Formatting.GOLD))
              .append(Text.literal(serviceType).formatted(Formatting.YELLOW))
              .append(Text.literal(" [" + names.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, names, Formatting.BLUE);
    }

    private void sendActionList(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      boolean showTypes = metadataFlagEnabled(metadata, "show_types");

      List<String> values;
      if (showTypes) {
        values = snapshot.actions().stream()
            .map(entry -> entry.name() + " [" + entry.type() + "]")
            .sorted()
            .toList();
      } else {
        values = snapshot.actions().stream()
            .map(entry -> entry.name())
            .distinct()
            .sorted()
            .toList();
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Actions [" + values.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, values, Formatting.DARK_AQUA);
    }

    private void sendActionType(GraphSnapshot snapshot, UUID requesterUuid, String actionName) {
      if (actionName == null || actionName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Action name is missing.").formatted(Formatting.RED)));
        return;
      }

      List<String> types = snapshot.actions().stream()
          .filter(entry -> entry.name().equals(actionName))
          .map(entry -> entry.type())
          .filter(type -> !type.isBlank())
          .distinct()
          .sorted()
          .toList();

      if (types.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Action not found: ").formatted(Formatting.RED))
                .append(Text.literal(actionName).formatted(Formatting.YELLOW)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Action type for ").formatted(Formatting.GOLD))
              .append(Text.literal(actionName).formatted(Formatting.YELLOW))
              .append(Text.literal(":").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.AQUA);
    }

    private void sendActionInfo(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String actionName = metadataValue(metadata, "action_name");
      if (actionName == null || actionName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Action name is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean includeHidden = metadataFlagEnabled(metadata, "include_hidden");

      List<String> types = snapshot.actions().stream()
          .filter(entry -> entry.name().equals(actionName))
          .filter(entry -> includeHidden || !isHiddenName(entry.name()))
          .map(entry -> entry.type())
          .filter(type -> !type.isBlank())
          .distinct()
          .sorted()
          .toList();

      if (types.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Action not found: ").formatted(Formatting.RED))
                .append(Text.literal(actionName).formatted(Formatting.YELLOW)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Action info for ").formatted(Formatting.GOLD))
              .append(Text.literal(actionName).formatted(Formatting.YELLOW))
              .append(Text.literal(" [types=" + types.size() + "]").formatted(Formatting.GOLD)));
      sendValuesToRequesterOrOperators(requesterUuid, types, Formatting.DARK_AQUA);
    }

    private void sendServiceCall(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String serviceName = metadataValue(metadata, "service_name");
      String serviceType = metadataValue(metadata, "service_type");
      String timeout = metadataValue(metadata, "timeout_seconds");
      String repeat = metadataValue(metadata, "repeat_count");
      String rate = metadataValue(metadata, "rate_hz");

      if (serviceName == null || serviceName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Service name is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean found = snapshot.services().stream()
          .anyMatch(entry -> entry.name().equals(serviceName)
              && (serviceType == null
                  || serviceType.isBlank()
                  || entry.type().equals(serviceType)));

      MutableText line = prefix()
          .append(Text.literal("Service call ").formatted(Formatting.GOLD))
          .append(Text.literal(serviceName).formatted(Formatting.YELLOW));
      if (serviceType != null && !serviceType.isBlank()) {
        line.append(Text.literal(" [" + serviceType + "]").formatted(Formatting.AQUA));
      }
      line.append(Text.literal(found ? " is available" : " not found")
          .formatted(found ? Formatting.GREEN : Formatting.RED));
      line.append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY));
      sendToRequesterOrOperators(requesterUuid, line);

      sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  timeout=" + (timeout == null ? "0.0" : timeout)
                  + ", repeat="
                  + (repeat == null ? "0" : repeat)
                  + ", rate_hz="
                  + (rate == null ? "0.0" : rate))
              .formatted(Formatting.GRAY));
    }

    private void sendActionSendGoal(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String actionName = metadataValue(metadata, "action_name");
      String actionType = metadataValue(metadata, "action_type");
      boolean feedback = metadataFlagEnabled(metadata, "feedback");
      String timeout = metadataValue(metadata, "timeout_seconds");

      if (actionName == null || actionName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Action name is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean found = snapshot.actions().stream()
          .anyMatch(entry -> entry.name().equals(actionName)
              && (actionType == null || actionType.isBlank() || entry.type().equals(actionType)));

      MutableText line = prefix()
          .append(Text.literal("Action send_goal ").formatted(Formatting.GOLD))
          .append(Text.literal(actionName).formatted(Formatting.YELLOW));
      if (actionType != null && !actionType.isBlank()) {
        line.append(Text.literal(" [" + actionType + "]").formatted(Formatting.AQUA));
      }
      line.append(Text.literal(found ? " is available" : " not found")
          .formatted(found ? Formatting.GREEN : Formatting.RED));
      line.append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY));
      sendToRequesterOrOperators(requesterUuid, line);

      sendToRequesterOrOperators(
          requesterUuid,
          Text.literal(
                  "  feedback=" + feedback + ", timeout=" + (timeout == null ? "0.0" : timeout))
              .formatted(Formatting.GRAY));
    }

    private void sendParamList(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String nodeName = metadataValue(metadata, "node");
      String depth = metadataValue(metadata, "depth");
      boolean includeTypes = metadataFlagEnabled(metadata, "include_types");
      String filter = metadataValue(metadata, "filter");

      if (nodeName == null || nodeName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Node name is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean nodeExists =
          snapshot.nodes().stream().anyMatch(entry -> entry.name().equals(nodeName));
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Param list for ").formatted(Formatting.GOLD))
              .append(Text.literal(nodeName).formatted(Formatting.YELLOW))
              .append(Text.literal(nodeExists ? " (node found)" : " (node not found)")
                  .formatted(nodeExists ? Formatting.GREEN : Formatting.RED))
              .append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY)));
      sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  depth=" + (depth == null ? "0" : depth)
                  + ", include_types="
                  + includeTypes
                  + ", filter="
                  + (filter == null ? "" : filter))
              .formatted(Formatting.GRAY));
    }

    private void sendParamGet(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String nodeName = metadataValue(metadata, "node");
      String paramName = metadataValue(metadata, "param");
      boolean hideType = metadataFlagEnabled(metadata, "hide_type");

      if (nodeName == null || nodeName.isBlank() || paramName == null || paramName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Node/param is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean nodeExists =
          snapshot.nodes().stream().anyMatch(entry -> entry.name().equals(nodeName));
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Param get ").formatted(Formatting.GOLD))
              .append(Text.literal(nodeName + "/" + paramName).formatted(Formatting.YELLOW))
              .append(Text.literal(nodeExists ? " (node found)" : " (node not found)")
                  .formatted(nodeExists ? Formatting.GREEN : Formatting.RED))
              .append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY)));
      sendToRequesterOrOperators(
          requesterUuid, Text.literal("  hide_type=" + hideType).formatted(Formatting.GRAY));
    }

    private void sendParamSet(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String nodeName = metadataValue(metadata, "node");
      String paramName = metadataValue(metadata, "param");
      String timeout = metadataValue(metadata, "timeout_seconds");

      if (nodeName == null || nodeName.isBlank() || paramName == null || paramName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Node/param is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean nodeExists =
          snapshot.nodes().stream().anyMatch(entry -> entry.name().equals(nodeName));
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Param set ").formatted(Formatting.GOLD))
              .append(Text.literal(nodeName + "/" + paramName).formatted(Formatting.YELLOW))
              .append(Text.literal(nodeExists ? " (node found)" : " (node not found)")
                  .formatted(nodeExists ? Formatting.GREEN : Formatting.RED))
              .append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY)));
      sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  timeout=" + (timeout == null ? "0.0" : timeout))
              .formatted(Formatting.GRAY));
    }

    private void sendParamDescribe(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String nodeName = metadataValue(metadata, "node");
      String paramName = metadataValue(metadata, "param");

      if (nodeName == null || nodeName.isBlank() || paramName == null || paramName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Node/param is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean nodeExists =
          snapshot.nodes().stream().anyMatch(entry -> entry.name().equals(nodeName));
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Param describe ").formatted(Formatting.GOLD))
              .append(Text.literal(nodeName + "/" + paramName).formatted(Formatting.YELLOW))
              .append(Text.literal(nodeExists ? " (node found)" : " (node not found)")
                  .formatted(nodeExists ? Formatting.GREEN : Formatting.RED))
              .append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY)));
    }

    private void sendParamDump(GraphSnapshot snapshot, UUID requesterUuid, String metadata) {
      String nodeName = metadataValue(metadata, "node");
      String prefixCount = metadataValue(metadata, "prefix_count");

      if (nodeName == null || nodeName.isBlank()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix().append(Text.literal("Node name is missing.").formatted(Formatting.RED)));
        return;
      }

      boolean nodeExists =
          snapshot.nodes().stream().anyMatch(entry -> entry.name().equals(nodeName));
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Param dump for ").formatted(Formatting.GOLD))
              .append(Text.literal(nodeName).formatted(Formatting.YELLOW))
              .append(Text.literal(nodeExists ? " (node found)" : " (node not found)")
                  .formatted(nodeExists ? Formatting.GREEN : Formatting.RED))
              .append(Text.literal(" (placeholder backend)").formatted(Formatting.DARK_GRAY)));
      sendToRequesterOrOperators(
          requesterUuid,
          Text.literal("  prefix_count=" + (prefixCount == null ? "0" : prefixCount))
              .formatted(Formatting.GRAY));
    }

    private void sendValuesToRequesterOrOperators(
        UUID requesterUuid, List<String> values, Formatting color) {
      if (values.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid, Text.literal("  (none)").formatted(Formatting.DARK_GRAY));
        return;
      }

      int count = Math.min(values.size(), MAX_COMMAND_ITEMS_TO_SHOW);
      for (int i = 0; i < count; i++) {
        sendToRequesterOrOperators(
            requesterUuid, Text.literal("  " + values.get(i)).formatted(color));
      }

      if (values.size() > MAX_COMMAND_ITEMS_TO_SHOW) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal("  ... and " + (values.size() - MAX_COMMAND_ITEMS_TO_SHOW) + " more")
                .formatted(Formatting.GRAY));
      }
    }

    private boolean metadataFlagEnabled(String metadata, String key) {
      String value = metadataValue(metadata, key);
      return "1".equals(value) || "true".equalsIgnoreCase(value);
    }

    private String metadataValue(String metadata, String key) {
      if (metadata == null || metadata.isBlank()) {
        return null;
      }

      for (String token : metadata.split(";")) {
        String trimmed = token.trim();
        int separatorIndex = trimmed.indexOf('=');
        if (separatorIndex <= 0 || separatorIndex + 1 >= trimmed.length()) {
          continue;
        }
        if (!trimmed.substring(0, separatorIndex).equals(key)) {
          continue;
        }
        return trimmed.substring(separatorIndex + 1);
      }

      return null;
    }

    private boolean isHiddenName(String name) {
      for (String segment : name.split("/")) {
        if (!segment.isEmpty() && segment.startsWith("_")) {
          return true;
        }
      }
      return false;
    }

    @Override
    public void onTopicPayload(TopicPayload payload) {
      LOGGER.trace(
          "Topic payload: {} ({} bytes, requestId={}, raw={})",
          payload.topicName(),
          payload.payloadLength(),
          payload.requestId(),
          payload.raw());

      PendingRequest pending = pendingRequest(payload.requestId());
      if (pending != null && pending.kind() != PendingRequestKind.TOPIC_ECHO) {
        completeRequest(payload.requestId());
        return;
      }

      UUID requesterUuid = pending == null ? null : pending.requesterUuid();
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topic echo ").formatted(Formatting.GOLD))
              .append(Text.literal(payload.topicName()).formatted(Formatting.YELLOW))
              .append(Text.literal(" (" + payload.messageType() + ") ").formatted(Formatting.GRAY))
              .append(Text.literal("bytes=").formatted(Formatting.DARK_GRAY))
              .append(
                  Text.literal(String.valueOf(payload.payloadLength())).formatted(Formatting.GREEN))
              .append(Text.literal(" raw=").formatted(Formatting.DARK_GRAY))
              .append(Text.literal(String.valueOf(payload.raw())).formatted(Formatting.AQUA)));

      if (pending != null && metadataFlagEnabled(pending.metadata(), "once")) {
        completeRequest(payload.requestId());
      }
    }

    @Override
    public void onPlayerList(PlayerList playerList) {
      LOGGER.debug("Player list received: {} players", playerList.size());

      PendingRequest pending = completeRequest(playerList.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      if (pending != null && pending.kind() == PendingRequestKind.CONNECTION_CHECK) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Connection probe #").formatted(Formatting.GOLD))
                .append(Text.literal(String.valueOf(playerList.requestId()))
                    .formatted(Formatting.YELLOW))
                .append(Text.literal(" succeeded. ").formatted(Formatting.GREEN))
                .append(Text.literal("Bridge replied with " + playerList.size() + " players.")
                    .formatted(Formatting.GRAY)));
        return;
      }

      if (pending != null && pending.kind() != PendingRequestKind.PLAYERS) {
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Player list reply #" + playerList.requestId() + " ")
                  .formatted(Formatting.GOLD))
              .append(Text.literal("count=" + playerList.size()).formatted(Formatting.GREEN)));

      int count = Math.min(playerList.players().size(), MAX_PLAYERS_TO_SHOW);
      for (int i = 0; i < count; i++) {
        Player player = playerList.players().get(i);
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - ")
                .formatted(Formatting.DARK_GRAY)
                .append(Text.literal(player.name()).formatted(Formatting.YELLOW))
                .append(Text.literal(
                        String.format(" (%.1f, %.1f, %.1f)", player.x(), player.y(), player.z()))
                    .formatted(Formatting.GRAY)));
      }

      if (playerList.players().size() > MAX_PLAYERS_TO_SHOW) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(
                    " - ... and " + (playerList.players().size() - MAX_PLAYERS_TO_SHOW) + " more")
                .formatted(Formatting.GRAY));
      }
    }

    @Override
    public void onTopicInfoResponse(TopicInfoResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();
      boolean verbose = pending != null && metadataFlagEnabled(pending.metadata(), "verbose");

      if (pending != null && pending.kind() != PendingRequestKind.TOPIC_INFO) {
        return;
      }

      MutableText line = prefix()
          .append(
              Text.literal("Topic info #" + response.requestId() + " ").formatted(Formatting.GOLD))
          .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW));
      if (response.hasMessageType()) {
        line.append(Text.literal(" [" + response.messageType() + "]").formatted(Formatting.AQUA));
      } else {
        line.append(Text.literal(" [unknown]").formatted(Formatting.GRAY));
      }
      line.append(Text.literal(" publishers=" + response.publisherCount()
              + ", subscribers="
              + response.subscriberCount())
          .formatted(Formatting.GREEN));
      sendToRequesterOrOperators(requesterUuid, line);

      if (!verbose) {
        return;
      }

      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Publisher nodes", response.publisherNodes(), Formatting.GRAY);
      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Subscriber nodes", response.subscriberNodes(), Formatting.GRAY);
    }

    @Override
    public void onServiceInfoResponse(ServiceInfoResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();
      boolean verbose = pending != null && metadataFlagEnabled(pending.metadata(), "verbose");

      if (pending != null && pending.kind() != PendingRequestKind.SERVICE_INFO) {
        return;
      }

      MutableText line = prefix()
          .append(Text.literal("Service info #" + response.requestId() + " ")
              .formatted(Formatting.GOLD))
          .append(Text.literal(response.serviceName()).formatted(Formatting.YELLOW));
      if (response.hasServiceType()) {
        line.append(Text.literal(" [" + response.serviceType() + "]").formatted(Formatting.AQUA));
      } else {
        line.append(Text.literal(" [unknown]").formatted(Formatting.GRAY));
      }
      line.append(
          Text.literal(" clients=" + response.clientCount() + ", servers=" + response.serverCount())
              .formatted(Formatting.GREEN));
      sendToRequesterOrOperators(requesterUuid, line);

      if (!verbose) {
        return;
      }

      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Client nodes", response.clientNodes(), Formatting.GRAY);
      sendListPreviewToRequesterOrOperators(
          requesterUuid, "Server nodes", response.serverNodes(), Formatting.GRAY);
    }

    @Override
    public void onInterfaceShowResponse(InterfaceShowResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();
      boolean noComments =
          pending != null && metadataFlagEnabled(pending.metadata(), "no_comments");

      if (pending != null && pending.kind() != PendingRequestKind.INTERFACE_SHOW) {
        return;
      }

      if (!response.found()) {
        sendToRequesterOrOperators(
            requesterUuid,
            prefix()
                .append(Text.literal("Interface " + response.interfaceType() + " not found")
                    .formatted(Formatting.RED)));
        return;
      }

      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Interface ").formatted(Formatting.GOLD))
              .append(Text.literal(response.interfaceType()).formatted(Formatting.YELLOW))
              .append(Text.literal(":").formatted(Formatting.GOLD)));

      for (String line : response.definition().split("\\R", -1)) {
        if (noComments && line.stripLeading().startsWith("#")) {
          continue;
        }
        if (line.isEmpty()) {
          continue;
        }
        sendToRequesterOrOperators(
            requesterUuid, Text.literal("  " + line).formatted(Formatting.GRAY));
      }
    }

    @Override
    public void onError(BridgeError error) {
      LOGGER.error("Bridge error: [{}] {}", error.errorCode(), error.errorMessage());

      PendingRequest pending = completeRequest(error.requestId());
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      String userMessage =
          switch (error.errorCode()) {
            case "SUBSCRIBE_FAILED" -> "Failed to subscribe: " + error.errorMessage();
            case "SUBSCRIBE_TIMEOUT" -> "Topic echo timed out: " + error.errorMessage();
            case "PUBLISH_FAILED" -> "Failed to publish: " + error.errorMessage();
            default -> "Error: " + error.errorMessage();
          };
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("[").formatted(Formatting.DARK_RED))
              .append(Text.literal(error.errorCode()).formatted(Formatting.RED))
              .append(Text.literal("] ").formatted(Formatting.DARK_RED))
              .append(Text.literal(userMessage).formatted(Formatting.RED)));
    }

    @Override
    public void onTopicHzResponse(TopicHzResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      if (pending != null && pending.kind() != PendingRequestKind.TOPIC_HZ) {
        return;
      }
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      String formattedFreq = String.format("%.2f", response.frequency());
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topic hz ").formatted(Formatting.GOLD))
              .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
              .append(Text.literal(" average rate: ").formatted(Formatting.GOLD))
              .append(Text.literal(formattedFreq).formatted(Formatting.GREEN))
              .append(Text.literal(" Hz").formatted(Formatting.GREEN))
              .append(
                  Text.literal("  min: 0.00 Hz  max: " + String.format("%.2f", response.frequency())
                          + " Hz"
                          + "  window: "
                          + response.window()
                          + "  samples: "
                          + response.messageCount())
                      .formatted(Formatting.GRAY)));
    }

    @Override
    public void onTopicBwResponse(TopicBwResponse response) {
      PendingRequest pending = completeRequest(response.requestId());
      if (pending != null && pending.kind() != PendingRequestKind.TOPIC_BW) {
        return;
      }
      UUID requesterUuid = pending == null ? null : pending.requesterUuid();

      String formattedBw = String.format("%.2f", response.bytesPerSecond());
      String formattedTotal = String.format("%.2f KB", response.totalBytes() / 1024.0);
      sendToRequesterOrOperators(
          requesterUuid,
          prefix()
              .append(Text.literal("Topic bw ").formatted(Formatting.GOLD))
              .append(Text.literal(response.topicName()).formatted(Formatting.YELLOW))
              .append(Text.literal(" average: ").formatted(Formatting.GOLD))
              .append(Text.literal(formattedBw + " B/s").formatted(Formatting.GREEN))
              .append(Text.literal("  total: " + formattedTotal
                      + "  window: "
                      + response.window()
                      + "  messages: "
                      + response.messageCount())
                  .formatted(Formatting.GRAY)));
    }

    private void sendListPreviewToRequesterOrOperators(
        UUID requesterUuid, String label, List<String> values, Formatting color) {
      if (values.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
        return;
      }

      int count = Math.min(values.size(), MAX_GRAPH_ITEMS_TO_SHOW);
      for (int i = 0; i < count; i++) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label + ": ")
                .formatted(Formatting.DARK_GRAY)
                .append(Text.literal(values.get(i)).formatted(color)));
      }

      if (values.size() > MAX_GRAPH_ITEMS_TO_SHOW) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label
                    + ": ... and "
                    + (values.size() - MAX_GRAPH_ITEMS_TO_SHOW)
                    + " more")
                .formatted(Formatting.GRAY));
      }
    }

    private <T> void sendEntryListPreviewToRequesterOrOperators(
        UUID requesterUuid,
        String label,
        List<T> entries,
        java.util.function.Function<T, String> formatter,
        Formatting color) {
      if (entries.isEmpty()) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label + ": (none)").formatted(Formatting.DARK_GRAY));
        return;
      }

      int count = Math.min(entries.size(), MAX_GRAPH_ITEMS_TO_SHOW);
      for (int i = 0; i < count; i++) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label + ": ")
                .formatted(Formatting.DARK_GRAY)
                .append(Text.literal(formatter.apply(entries.get(i))).formatted(color)));
      }

      if (entries.size() > MAX_GRAPH_ITEMS_TO_SHOW) {
        sendToRequesterOrOperators(
            requesterUuid,
            Text.literal(" - " + label
                    + ": ... and "
                    + (entries.size() - MAX_GRAPH_ITEMS_TO_SHOW)
                    + " more")
                .formatted(Formatting.GRAY));
      }
    }
  }

  private void sendToRequesterOrOperators(UUID requesterUuid, Text message) {
    if (requesterUuid != null) {
      ServerPlayerEntity player = findPlayer(requesterUuid);
      if (player != null) {
        player.sendMessage(message.copy(), false);
        return;
      }
    }
    broadcastToOperators(message);
  }

  private ServerPlayerEntity findPlayer(UUID requesterUuid) {
    var currentServer = server;
    if (currentServer == null) {
      return null;
    }

    var playerManager = currentServer.getPlayerManager();
    if (playerManager == null) {
      return null;
    }

    return playerManager.getPlayer(requesterUuid);
  }

  private void broadcastToOperators(Text message) {
    var currentServer = server;
    if (currentServer == null) {
      return;
    }

    var playerManager = currentServer.getPlayerManager();
    if (playerManager == null) {
      return;
    }

    MutableText copy = message.copy();
    playerManager.getPlayerList().forEach(player -> {
      if (player.hasPermissionLevel(2)) {
        player.sendMessage(copy.copy(), false);
      }
    });
  }

  private static MutableText prefix() {
    return Text.literal("[Roscraft] ").formatted(Formatting.AQUA);
  }
}
