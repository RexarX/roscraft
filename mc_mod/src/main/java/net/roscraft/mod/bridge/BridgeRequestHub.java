package net.roscraft.mod.bridge;

import net.roscraft.mod.addon.RequestRouter;
import net.roscraft.mod.command.request.CommandRequestTracker;

/**
 * Central registry for bridge request ownership ({@code /ros} commands vs addons).
 *
 * <p>Command and addon trackers are separate maps keyed by the same global request-ID
 * space from {@link net.roscraft.bridge.RoscraftBridge}. Trackers refuse
 * double-registration when the same ID is already owned by the other side.
 */
public final class BridgeRequestHub {

  private final CommandRequestTracker commands;
  private final RequestRouter addons;

  public BridgeRequestHub(CommandRequestTracker commands) {
    this.commands = commands;
    this.addons = new RequestRouter(commands::has);
  }

  public CommandRequestTracker commands() {
    return commands;
  }

  public RequestRouter addons() {
    return addons;
  }
}
