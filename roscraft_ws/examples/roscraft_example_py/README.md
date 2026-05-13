# Roscraft Python Bridge Demo

This package provides a small ROS 2 node that can be used to validate the
Roscraft bridge topic and service flow.

## Build

```bash
source /opt/ros/jazzy/setup.sh
cd roscraft_ws
colcon build --packages-select roscraft_example_py
source install/setup.bash
```

## Run

```bash
ros2 run roscraft_example_py roscraft_bridge_demo
```

## Minecraft commands

```text
/ros topic subscribe /roscraft/example/out std_msgs/msg/String
/ros topic pub /roscraft/example/in std_msgs/msg/String "data: 'hello from MC'"
/ros service call /roscraft/example/ping std_srvs/srv/Trigger "{}"
```

## Addon Event Demo

This demo exercises the addon event channel (ROS <-> mod addons).

```bash
ros2 run roscraft_example_py roscraft_addon_event_demo
```

It sends a `ping` event every 5 seconds to the mod's built-in `ping`
addon, and logs the `pong` response.

Requires the roscraft bridge to be running with the addon event node
configured.
