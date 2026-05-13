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
 * bridge API, event bus, and request tracking.
 *
 * <h3>Request/response flow</h3>
 * <ol>
 * <li>Call any method on {@code ctx.bridge()} (e.g. {@code queryGraph()},
 *     {@code subscribeTopic()}, {@code nodeInfo()}, {@code serviceCall()},
 *     {@code paramGet()}, …).</li>
 * <li>Call {@code ctx.trackRequest(requestId)} to associate the returned
 *     request ID with this addon.</li>
 * <li>When the response arrives, {@link #onBridgeEvent(BridgeEvent)} is
 *     invoked with the deserialized event.</li>
 * </ol>
 *
 * <h3>Using the EventBus</h3>
 * As an alternative to overriding {@link #onBridgeEvent}, addons can
 * subscribe to specific event types via {@code ctx.eventBus()}:
 * <pre>{@code
 * ctx.eventBus().subscribe(BridgeEvent.TopicPayload.class, this::handlePayload);
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
   * {@code ctx.eventBus().subscribe(Class, Consumer)} in {@link #init}.
   */
  default void onBridgeEvent(BridgeEvent event) {}
}
