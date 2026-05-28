package net.roscraft.mod.addon;

import java.util.Objects;
import java.util.function.Consumer;
import net.roscraft.bridge.event.BridgeEvent;

/**
 * Factory helpers for lightweight addons without a dedicated subclass file.
 *
 * <pre>{@code
 * public final class PingMod implements ModInitializer {
 *   public void onInitialize() {
 *     // Usually registered via fabric.mod.json entrypoint instead:
 *   }
 * }
 *
 * // fabric.mod.json:
 * // "roscraft:addon": ["your.mod.YourAddon"]
 *
 * // Or inline for tests:
 * RoscraftAddons.builder("demo")
 *     .onInit(ctx -> ctx.signalBus().on("ping", e -> { ... }))
 *     .onBridgeEvent(e -> { if (e instanceof BridgeEvent.TopicPayload p) { ... } })
 *     .build();
 * }</pre>
 */
public final class RoscraftAddons {

  private RoscraftAddons() {}

  public static Builder builder(String addonId) {
    return new Builder(addonId);
  }

  public static RoscraftAddon of(String addonId, Consumer<AddonContext> onInit) {
    return builder(addonId).onInit(onInit).build();
  }

  public static final class Builder {
    private final String addonId;
    private Consumer<AddonContext> onInit = ctx -> {};
    private Consumer<BridgeEvent> onBridgeEvent = event -> {};
    private Runnable onShutdown = () -> {};

    Builder(String addonId) {
      if (addonId == null || addonId.isBlank()) {
        throw new IllegalArgumentException("addonId must not be null or blank");
      }
      this.addonId = addonId;
    }

    public Builder onInit(Consumer<AddonContext> onInit) {
      this.onInit = Objects.requireNonNull(onInit, "onInit must not be null");
      return this;
    }

    public Builder onBridgeEvent(Consumer<BridgeEvent> onBridgeEvent) {
      this.onBridgeEvent = Objects.requireNonNull(onBridgeEvent, "onBridgeEvent must not be null");
      return this;
    }

    public Builder onShutdown(Runnable onShutdown) {
      this.onShutdown = Objects.requireNonNull(onShutdown, "onShutdown must not be null");
      return this;
    }

    public RoscraftAddon build() {
      return new RoscraftAddon() {
        @Override
        public String addonId() {
          return addonId;
        }

        @Override
        public void init(AddonContext ctx) {
          onInit.accept(ctx);
        }

        @Override
        public void onBridgeEvent(BridgeEvent event) {
          onBridgeEvent.accept(event);
        }

        @Override
        public void shutdown() {
          onShutdown.run();
        }
      };
    }
  }
}
