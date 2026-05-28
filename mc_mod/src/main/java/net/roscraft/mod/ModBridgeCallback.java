package net.roscraft.mod;

import net.roscraft.bridge.BridgeCallback;
import net.roscraft.bridge.event.BridgeEvent;

/** Routes bridge events to addons and {@code /ros} command chat handlers. */
final class ModBridgeCallback implements BridgeCallback {

  private final RoscraftMod mod;

  ModBridgeCallback(RoscraftMod mod) {
    this.mod = mod;
  }

  @Override
  public void onEvent(BridgeEvent event) {
    mod.addonManager().onEvent(event);
    mod.commandEvents().onEvent(event);
  }
}
