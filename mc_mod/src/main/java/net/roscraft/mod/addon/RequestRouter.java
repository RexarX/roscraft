package net.roscraft.mod.addon;

import java.util.concurrent.ConcurrentHashMap;

/**
 * Thread-safe router mapping request IDs to addon IDs.
 *
 * <p>Supports two tracking modes:
 * <ul>
 * <li><b>One-shot</b> — request ID is consumed (removed) on first use.
 *     Used for query/command responses that deliver exactly one event.</li>
 * <li><b>Persistent</b> — request ID stays mapped until explicitly untracked.
 *     Used for topic subscriptions, action goals, etc. that may deliver
 *     multiple events.</li>
 * </ul>
 */
final class RequestRouter implements AddonContext.RequestTracker {

  private final ConcurrentHashMap<Long, String> oneShotOwners = new ConcurrentHashMap<>();
  private final ConcurrentHashMap<Long, String> persistentOwners = new ConcurrentHashMap<>();

  @Override
  public void track(String addonId, long requestId) {
    oneShotOwners.put(requestId, addonId);
  }

  @Override
  public void untrack(String addonId, long requestId) {
    oneShotOwners.remove(requestId, addonId);
    persistentOwners.remove(requestId, addonId);
  }

  @Override
  public void trackPersistent(String addonId, long requestId) {
    persistentOwners.put(requestId, addonId);
  }

  /** @return the addon ID, or null if no owner */
  String consume(long requestId) {
    String owner = oneShotOwners.remove(requestId);
    return owner != null ? owner : persistentOwners.get(requestId);
  }

  void clear() {
    oneShotOwners.clear();
    persistentOwners.clear();
  }

  int size() {
    return oneShotOwners.size() + persistentOwners.size();
  }
}
