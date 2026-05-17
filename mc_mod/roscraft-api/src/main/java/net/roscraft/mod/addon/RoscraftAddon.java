package net.roscraft.mod.addon;

import com.mojang.brigadier.builder.LiteralArgumentBuilder;
import java.util.Collections;
import java.util.List;
import net.minecraft.server.command.ServerCommandSource;
import net.roscraft.bridge.event.BridgeEvent;

/**
 * Interface for roscraft mod addons.
 *
 * <p>Addons are discovered via Fabric entrypoint {@code roscraft:addon}.
 * Each addon receives an {@link AddonContext} providing access to the
 * bridge API, event buses, and automatic request tracking.
 *
 * <h3>Request/response flow</h3>
 * All bridge operations invoked through {@code ctx.bridgeIfConnected()}
 * are automatically tracked — responses route to {@link #onBridgeEvent(BridgeEvent)}
 * without any manual tracking step.
 *
 * <h3>Using the EventBuses</h3>
 * Addons can subscribe to specific event types via the context buses:
 * <pre>{@code
 * ctx.bridgeBus().onAny(BridgeEvent.TopicPayload.class, this::handlePayload);
 * ctx.localBus().on(MyMessage.class, this::handleLocal);
 * ctx.signalBus().on("ping", this::handlePing);
 * }</pre>
 *
 * <h3>Command registration</h3>
 * Override {@link #commands()} to return Brigadier literal nodes.
 * They are attached under {@code /ros} alongside built-in commands.
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

  // ── Lifecycle ──────────────────────────────────────────────────────

  default void init(AddonContext ctx) {}

  default void shutdown() {}

  // ── Commands ───────────────────────────────────────────────────────

  default List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return Collections.emptyList();
  }

  // ── Events ─────────────────────────────────────────────────────────

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
   * <p>Addons that prefer type-specific subscriptions should leave this
   * method with the default no-op implementation and use
   * {@code ctx.bridgeBus().onAny(Class, Consumer)} in {@link #init}.
   */
  default void onBridgeEvent(BridgeEvent event) {}
}
