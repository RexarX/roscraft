# Roscraft Turtlebot Addon

This addon is the Minecraft-side companion for the `roscraft_turtlebot` ROS 2 package.
It currently does not spawn turtles yet. Instead, it tracks turtle namespaces,
subscribes to the turtlebot command/state topics, and mirrors updates to chat while
keeping a local pose model for each turtle.

## Current topic mapping

The addon watches these namespaced topics:

* `roscraft/turtlebot/movement/cmd` as `std_msgs/msg/String`
* `roscraft/turtlebot/movement/state` as `std_msgs/msg/String`
* `roscraft/turtlebot/lifecycle/state` as `std_msgs/msg/Bool`

When the ROS launch file pushes a namespace such as `turtle1`, the addon tracks the
fully qualified topics under that namespace automatically after a graph refresh.

## Commands

These commands appear under `/ros`:

* `/ros turtlebot` or `/ros turtlebot status` shows tracked turtles and their local state.
* `/ros turtlebot refresh` asks the bridge for a fresh graph snapshot and auto-discovers turtles.
* `/ros turtlebot watch <namespace>` starts tracking one namespace manually.
* `/ros turtlebot forget <namespace>` stops tracking a namespace.

## Notes

* Movement commands are applied locally to a tracked pose: `x`, `y`, `z`, `yaw`, `pitch`, `roll`.
* `left` and `right` rotate yaw; `forward` and `backward` move in the current facing direction.
* `spawn` / `despawn` state is taken from the lifecycle topic and gates movement updates.

The addon is intentionally minimal so the spawn logic can be added later without changing the ROS-facing command surface.
