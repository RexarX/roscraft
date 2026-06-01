# Roscraft Addon Template

Copy this directory to start your own roscraft addon.

## Setup

1. Copy `template/` to a new directory (e.g. `my_roscraft_addon/`)
2. Rename the Java package from `your.mod` to your own (e.g. `com.example.myaddon`)
3. Edit `src/main/resources/fabric.mod.json` — update `id`, `name`, `entrypoints`
4. Edit `gradle.properties` — update `roscraft_jar_path` to point to the roscraft mod JAR
5. Run `./gradlew build`

```bash
./gradlew spotlessApply   # format Java sources (Google style via Spotless)
```

```
template/
├── build.gradle          # Fabric Loom build with roscraft dependency
├── settings.gradle       # Plugin repositories
├── gradle.properties     # Versions and roscraft_jar_path
├── src/
│   ├── main/
│   │   ├── java/
│   │   │   └── your/mod/TemplateAddon.java
│   │   └── resources/
│   │       └── fabric.mod.json
│   └── client/          # (unused — add client-side code here)
```

## Addon API quickstart

### Bridge operations (auto-tracked)

All operations invoked through `ctx.bridgeIfConnected()` are automatically tracked.
No need to call `ctx.track()` — responses route to `onBridgeEvent(BridgeEvent)`.

```java
// One-shot — response arrives once
ctx.bridgeIfConnected().ifPresent(bridge -> {
    bridge.graph().snapshot();               // ROS graph query
    bridge.topics().subscribe("/foo", "");    // topic subscribe (persistent)
    bridge.params().get("/node", "/param",
        new BridgeOperations.ParamOps.ParamGetOptions(false, 5.0));
    bridge.services().call("/service", "std_srvs/srv/Empty", requestBytes,
        BridgeOperations.ServiceOps.ServiceCallOptions.defaults());
    bridge.queryPlayers();
});
```

For topic subscriptions and action goals, use convenience methods that return
a `Subscription` handle:

```java
ctx.subscribeTopic("/topic", "std_msgs/msg/String").ifPresent(sub -> {
    sub.close();  // unsubscribes + untracks
});
```

### Inter-addon events

```java
ctx.sendEvent("my_event", payloadBytes, false);
// Returns ctx.DISCONNECTED (0) if bridge is disconnected
```

### Event buses

```java
// Global bridge events (fires for all addons)
ctx.bridgeBus().onAny(BridgeEvent.TopicPayload.class, this::handlePayload);

// Typed local messages (shared classpath)
ctx.localBus().on(MyMessage.class, this::handleLocal);
ctx.localBus().emit(new MyMessage("hello"));

// String-keyed signals (loosely coupled)
ctx.signalBus().on("ping", this::handleSignal);
```

`Subscription.on()` / `Subscription.onAny()` returns a closing handle.
All three buses are available from `AddonContext` after `init()`.

### Commands

Implement `RoscraftAddonCommands` and return Brigadier literal nodes from `commands()` — they appear under `/ros`:

```java
public class MyAddon extends AbstractRoscraftAddon implements RoscraftAddonCommands {
@Override
public List<LiteralArgumentBuilder<ServerCommandSource>> commands() {
    return List.of(
        CommandManager.literal("mycmd")
            .executes(ctx -> { ...; return 1; })
    );
}
```

See the full API reference at `../README.md`.
