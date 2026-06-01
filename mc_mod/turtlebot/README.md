# Roscraft Turtlebot Addon

Minecraft-side companion for the `roscraft_turtlebot` ROS 2 package. Spawns real
`turtle` entities, tracks namespaced turtlebot topics, and drives movement from
`cmd_vel` messages.

## Build

Build the main roscraft mod first, then this addon:

```bash
cd mc_mod
./gradlew build -PforceNetworkOnly=true

cd turtlebot
./gradlew build

# Format Java sources
./gradlew spotlessApply
```

Deploy both JARs to your Fabric server `mods/` folder:

- `mc_mod/build/libs/roscraft-1.21.1.jar`
- `mc_mod/turtlebot/build/libs/roscraft-turtlebot-0.1.0.jar`

Ensure `gradle.properties` `roscraft_jar_path` and `roscraft_api_jar_path` point at
the built main-mod artifacts.

## ROS stack

```bash
source /opt/ros/jazzy/setup.sh
cd roscraft_ws
make build-relwithdebinfo CMAKE_ARGS="-GNinja -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
colcon build --packages-select roscraft_turtlebot
source install/setup.bash
ros2 launch roscraft_turtlebot turtlebot.launch.py
```

In-game (operator): `/ros connection connect`, then `/ros turtlebot refresh`.

## Topic mapping

Per namespace (e.g. `turtle1`):

| Topic                                | Type                      | Addon behavior                                      |
| ------------------------------------ | ------------------------- | --------------------------------------------------- |
| `roscraft/turtlebot/lifecycle/state` | `std_msgs/msg/Bool`       | Spawn/despawn entity on `false→true` / `true→false` |
| `roscraft/turtlebot/cmd_vel`         | `geometry_msgs/msg/Twist` | Move entity (one step per message)                  |
| `roscraft/turtlebot/movement/state`  | `std_msgs/msg/String`     | Status / chat telemetry                             |

## Commands

Under `/ros`:

| Command                                   | Description                                        |
| ----------------------------------------- | -------------------------------------------------- |
| `turtlebot` / `turtlebot status`          | List tracked turtles and poses                     |
| `turtlebot refresh`                       | Graph snapshot and auto-discover namespaces        |
| `turtlebot watch <namespace>`             | Manually track a namespace                         |
| `turtlebot forget <namespace>`            | Stop tracking and despawn entity                   |
| `turtlebot spawn <namespace>`             | Spawn at world spawn (on ground) via ROS lifecycle |
| `turtlebot spawn <namespace> at <player>` | Spawn at player position via ROS lifecycle         |
| `turtlebot despawn <namespace>`           | Despawn via ROS lifecycle                          |

## Spawn locations

- **Default:** overworld world spawn (X/Z), Y from heightmap so the turtle sits on the ground. Namespaces are offset along X (`turtle1`, `turtle2`, …).
- **At player:** use `spawn <namespace> at <player>` so the entity appears at that player's block position.

## Test flow

1. Start Fabric server with both mods; connect bridge.
2. Launch ROS turtlebot (single or multi-namespace).
3. `ros2 service call /turtle1/roscraft/turtlebot/lifecycle/spawn std_srvs/srv/Trigger "{}"`
4. Publish movement: `ros2 topic pub /turtle1/roscraft/turtlebot/movement/cmd std_msgs/msg/String "{data: 'forward'}"`
5. Entity should move; `stop` sends zero `cmd_vel`.
6. `ros2 service call /turtle1/roscraft/turtlebot/lifecycle/despawn std_srvs/srv/Trigger "{}"`
