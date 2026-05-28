package net.roscraft.mod.addon;

import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import net.roscraft.mod.RoscraftMod;

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
public final class RequestRouter implements RequestTracker {

  private final ConcurrentHashMap<Long, String> oneShotOwners = new ConcurrentHashMap<>();
  private final ConcurrentHashMap<Long, String> persistentOwners = new ConcurrentHashMap<>();
  private final java.util.function.LongPredicate commandOwnerCheck;

  public RequestRouter(java.util.function.LongPredicate commandOwnerCheck) {
    this.commandOwnerCheck =
        Objects.requireNonNull(commandOwnerCheck, "commandOwnerCheck must not be null");
  }

  @Override
  public void track(String addonId, long requestId) {
    if (rejectIfCommandOwned(requestId, addonId)) {
      return;
    }
    oneShotOwners.put(requestId, addonId);
  }

  @Override
  public void untrack(String addonId, long requestId) {
    oneShotOwners.remove(requestId, addonId);
    persistentOwners.remove(requestId, addonId);
  }

  @Override
  public void trackPersistent(String addonId, long requestId) {
    if (rejectIfCommandOwned(requestId, addonId)) {
      return;
    }
    persistentOwners.put(requestId, addonId);
  }

  private boolean rejectIfCommandOwned(long requestId, String addonId) {
    if (commandOwnerCheck.test(requestId)) {
      RoscraftMod.LOGGER.warn(
          "Request #{} is already tracked by a /ros command; refusing addon track for {}",
          requestId,
          addonId);
      return true;
    }
    return false;
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

  public boolean isTracked(long requestId) {
    return oneShotOwners.containsKey(requestId) || persistentOwners.containsKey(requestId);
  }

  int size() {
    return oneShotOwners.size() + persistentOwners.size();
  }
}
