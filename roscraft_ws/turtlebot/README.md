# Roscraft Turtlebot Nodes

This package provides three ROS 2 nodes for a Roscraft turtlebot workflow:

- `roscraft_turtlebot_lifecycle` exposes spawn and despawn services.
- `roscraft_turtlebot_movement` translates movement commands into `Twist`
  velocity messages.
- `roscraft_turtlebot_keyboard` is an interactive CLI that turns key presses
  into movement commands and (by default) calls lifecycle spawn on start.

## Launch (recommended)

The launch file is the recommended entry point. By default it starts one turtle
instance in the `turtle1` namespace:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py
```

Movement logs from `roscraft_turtlebot_movement` appear only when commands are
received on `movement/cmd`. Publish movement commands or use the keyboard node;
an idle stack is quiet aside from the startup ready line.

`auto_spawn` defaults to `true`, which sets `spawn_on_start` on the lifecycle
node (ROS lifecycle state becomes spawned). You still need
`/ros turtlebot spawn turtle1` in Minecraft for the entity to appear.

Keyboard control from `ros2 launch ... launch_keyboard:=true` often has no
interactive TTY; the keyboard process logs a warning and exits without undoing
spawn. Run keyboard input in a **separate terminal**:

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard --ros-args -r __ns:=/turtle1
```

To start more than one turtle, pass a comma-separated list of namespaces:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py turtle_names:=turtle1,turtle2
```

To attach the keyboard CLI to a specific turtle instance:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py \
  turtle_names:=turtle1,turtle2 \
  launch_keyboard:=true \
  keyboard_namespace:=turtle2
```

Keyboard lifecycle options (when `launch_keyboard:=true`):

- `auto_spawn:=true` (default) — call spawn service when the keyboard node starts
- `auto_despawn:=true` (default) — call despawn when the keyboard node exits

If you prefer a dedicated interactive terminal for the keyboard (while launch
runs lifecycle and movement), plain `ros2 run` works — the keyboard defaults to
`turtle_namespace:=turtle1` to match the launch file:

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard
```

For another instance: `ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard --ros-args -p turtle_namespace:=turtle2`

Alternatively, remap the node namespace:

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard --ros-args -r __ns:=/turtle2
```

## Namespaces

Launch places lifecycle and movement under each turtle namespace (e.g.
`/turtle1/roscraft/turtlebot/...`). That matches in-game
`/ros turtlebot spawn turtle1`.

Running nodes with plain `ros2 run` (no `__ns`) uses the **root** namespace
(`/roscraft/turtlebot/...`). Use `/ros turtlebot spawn root` in Minecraft, or
remap all nodes:

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_lifecycle --ros-args -r __ns:=/turtle1
ros2 run roscraft_turtlebot roscraft_turtlebot_movement --ros-args -r __ns:=/turtle1
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard --ros-args -r __ns:=/turtle1
```

## Build

```bash
source /opt/ros/jazzy/setup.sh
cd roscraft_ws
colcon build --packages-select roscraft_turtlebot
source install/setup.bash
```

## Run lifecycle control (manual, root namespace)

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_lifecycle
ros2 service call /roscraft/turtlebot/lifecycle/spawn std_srvs/srv/Trigger "{}"
ros2 service call /roscraft/turtlebot/lifecycle/despawn std_srvs/srv/Trigger "{}"
```

For namespaced manual runs, add `-r __ns:=/turtle1` to each command and use
`/turtle1/roscraft/turtlebot/lifecycle/spawn` instead.

## Run movement control

```text
ros2 run roscraft_turtlebot roscraft_turtlebot_movement
ros2 topic pub /roscraft/turtlebot/movement/cmd std_msgs/msg/String "{data: 'forward'}"
ros2 topic pub /roscraft/turtlebot/movement/cmd std_msgs/msg/String "{data: 'left 0.5'}"
ros2 topic echo /roscraft/turtlebot/cmd_vel
```

## Run keyboard control

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard
```

By default the keyboard node calls the lifecycle spawn service on start and
despawn on exit. Disable with ROS parameters `auto_spawn:=false` or
`auto_despawn:=false`.

Keyboard map:

```text
w forward
s backward
a turn left
d turn right
x stop
space up
c down
h help
q quit
```
