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

/**
 * Coordinates addon lifecycle, event dispatching, and command collection.
 *
 * <p>Delegates event routing to {@link EventDispatcher}, typed subscriptions
 * to {@link BridgeEventBusImpl}, local messaging to {@link LocalBusImpl},
 * and inter-addon signals to {@link AddonSignalBusImpl}.
 */
public final class AddonManager {

  private final RoscraftMod mod;
  private final Map<String, RoscraftAddon> addons = new HashMap<>();
  private final Map<String, AddonContext> contexts = new HashMap<>();
  private final EventDispatcher dispatcher;
  private final LocalBusImpl localBus;
  private final AddonSignalBusImpl signalBus;

  public AddonManager(RoscraftMod mod) {
    this.mod = Objects.requireNonNull(mod, "mod must not be null");
    this.addons.put("", null); // dummy — EventDispatcher references addons map
    this.dispatcher = new EventDispatcher(addons);
    this.localBus = new LocalBusImpl();
    this.signalBus = new AddonSignalBusImpl();
  }

  // ── Public bus accessors ────────────────────────────────────────────

  public BridgeEventBusImpl bridgeBus() {
    return dispatcher.bridgeBus();
  }

  public LocalBusImpl localBus() {
    return localBus;
  }

  public AddonSignalBusImpl signalBus() {
    return signalBus;
  }

  // ── Lifecycle ──────────────────────────────────────────────────────

  public void loadAddons() {
    addons.clear();
    contexts.clear();
    dispatcher.clear();
    localBus.clear();
    signalBus.clear();
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
      RoscraftMod.LOGGER.error(
          "Addon ID '{}' is already registered by {}. Skipping {}. "
              + "Each addon must return a unique addonId().",
          id,
          addons.get(id).getClass().getName(),
          addon.getClass().getName());
      return;
    }
    addons.put(id, addon);

    var requestRouter = dispatcher.requestRouter();
    Supplier<BridgeOperations> trackedBridgeSupplier = () -> {
      RoscraftBridge bridge = mod.bridgeManager().getBridge();
      return bridge != null ? new TrackedBridge(bridge, id, requestRouter) : null;
    };

