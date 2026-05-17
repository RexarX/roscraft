# Roscraft Addon Development Guide

## What addons can access

### Namespaced bridge API via `ctx.bridgeIfConnected()`

All operations are **automatically tracked** — responses route to `onBridgeEvent(BridgeEvent)` without manual `track()` calls.

| Namespace           | Method                                 | Returns | Response event                                 |
| ------------------- | -------------------------------------- | ------- | ---------------------------------------------- |
| `bridge.graph()`    | `.snapshot()`                          | `long`  | `GraphSnapshot`                                |
|                     | `.nodeInfo(name, hidden)`              | `long`  | `NodeInfoResponse`                             |
|                     | `.topicInfo(name)`                     | `long`  | `TopicInfoResponse`                            |
|                     | `.serviceInfo(name)`                   | `long`  | `ServiceInfoResponse`                          |
|                     | `.interfaceList(msg, srv, act)`        | `long`  | `InterfaceListResponse`                        |
|                     | `.interfaceShow(type)`                 | `long`  | `InterfaceShowResponse`                        |
| `bridge.topics()`   | `.subscribe(topic, type)`              | `long`  | `TopicPayload` (persistent)                    |
|                     | `.unsubscribe(topic)`                  | `long`  | —                                              |
|                     | `.publish(topic, type, payload)`       | `long`  | (fire and forget)                              |
|                     | `.hz(topic, type, window)`             | `long`  | `TopicHzResponse`                              |
|                     | `.bw(topic, type, window)`             | `long`  | `TopicBwResponse`                              |
|                     | `.delay(topic, type, window)`          | `long`  | `TopicDelayResponse`                           |
| `bridge.params()`   | `.list(node, opts)`                    | `long`  | `ParamListResponse`                            |
|                     | `.get(node, name, opts)`               | `long`  | `ParamGetResponse`                             |
|                     | `.set(node, name, val, timeout)`       | `long`  | `ParamSetResponse`                             |
|                     | `.describe(node, name, timeout)`       | `long`  | `ParamDescribeResponse`                        |
|                     | `.dump(node, prefixes, timeout)`       | `long`  | `ParamDumpResponse`                            |
|                     | `.load(node, yaml, opts)`              | `long`  | `ParamLoadResponse`                            |
| `bridge.services()` | `.call(name, type, payload, opts)`     | `long`  | `ServiceCallResponse`                          |
| `bridge.actions()`  | `.info(name, hidden)`                  | `long`  | `ActionInfoResponse`                           |
|                     | `.sendGoal(name, type, payload, opts)` | `long`  | `ActionFeedback` / `ActionResult` (persistent) |
| `bridge`            | `.queryPlayers()`                      | `long`  | `PlayerList`                                   |

Options records: `SubscribeOptions`, `PublishOptions`, `HzOptions`, `BwOptions`,
`ParamListOptions`, `ParamGetOptions`, `ParamLoadOptions`, `ServiceCallOptions`,
`ActionGoalOptions` — each has a `defaults()` factory and a `static Builder`.

### Persistent operations return `Subscription` handles

```java
ctx.subscribeTopic("/topic", "std_msgs/msg/String").ifPresent(sub -> {
    // sub.close() unsubscribes and untracks
});
```

### Inter-addon events

- ROS -> Minecraft: publish `roscraft_bridge_common/msg/AddonEvent` to `/roscraft/addon/event_in`.
- Minecraft -> ROS: `ctx.sendEvent(eventType, payload, response)`. Returns `AddonContext.DISCONNECTED` (0) when disconnected.

### Error handling

Errors from tracked requests are delivered as `BridgeEvent.BridgeError` to `onBridgeEvent`.

## Quickstart

1. Copy `examples/template/` to your project directory
2. Rename the package from `your.mod` to your own
3. Implement `RoscraftAddon` with a unique `addonId()`
4. Add to `fabric.mod.json`:

```json
{
    "entrypoints": {
        "roscraft:addon": ["your.mod.YourAddon"]
    },
    "depends": { "roscraft": ">=0.1.0" }
}
```

## Reference

- `mc_mod/src/main/java/net/roscraft/mod/addon/example/ExampleAddon.java` — complete working example
- `mc_mod/examples/template/` — full Fabric project template you can copy and build
