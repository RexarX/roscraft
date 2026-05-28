package net.roscraft.mod.addon;

import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.function.Supplier;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.bridge.BridgeOperations;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.addon.minecraft.RoscraftAddonCommands;
import net.roscraft.mod.bridge.BridgeRequestHub;

/**
 * Coordinates addon lifecycle, event dispatching, and command collection.
 */
public final class AddonManager {

  private final RoscraftMod mod;
  private final Map<String, RoscraftAddon> addons = new HashMap<>();
  private final LocalBusImpl localBus = new LocalBusImpl();
  private final AddonSignalBusImpl signalBus = new AddonSignalBusImpl();
  private final EventDispatcher dispatcher;
  private final Map<String, AddonContext> contexts = new HashMap<>();

  public AddonManager(RoscraftMod mod, BridgeRequestHub requestHub) {
    this.mod = Objects.requireNonNull(mod, "mod must not be null");
    Objects.requireNonNull(requestHub, "requestHub must not be null");
    this.dispatcher = new EventDispatcher(addons, requestHub.addons(), signalBus);
  }

  public BridgeEventBusImpl bridgeBus() {
    return dispatcher.bridgeBus();
  }

  public LocalBusImpl localBus() {
    return localBus;
  }

  public AddonSignalBusImpl signalBus() {
    return signalBus;
  }

  public void loadAddons() {
    addons.clear();
    contexts.clear();
    dispatcher.clear();
    localBus.clear();
    signalBus.clear();

    registerAddon(new PingAddon());
    for (var addon :
        FabricLoader.getInstance().getEntrypoints("roscraft:addon", RoscraftAddon.class)) {
      registerAddon(addon);
    }
  }

  private void registerAddon(RoscraftAddon addon) {
    String id = addon.addonId();
    if (id == null || id.isBlank()) {
      RoscraftMod.LOGGER.warn(
          "Skipping addon with empty addonId: {}", addon.getClass().getName());
      return;
    }
    if (addons.containsKey(id)) {
      RoscraftMod.LOGGER.error(
          "Addon ID '{}' is already registered by {}. Skipping {}. "
              + "Each addon must return a unique addonId().",
          id,
          addons.get(id).getClass().getName(),
          addon.getClass().getName());
      return;
    }
    addons.put(id, addon);

    RequestRouter requestRouter = dispatcher.requestRouter(); // shared hub router
    Supplier<BridgeOperations> trackedBridgeSupplier = () -> {
      RoscraftBridge bridge = mod.bridgeManager().getBridge();
      return bridge != null ? new TrackedBridge(bridge, id, requestRouter) : null;
    };

    AddonContext.SendEventFunction sendEventFn =
        (addonId, eventType, encoding, payload, response) -> {
          RoscraftBridge bridge = mod.bridgeManager().getBridge();
          if (bridge == null) {
            return AddonContext.DISCONNECTED;
          }
          return bridge.sendAddonEvent(addonId, eventType, encoding, payload, response);
        };

    var ctx = new AddonContext(
        id,
        trackedBridgeSupplier,
        requestRouter,
        dispatcher.bridgeBus(),
        localBus,
        signalBus,
        sendEventFn);
    contexts.put(id, ctx);
    addon.init(ctx);
    RoscraftMod.LOGGER.info("Registered addon: {}", id);
  }

  public void shutdown() {
    for (var entry : addons.entrySet()) {
      try {
        entry.getValue().shutdown();
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn(
            "Error shutting down addon '{}': {}", entry.getKey(), ex.getMessage());
      }
    }
    addons.clear();
    contexts.clear();
    dispatcher.clear();
    localBus.clear();
    signalBus.clear();
  }

  public int size() {
    return addons.size();
  }

  public void onEvent(BridgeEvent event) {
    dispatcher.onEvent(event);
  }

  public List<LiteralArgumentBuilder<ServerCommandSource>> collectCommands() {
    var all = new ArrayList<LiteralArgumentBuilder<ServerCommandSource>>();
    for (var entry : addons.entrySet()) {
      RoscraftAddon addon = entry.getValue();
      if (!(addon instanceof RoscraftAddonCommands commandAddon)) {
        continue;
      }
      try {
        var commands = commandAddon.commands();
        if (commands != null) {
          all.addAll(commands);
        }
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn(
            "Error collecting commands from '{}': {}", entry.getKey(), ex.getMessage());
      }
    }
    return Collections.unmodifiableList(all);
  }
}