    AddonContext.SendEventFunction sendEventFn =
        (addonId, eventType, encoding, payload, response) -> {
          RoscraftBridge bridge = mod.bridgeManager().getBridge();
          if (bridge == null) return AddonContext.DISCONNECTED;
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
    for (var e : addons.entrySet()) {
      try {
        e.getValue().shutdown();
      } catch (Exception ex) {
        RoscraftMod.LOGGER.warn("Error shutting down addon '{}': {}", e.getKey(), ex.getMessage());
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

  // ── Event dispatching ───────────────────────────────────────────────

  /** Called by ModBridgeCallback for every incoming bridge event. */
  public void onEvent(BridgeEvent event) {
    dispatcher.onEvent(event);
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

  // ── Tracked bridge wrapper ──────────────────────────────────────────

  /**
   * Wraps a {@link BridgeOperations} to auto-track every request.
   * One-shot operations use {@code track()}; persistent operations
   * (subscribe, sendGoal) use {@code trackPersistent()}.
   */
  private static final class TrackedBridge
      implements BridgeOperations,
          BridgeOperations.TopicOps,
          BridgeOperations.ParamOps,
          BridgeOperations.ServiceOps,
          BridgeOperations.ActionOps,
          BridgeOperations.GraphOps {

    private final BridgeOperations delegate;
    private final String addonId;
    private final AddonContext.RequestTracker router;

    TrackedBridge(BridgeOperations delegate, String addonId, AddonContext.RequestTracker router) {
      this.delegate = delegate;
      this.addonId = addonId;
      this.router = router;
    }

    // ── BridgeOperations accessors ────────────────────────────────────

    @Override
    public TopicOps topics() {
      return this;
    }

    @Override
    public ParamOps params() {
      return this;
    }

    @Override
    public ServiceOps services() {
      return this;
    }

    @Override
    public ActionOps actions() {
      return this;
    }

    @Override
    public GraphOps graph() {
      return this;
    }

    @Override
    public long queryPlayers() {
      long id = delegate.queryPlayers();
      router.track(addonId, id);
      return id;
    }

    @Override
    public long sendRawPacket(byte[] payload) {
      long id = delegate.sendRawPacket(payload);
      router.track(addonId, id);
      return id;
    }

    // ── Graph ─────────────────────────────────────────────────────────

    @Override
    public long snapshot() {
      long id = delegate.graph().snapshot();
      router.track(addonId, id);
      return id;
    }

    @Override
    public long nodeInfo(String n, boolean h) {
      long id = delegate.graph().nodeInfo(n, h);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long topicInfo(String t) {
      long id = delegate.graph().topicInfo(t);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long serviceInfo(String s) {
      long id = delegate.graph().serviceInfo(s);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long interfaceList(boolean m, boolean s, boolean a) {
      long id = delegate.graph().interfaceList(m, s, a);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long interfaceShow(String t) {
      long id = delegate.graph().interfaceShow(t);
      router.track(addonId, id);
      return id;
    }

    // ── Topic ─────────────────────────────────────────────────────────

    @Override
    public long subscribe(String t, String ty) {
      long id = delegate.topics().subscribe(t, ty);
      router.trackPersistent(addonId, id);
      return id;
    }

    @Override
    public long subscribe(String t, String ty, TopicOps.SubscribeOptions o) {
      long id = delegate.topics().subscribe(t, ty, o);
      router.trackPersistent(addonId, id);
      return id;
    }

    @Override
    public long unsubscribe(String t) {
      long id = delegate.topics().unsubscribe(t);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long publish(String t, String ty, byte[] p) {
      long id = delegate.topics().publish(t, ty, p);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long publish(String t, String ty, byte[] p, TopicOps.PublishOptions o) {
      long id = delegate.topics().publish(t, ty, p, o);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long hz(String t, String ty, int w) {
      long id = delegate.topics().hz(t, ty, w);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long hz(String t, String ty, int w, TopicOps.HzOptions o) {
      long id = delegate.topics().hz(t, ty, w, o);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long bw(String t, String ty, int w) {
      long id = delegate.topics().bw(t, ty, w);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long bw(String t, String ty, int w, TopicOps.BwOptions o) {
      long id = delegate.topics().bw(t, ty, w, o);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long delay(String t, String ty, int w) {
      long id = delegate.topics().delay(t, ty, w);
      router.track(addonId, id);
      return id;
    }

    // ── Param ─────────────────────────────────────────────────────────

    @Override
    public long list(String n, ParamOps.ParamListOptions o) {
      long id = delegate.params().list(n, o);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long get(String n, String p, ParamOps.ParamGetOptions o) {
      long id = delegate.params().get(n, p, o);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long set(String n, String p, String v, double t) {
      long id = delegate.params().set(n, p, v, t);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long describe(String n, String p, double t) {
      long id = delegate.params().describe(n, p, t);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long dump(String n, String[] pr, double t) {
      long id = delegate.params().dump(n, pr, t);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long load(String n, String y, ParamOps.ParamLoadOptions o) {
      long id = delegate.params().load(n, y, o);
      router.track(addonId, id);
      return id;
    }

    // ── Service ───────────────────────────────────────────────────────

    @Override
    public long call(String n, String t, byte[] p, ServiceOps.ServiceCallOptions o) {
      long id = delegate.services().call(n, t, p, o);
      router.track(addonId, id);
      return id;
    }

    // ── Action ────────────────────────────────────────────────────────

    @Override
    public long info(String n, boolean h) {
      long id = delegate.actions().info(n, h);
      router.track(addonId, id);
      return id;
    }

    @Override
    public long sendGoal(String n, String t, byte[] p, ActionOps.ActionGoalOptions o) {
      long id = delegate.actions().sendGoal(n, t, p, o);
      router.trackPersistent(addonId, id);
      return id;
    }
  }
}
