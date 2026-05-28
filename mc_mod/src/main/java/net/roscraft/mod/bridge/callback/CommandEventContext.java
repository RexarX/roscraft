package net.roscraft.mod.bridge.callback;

import net.roscraft.mod.RoscraftMod;
import net.roscraft.mod.command.request.CommandRequestTracker;

final class CommandEventContext {

  private final RoscraftMod mod;
  private final CommandRequestTracker requests;
  private final BridgeEventChatSupport chat;

  CommandEventContext(RoscraftMod mod, CommandRequestTracker requests) {
    this.mod = mod;
    this.requests = requests;
    this.chat = new BridgeEventChatSupport(mod);
  }

  RoscraftMod mod() {
    return mod;
  }

  CommandRequestTracker requests() {
    return requests;
  }

  BridgeEventChatSupport chat() {
    return chat;
  }
}
