package net.roscraft.mod.addon;

import net.roscraft.bridge.event.BridgeEvent;

/**
 * Interface for roscraft mod addons.
 *
 * <p>Addons are discovered via Fabric entrypoint {@code roscraft:addon}.
 * Each addon receives an {@link AddonContext} providing access to the
 * bridge API, event buses, and automatic request tracking.
 *
 * <p>For less boilerplate, extend {@link AbstractRoscraftAddon} or use
 * {@link RoscraftAddons#builder(String)}. To register Minecraft commands under
 * {@code /ros}, implement {@code net.roscraft.mod.addon.minecraft.RoscraftAddonCommands}
 * in your addon mod (requires a dependency on the roscraft mod artifact).
 *
 * <h3>Request/response flow</h3>
 * All bridge operations invoked through {@code ctx.bridgeIfConnected()}
 * are automatically tracked — responses route to {@link #onBridgeEvent(BridgeEvent)}
 * without any manual tracking step.
 *
 * <h3>Using the event buses</h3>
 * <pre>{@code
 * ctx.bridgeBus().onAny(BridgeEvent.TopicPayload.class, this::handlePayload);
 * ctx.localBus().on(MyMessage.class, this::handleLocal);
 * ctx.signalBus().on("ping", this::handlePing);
 * }</pre>
 *
 * <h3>Lifecycle</h3>
 * <ol>
 * <li>{@link #init(AddonContext)} — called once during mod init</li>
 * <li>{@link #onBridgeEvent(BridgeEvent)} — called when tracked requests complete</li>
 * <li>{@link #shutdown()} — called during mod shutdown</li>
 * </ol>
 */
public interface RoscraftAddon {

  /** @return Non-null, non-blank unique identifier for this addon. */
  String addonId();

  default void init(AddonContext ctx) {}

  default void shutdown() {}

  /**
   * Called when a bridge response event arrives for a tracked request.
   *
   * <p>Use pattern matching to handle specific event types:
   * <pre>{@code
   * switch (event) {
   *     case BridgeEvent.GraphSnapshot s -> handleGraph(s);
   *     case BridgeEvent.TopicPayload p -> handlePayload(p);
   *     case BridgeEvent.BridgeError e -> handleError(e);
   *     default -> {}
   * }
   * }</pre>
   *
   * <p>Addons that prefer type-specific handlers should extend
   * {@link AbstractRoscraftAddon} and use {@code on(Class, Consumer)} in
   * {@link AbstractRoscraftAddon#configure()}, or subscribe via
   * {@code ctx.bridgeBus().onAny(Class, Consumer)} for global events.
   */
  default void onBridgeEvent(BridgeEvent event) {}
}
