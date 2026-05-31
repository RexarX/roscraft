# Roscraft Turtlebot Nodes

This package provides three ROS 2 nodes for a Roscraft turtlebot workflow:

* `roscraft_turtlebot_lifecycle` exposes spawn and despawn services.
* `roscraft_turtlebot_movement` translates movement commands into `Twist`
  velocity messages.
* `roscraft_turtlebot_keyboard` is an interactive CLI that turns key presses
  into movement commands.

## Launch

The recommended entry point is the launch file. By default it starts one turtle
instance in the `turtle1` namespace:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py
```

To start more than one turtle, pass a comma-separated list of namespaces:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py turtle_names:=turtle1,turtle2
```

To attach the keyboard CLI to a specific turtle instance in the same launch
session, enable the keyboard and pick a namespace:

```bash
ros2 launch roscraft_turtlebot turtlebot.launch.py \
  turtle_names:=turtle1,turtle2 \
  launch_keyboard:=true \
  keyboard_namespace:=turtle2
```

If you prefer to keep the keyboard controller in a dedicated interactive
terminal, run it with the same namespace as the turtle you want to control:

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard --ros-args -r __ns:=/turtle2
```

## Build

```bash
source /opt/ros/jazzy/setup.sh
cd roscraft_ws
colcon build --packages-select roscraft_turtlebot
source install/setup.bash
```

## Run lifecycle control

```bash
ros2 run roscraft_turtlebot roscraft_turtlebot_lifecycle
ros2 service call /roscraft/turtlebot/lifecycle/spawn std_srvs/srv/Trigger "{}"
ros2 service call /roscraft/turtlebot/lifecycle/despawn std_srvs/srv/Trigger "{}"
```

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

The movement node publishes the resulting velocity command as a standard
`geometry_msgs/msg/Twist` message, which makes it easy to connect the package
to a downstream bridge or simulator later.
