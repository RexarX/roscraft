package net.roscraft.mod.addon;

import net.roscraft.bridge.BridgeOperations;

/**
 * Decorates {@link BridgeOperations} so every request ID is registered with the
 * owning addon. One-shot operations use {@link RequestTracker#track}; persistent
 * operations (subscribe, sendGoal) use {@link RequestTracker#trackPersistent}.
 */
final class TrackedBridge
    implements BridgeOperations,
        BridgeOperations.TopicOps,
        BridgeOperations.ParamOps,
        BridgeOperations.ServiceOps,
        BridgeOperations.ActionOps,
        BridgeOperations.GraphOps {

  private final BridgeOperations delegate;
  private final String addonId;
  private final RequestTracker router;

  TrackedBridge(BridgeOperations delegate, String addonId, RequestTracker router) {
    this.delegate = delegate;
    this.addonId = addonId;
    this.router = router;
  }

  @Override
  public TopicOps topics() {
    return this;
  }

  @Override
  public ParamOps params() {
    return this;
  }

  @Override
  public ServiceOps services() {
    return this;
  }

  @Override
  public ActionOps actions() {
    return this;
  }

  @Override
  public GraphOps graph() {
    return this;
  }

  @Override
  public long queryPlayers() {
    return track(delegate.queryPlayers());
  }

  @Override
  public long sendRawPacket(byte[] payload) {
    return track(delegate.sendRawPacket(payload));
  }

  @Override
  public long snapshot() {
    return track(delegate.graph().snapshot());
  }

  @Override
  public long nodeInfo(String n, boolean h) {
    return track(delegate.graph().nodeInfo(n, h));
  }

  @Override
  public long topicInfo(String t) {
    return track(delegate.graph().topicInfo(t));
  }

  @Override
  public long serviceInfo(String s) {
    return track(delegate.graph().serviceInfo(s));
  }

  @Override
  public long interfaceList(boolean m, boolean s, boolean a) {
    return track(delegate.graph().interfaceList(m, s, a));
  }

  @Override
  public long interfaceShow(String t) {
    return track(delegate.graph().interfaceShow(t));
  }

  @Override
  public long subscribe(String t, String ty) {
    return trackPersistent(delegate.topics().subscribe(t, ty));
  }

  @Override
  public long subscribe(String t, String ty, TopicOps.SubscribeOptions o) {
    return trackPersistent(delegate.topics().subscribe(t, ty, o));
  }

  @Override
  public long unsubscribe(String t) {
    return track(delegate.topics().unsubscribe(t));
  }

  @Override
  public long publish(String t, String ty, byte[] p) {
    return track(delegate.topics().publish(t, ty, p));
  }

  @Override
  public long publish(String t, String ty, byte[] p, TopicOps.PublishOptions o) {
    return track(delegate.topics().publish(t, ty, p, o));
  }

  @Override
  public long hz(String t, String ty, int w) {
    return track(delegate.topics().hz(t, ty, w));
  }

  @Override
  public long hz(String t, String ty, int w, TopicOps.HzOptions o) {
    return track(delegate.topics().hz(t, ty, w, o));
  }

  @Override
  public long bw(String t, String ty, int w) {
    return track(delegate.topics().bw(t, ty, w));
  }

  @Override
  public long bw(String t, String ty, int w, TopicOps.BwOptions o) {
    return track(delegate.topics().bw(t, ty, w, o));
  }

  @Override
  public long delay(String t, String ty, int w) {
    return track(delegate.topics().delay(t, ty, w));
  }

  @Override
  public long list(String n, ParamOps.ParamListOptions o) {
    return track(delegate.params().list(n, o));
  }

  @Override
  public long get(String n, String p, ParamOps.ParamGetOptions o) {
    return track(delegate.params().get(n, p, o));
  }

  @Override
  public long set(String n, String p, String v, double t) {
    return track(delegate.params().set(n, p, v, t));
  }

  @Override
  public long describe(String n, String p, double t) {
    return track(delegate.params().describe(n, p, t));
  }

  @Override
  public long dump(String n, String[] pr, double t) {
    return track(delegate.params().dump(n, pr, t));
  }

  @Override
  public long load(String n, String y, ParamOps.ParamLoadOptions o) {
    return track(delegate.params().load(n, y, o));
  }

  @Override
  public long call(String n, String t, byte[] p, ServiceOps.ServiceCallOptions o) {
    return track(delegate.services().call(n, t, p, o));
  }

  @Override
  public long info(String n, boolean h) {
    return track(delegate.actions().info(n, h));
  }

  @Override
  public long sendGoal(String n, String t, byte[] p, ActionOps.ActionGoalOptions o) {
    return trackPersistent(delegate.actions().sendGoal(n, t, p, o));
  }

  private long track(long requestId) {
    router.track(addonId, requestId);
    return requestId;
  }

  private long trackPersistent(long requestId) {
    router.trackPersistent(addonId, requestId);
    return requestId;
  }
}
