package net.roscraft.mod.addon;

import java.nio.charset.StandardCharsets;
import net.roscraft.bridge.event.BridgeEvent;
import net.roscraft.mod.RoscraftMod;

final class PingAddon extends AbstractRoscraftAddon {

  @Override
  public String addonId() {
    return "ping";
  }

  @Override
  protected void configure() {
    on(BridgeEvent.AddonEvent.class, ae -> {
      if (!"ping".equals(ae.eventType())) {
        return;
      }
      var responsePayload = String.format(
              "pong from roscraft mod (echo: %s)", new String(ae.payload(), StandardCharsets.UTF_8))
          .getBytes(StandardCharsets.UTF_8);
      ctx.sendEvent("pong", responsePayload, true);
      RoscraftMod.LOGGER.debug("PingAddon responded to request #{}", ae.requestId());
    });
  }
}
