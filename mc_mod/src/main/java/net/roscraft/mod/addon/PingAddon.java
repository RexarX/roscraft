package net.roscraft.mod.addon;

import java.nio.charset.StandardCharsets;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;

final class PingAddon implements RoscraftAddon {

  private AddonContext ctx;

  @Override
  public String addonId() {
    return "ping";
  }

  @Override
  public void init(AddonContext ctx) {
    this.ctx = ctx;
  }

  @Override
  public void onBridgeEvent(BridgeEvent event) {
    if (event instanceof BridgeEvent.AddonEvent ae && "ping".equals(ae.eventType())) {
      var responsePayload = String.format(
              "pong from roscraft mod (echo: %s)", new String(ae.payload(), StandardCharsets.UTF_8))
          .getBytes(StandardCharsets.UTF_8);
      ctx.sendEvent("pong", responsePayload, true);
      RoscraftMod.LOGGER.debug("PingAddon responded to request #{}", ae.requestId());
    }
  }
}
