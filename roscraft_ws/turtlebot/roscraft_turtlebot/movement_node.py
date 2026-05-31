from __future__ import annotations

from dataclasses import dataclass
from typing import Final

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node
from std_msgs.msg import String

DEFAULT_INPUT_TOPIC: Final = "roscraft/turtlebot/movement/cmd"
DEFAULT_OUTPUT_TOPIC: Final = "roscraft/turtlebot/cmd_vel"
DEFAULT_STATE_TOPIC: Final = "roscraft/turtlebot/movement/state"
DEFAULT_LINEAR_SPEED: Final = 0.25
DEFAULT_ANGULAR_SPEED: Final = 1.0


@dataclass(slots=True)
class ParsedCommand:
    action: str
    scale: float = 1.0


def _parse_command(raw_command: str) -> ParsedCommand | None:
    command = raw_command.strip().lower()
    if not command:
        return None

    parts = command.split(maxsplit=1)
    action = parts[0]
    if action not in {"backward", "down", "forward", "left", "right", "stop", "up"}:
        return None

    if len(parts) == 1:
        return ParsedCommand(action=action)

    try:
        return ParsedCommand(action=action, scale=float(parts[1]))
    except ValueError:
        return None


class MovementNode(Node):
    def __init__(self) -> None:
        super().__init__("roscraft_turtlebot_movement")

        input_topic = self.declare_parameter(
            "input_topic",
            DEFAULT_INPUT_TOPIC,
        ).value
        output_topic = self.declare_parameter(
            "output_topic",
            DEFAULT_OUTPUT_TOPIC,
        ).value
        state_topic = self.declare_parameter(
            "state_topic",
            DEFAULT_STATE_TOPIC,
        ).value
        self._linear_speed = float(
            self.declare_parameter("linear_speed", DEFAULT_LINEAR_SPEED).value
        )
        self._angular_speed = float(
            self.declare_parameter("angular_speed", DEFAULT_ANGULAR_SPEED).value
        )

        self._command_subscription = self.create_subscription(
            String,
            input_topic,
            self._on_command,
            10,
        )
        self._velocity_publisher = self.create_publisher(Twist, output_topic, 10)
        self._state_publisher = self.create_publisher(String, state_topic, 10)
        self.get_logger().info(
            f"Movement controller ready on {input_topic} -> {output_topic}"
        )

    def _make_twist(self, command: ParsedCommand) -> Twist:
        twist = Twist()
        magnitude = command.scale

        if command.action == "forward":
            twist.linear.x = self._linear_speed * magnitude
        elif command.action == "backward":
            twist.linear.x = -self._linear_speed * magnitude
        elif command.action == "left":
            twist.angular.z = self._angular_speed * magnitude
        elif command.action == "right":
            twist.angular.z = -self._angular_speed * magnitude
        elif command.action == "up":
            twist.linear.z = self._linear_speed * magnitude
        elif command.action == "down":
            twist.linear.z = -self._linear_speed * magnitude

        return twist

    def _publish_state(self, command: ParsedCommand, twist: Twist) -> None:
        state = String()
        state.data = (
            f"action={command.action} scale={command.scale:.3f} "
            f"linear_x={twist.linear.x:.3f} linear_z={twist.linear.z:.3f} "
            f"angular_z={twist.angular.z:.3f}"
        )
        self._state_publisher.publish(state)
        self.get_logger().info(state.data)

    def _on_command(self, msg: String) -> None:
        command = _parse_command(msg.data)
        if command is None:
            self.get_logger().warning(
                f"Ignoring invalid movement command: {msg.data!r}"
            )
            return

        twist = self._make_twist(command)
        self._velocity_publisher.publish(twist)
        self._publish_state(command, twist)


def main() -> None:
    rclpy.init()
    node = MovementNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
