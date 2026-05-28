package net.roscraft.mod.addon;

/**
 * Tracks which addon owns a bridge request ID so responses route correctly.
 *
 * <p>Implemented by the mod runtime; addon authors normally do not implement this.
 */
public interface RequestTracker {

  void track(String addonId, long requestId);

  void trackPersistent(String addonId, long requestId);

  void untrack(String addonId, long requestId);
}
