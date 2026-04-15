package net.roscraft.mod.command;

import java.util.Objects;
import java.util.UUID;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.bridge.BridgeManager;

/**
 * Context passed to command-logic methods.
 *
 * @param bridgeManager active bridge manager
 * @param requesterUuid UUID of the command sender, or null when unavailable
 */
public record CommandContext(BridgeManager bridgeManager, UUID requesterUuid) {
  /** Canonical constructor — null-check for required fields. */
  public CommandContext {
    Objects.requireNonNull(bridgeManager, "bridgeManager must not be null");
  }

  /** Returns the connected bridge or throws if disconnected. */
  public RoscraftBridge requireBridge() {
    return bridgeManager.requireConnectedBridge();
  }
}
