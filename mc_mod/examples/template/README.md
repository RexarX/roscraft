# Roscraft Addon Template

Copy this directory to start your own roscraft addon.

## Setup

1. Copy `template/` to a new directory (e.g. `my_roscraft_addon/`)
2. Rename the Java package from `your.mod` to your own (e.g. `com.example.myaddon`)
3. Edit `src/main/resources/fabric.mod.json` — update `id`, `name`, `entrypoints`
4. Edit `gradle.properties` — update `roscraft_jar_path` to point to the roscraft mod JAR
5. Run `./gradlew build`

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

## After setup

- Override callbacks you need: `onGraphSnapshot`, `onTopicPayload`, `onPlayerList`, `onServiceCallResponse`, etc.
- Use `ctx.bridge()` for ROS operations, `ctx.trackRequest(rid)` for responses.
- Use `ctx.eventSender().sendAddonEvent(...)` for bidirectional events.

See `../README.md` for the full API reference.
