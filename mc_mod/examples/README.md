# Roscraft Addon Development Guide

## What addons can access

### Full `RoscraftBridge` API via `ctx.bridge()`

All 20+ bridge methods. After calling, track with `ctx.trackRequest(requestId)`.

| Method                                         | Response callback                                               |
| ---------------------------------------------- | --------------------------------------------------------------- |
| `queryGraph()`                                 | `onGraphSnapshot(GraphSnapshot)`                                |
| `nodeInfo(name, hidden)`                       | `onNodeInfoResponse(NodeInfoResponse)`                          |
| `topicInfo(name)`                              | `onTopicInfoResponse(TopicInfoResponse)`                        |
| `serviceInfo(name)`                            | `onServiceInfoResponse(ServiceInfoResponse)`                    |
| `subscribeTopic(name, type)`                   | `onTopicPayload(TopicPayload)`                                  |
| `publishMessage(name, type, payload)`          | (fire and forget)                                               |
| `serviceCall(name, type, payload, ...)`        | `onServiceCallResponse(ServiceCallResponse)`                    |
| `queryPlayers()`                               | `onPlayerList(PlayerList)`                                      |
| `sendAddonEvent(id, type, enc, payload, resp)` | `onAddonEvent(AddonEvent)`                                      |
| `interfaceList(...)`                           | `onInterfaceListResponse(InterfaceListResponse)`                |
| `interfaceShow(type)`                          | `onInterfaceShowResponse(InterfaceShowResponse)`                |
| `paramList(node, ...)`                         | `onParamListResponse(ParamListResponse)`                        |
| `paramGet(node, name, ...)`                    | `onParamGetResponse(ParamGetResponse)`                          |
| `paramSet(node, name, val, ...)`               | `onParamSetResponse(ParamSetResponse)`                          |
| `paramDescribe(node, name, ...)`               | `onParamDescribeResponse(ParamDescribeResponse)`                |
| `paramDump(node, ...)`                         | `onParamDumpResponse(ParamDumpResponse)`                        |
| `paramLoad(node, yaml, ...)`                   | `onParamLoadResponse(ParamLoadResponse)`                        |
| `actionInfo(name, ...)`                        | `onActionInfoResponse(ActionInfoResponse)`                      |
| `actionSendGoal(name, type, payload, ...)`     | `onActionFeedback/onActionResult(...)`                          |
| `topicHz/Bw/Delay(...)`                        | `onTopicHzResponse/onTopicBwResponse/onTopicDelayResponse(...)` |

All 24 response callbacks have default no-op implementations — override only what you need.

### Request tracking

```java
long rid = ctx.bridge().queryGraph();
ctx.trackRequest(rid);      // onGraphSnapshot will be called

long rid = ctx.bridge().serviceCall("/add", "example_interfaces/srv/AddTwoInts", req, 5, 1, 0);
ctx.trackRequest(rid);      // onServiceCallResponse will be called
```

### Command registration

```java
@Override
public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return List.of(CommandManager.literal("myaddon")
        .executes(ctx -> { /* ... */ return 1; }));
}
```

Commands appear under `/ros myaddon`.

### Addon events (bidirectional)

ROS → Minecraft: publish `roscraft_bridge_common/msg/AddonEvent` to `/roscraft/addon/event_in`.

Minecraft → ROS: `ctx.eventSender().sendAddonEvent(eventType, payload, response)`.

### Error handling

Errors from tracked requests are delivered to `onBridgeError(BridgeError)`.

## Quickstart

1. Create a class implementing `RoscraftAddon`
2. Add to `fabric.mod.json`:

```json
{
    "entrypoints": {
        "roscraft:addon": ["your.mod.YourAddon"]
    },
    "depends": {
        "roscraft": ">=0.1.0"
    }
}
```

## Reference

- `mc_mod/src/main/java/net/roscraft/mod/addon/example/ExampleAddon.java` — complete working example
- `mc_mod/examples/template/` — full Fabric project template you can copy and build
