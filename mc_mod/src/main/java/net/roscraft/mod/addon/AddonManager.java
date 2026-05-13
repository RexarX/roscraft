package net.roscraft.mod.addon;

import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.function.Consumer;
import net.fabricmc.loader.api.FabricLoader;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.bridge.event.EventBus;
import net.roscraft.mod.RoscraftMod;

public final class AddonManager implements EventBus {

  private final RoscraftMod mod;
  private final Map<String, RoscraftAddon> addons = new HashMap<>();
  private final Map<String, AddonContext> contexts = new HashMap<>();
  private final RequestRouter requestRouter = new RequestRouter();
  private final Map<Class<?>, List<Consumer<?>>> typedSubscribers = new HashMap<>();
  private final Map<Class<?>, List<Consumer<?>>> localSubscribers = new HashMap<>();
  private final Map<String, List<Consumer<BridgeEvent.AddonEvent>>> addonEventSubscribers =
      new HashMap<>();

  public AddonManager(RoscraftMod mod) {
    this.mod = Objects.requireNonNull(mod, "mod must not be null");
  }

  // ── Lifecycle ──────────────────────────────────────────────────────

  public void loadAddons() {
    addons.clear();
    contexts.clear();
    requestRouter.clear();
    // PingAddon is hardcoded for guaranteed-first registration so it
    // handles ROS-side ping requests before any user addons are loaded.
    registerAddon(new PingAddon());
    var entrypoints =
        FabricLoader.getInstance().getEntrypoints("roscraft:addon", RoscraftAddon.class);
    for (var addon : entrypoints) {
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
      RoscraftMod.LOGGER.warn(
          "Addon '{}' already registered, skipping: {}", id, addon.getClass().getName());
      return;
    }
    addons.put(id, addon);
    var ctx = new AddonContext(id, () -> mod.bridgeManager().getBridge(), requestRouter, this);
    contexts.put(id, ctx);
    addon.init(ctx);
    RoscraftMod.LOGGER.info("Registered addon: {}", id);
  }

  public void shutdown() {
    for (var e : addons.entrySet()) {
      try {
        e.getValue().shutdown();
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn("Error shutting down addon '{}': {}", e.getKey(), ex.getMessage());
      }
    }
    addons.clear();
    contexts.clear();
    requestRouter.clear();
    typedSubscribers.clear();
    localSubscribers.clear();
    addonEventSubscribers.clear();
  }

  public int size() {
    return addons.size();
  }

  // ── EventBus implementation ─────────────────────────────────────────

  @Override
  @SuppressWarnings("unchecked")
  public <T extends BridgeEvent> Subscription subscribe(Class<T> type, Consumer<T> handler) {
    Objects.requireNonNull(type, "type must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    typedSubscribers.computeIfAbsent(type, k -> new CopyOnWriteArrayList<>()).add(handler);
    return () -> {
      var subs = typedSubscribers.get(type);
      if (subs != null) subs.remove(handler);
    };
  }

  @Override
  public Subscription subscribeAddonEvent(
      String eventType, Consumer<BridgeEvent.AddonEvent> handler) {
    Objects.requireNonNull(eventType, "eventType must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    addonEventSubscribers
        .computeIfAbsent(eventType, k -> new CopyOnWriteArrayList<>())
        .add(handler);
    return () -> {
      var subs = addonEventSubscribers.get(eventType);
      if (subs != null) subs.remove(handler);
    };
  }

  @Override
  public void publish(BridgeEvent.AddonEvent event) {
    Objects.requireNonNull(event, "event must not be null");
    var subs = addonEventSubscribers.get(event.eventType());
    if (subs != null) {
      for (var handler : subs) {
        try {
          handler.accept(event);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in addon event subscriber for '{}': {}", event.eventType(), ex.getMessage());
        }
      }
    }
  }

  @Override
  @SuppressWarnings("unchecked")
  public <T> Subscription subscribeLocal(Class<T> type, Consumer<T> handler) {
    Objects.requireNonNull(type, "type must not be null");
    Objects.requireNonNull(handler, "handler must not be null");
    localSubscribers.computeIfAbsent(type, k -> new CopyOnWriteArrayList<>()).add(handler);
    return () -> {
      var subs = localSubscribers.get(type);
      if (subs != null) subs.remove(handler);
    };
  }

  @Override
  @SuppressWarnings("unchecked")
  public <T> void publishLocal(T event) {
    Objects.requireNonNull(event, "event must not be null");
    var subs = localSubscribers.get(event.getClass());
    if (subs != null) {
      for (var handler : subs) {
        try {
          ((Consumer<T>) handler).accept(event);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in local event subscriber for {}: {}",
              event.getClass().getSimpleName(),
              ex.getMessage());
        }
      }
    }
  }

  // ── Event dispatching ───────────────────────────────────────────────

  /** Called by ModBridgeCallback for every incoming bridge event. */
  public void onEvent(BridgeEvent event) {
    dispatchToTypedSubscribers(event);
    dispatchToAddonOwner(event);
  }

  @SuppressWarnings("unchecked")
  private <T extends BridgeEvent> void dispatchToTypedSubscribers(T event) {
    var subs = typedSubscribers.get(event.getClass());
    if (subs != null) {
      for (var handler : subs) {
        try {
          ((Consumer<T>) handler).accept(event);
        } catch (Exception ex) {
          RoscraftMod.LOGGER.warn(
              "Error in typed event subscriber for {}: {}",
              event.getClass().getSimpleName(),
              ex.getMessage());
        }
      }
    }
  }

  private void dispatchToAddonOwner(BridgeEvent event) {
    RoscraftAddon addon;
    if (event instanceof BridgeEvent.AddonEvent ae) {
      addon = addons.get(ae.addonId());
    } else {
      addon = addons.get(requestRouter.consume(event.requestId()));
    }

    if (addon != null) {
      try {
        addon.onBridgeEvent(event);
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn(
            "Error dispatching event to addon '{}': {}", addon.addonId(), ex.getMessage());
      }
    }
  }

  // ── Commands ───────────────────────────────────────────────────────

  public List<LiteralArgumentBuilder<ServerCommandSource>> collectCommands() {
    var all = new ArrayList<LiteralArgumentBuilder<ServerCommandSource>>();
    for (var e : addons.entrySet()) {
      try {
        var commands = e.getValue().commands();
        if (commands != null) all.addAll(commands);
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn(
            "Error collecting commands from '{}': {}", e.getKey(), ex.getMessage());
      }
    }
    return Collections.unmodifiableList(all);
  }
}
