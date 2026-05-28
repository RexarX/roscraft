# Roscraft — Minecraft Mod

ROS 2 <=> Minecraft bidirectional bridge.

## Build

```bash
cd mc_mod

# Full build (spotless format checks + compilation)
./gradlew build

# Build skipping format checks
./gradlew build -x spotlessJavaCheck

# Format code only
./gradlew spotlessApply

# Generate FlatBuffers Java sources only
./gradlew generateFlatBuffers
```

**Requirements:**

- Java 21
- `flatc` on PATH (schemas -> Java code generation)

## Run

1. Build the mod: `./gradlew build`
2. Copy `build/libs/roscraft-<version>.jar` into your Fabric server's `mods/` folder
3. Start the C++ bridge side (see `../roscraft_ws/`)
4. In-game (operator lvl 2+): `/ros connection connect`

## Project structure

```
mc_mod/
├── roscraft-api/              Public API for addon authors
│   └── src/main/java/net/roscraft/
│       ├── bridge/event/       BridgeEvent sealed hierarchy (22 records)
│       │   ├── BridgeEventBus.java     Global bridge event subscriptions
│       │   ├── LocalBus.java           Typed local messages
│       │   ├── AddonSignalBus.java     String-keyed inter-addon signals
│       │   └── Subscription.java       AutoCloseable unsubscribe handle
│       ├── bridge/             BridgeOperations (namespaced sub-interfaces)
│       │                         TopicOps, ParamOps, ServiceOps,
│       │                         ActionOps, GraphOps
│       └── mod/addon/          RoscraftAddon, AbstractRoscraftAddon, AddonContext
├── src/main/java/net/roscraft/
│   ├── bridge/                 Transport, BridgeRequestHub, command event router
│   └── mod/                    Commands, addon manager, mod entrypoint
│       ├── command/request/    /ros command pending-request tracking
│       └── bridge/callback/    CommandBridgeEventRouter + domain handlers
│                               (Graph/Topic/Service/Param/Action/Misc)
│                               and BridgeEventChatSupport
├── examples/template/          Addon template — copy to start your own addon
└── build.gradle                Main build config
```

## Architecture

- **BridgeOperations** are grouped into domain sub-interfaces (`topics()`, `params()`, `services()`, `actions()`, `graph()`) with options records replacing boolean-heavy overloads.
- **Auto-tracking** — every bridge operation invoked through `ctx.bridgeIfConnected()` is automatically tracked; manual `track()` calls are deprecated.
- **Three event buses**: `BridgeEventBus`, `LocalBus`, `AddonSignalBus` (incoming `AddonEvent` packets fan out to the signal bus automatically).
- **BridgeRequestHub** coordinates command vs addon request-ID ownership with conflict detection on double-registration.
- **Gradle `:roscraft-api`** subproject — published separately; main mod depends on it via `api project(':roscraft-api')`.
- **Subscription handles** (`AutoCloseable`) replace raw `long` request IDs for persistent operations; closing a handle unsubscribes and untracks.
- **`AddonContext.DISCONNECTED`** sentinel (`0L`) documents the disconnected-bridge return value.

## Addon template

```bash
cd examples/template
./gradlew build
```

The template compiles against the roscraft mod jar. Build the main mod first
to produce the jar at `../../build/libs/roscraft-<version>.jar`.

### Creating your own addon

1. Copy `examples/template/` to a new directory
2. Rename `your.mod` in both `src/main/java/` and `fabric.mod.json`
3. Set a unique `addonId()` in your addon class
4. Register in `fabric.mod.json` under `"entrypoints" -> "roscraft:addon"`
5. Build and place the JAR alongside the roscraft mod
