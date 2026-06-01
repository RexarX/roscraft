from __future__ import annotations

from typing import Final

import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import Bool
from std_srvs.srv import Trigger

LIFECYCLE_STATE_QOS = QoSProfile(
    depth=10,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    reliability=ReliabilityPolicy.RELIABLE,
)

DEFAULT_SPAWN_SERVICE: Final = "roscraft/turtlebot/lifecycle/spawn"
DEFAULT_DESPAWN_SERVICE: Final = "roscraft/turtlebot/lifecycle/despawn"
DEFAULT_STATE_TOPIC: Final = "roscraft/turtlebot/lifecycle/state"


def _is_truthy(raw_value: object) -> bool:
    if isinstance(raw_value, bool):
        return raw_value
    return str(raw_value).strip().lower() in {"1", "true", "yes", "on"}


class LifecycleNode(Node):
    def __init__(self) -> None:
        super().__init__("roscraft_turtlebot_lifecycle")

        spawn_service = self.declare_parameter(
            "spawn_service",
            DEFAULT_SPAWN_SERVICE,
        ).value
        despawn_service = self.declare_parameter(
            "despawn_service",
            DEFAULT_DESPAWN_SERVICE,
        ).value
        state_topic = self.declare_parameter(
            "state_topic",
            DEFAULT_STATE_TOPIC,
        ).value

        self._is_spawned = False
        self._state_publisher = self.create_publisher(
            Bool,
            state_topic,
            LIFECYCLE_STATE_QOS,
        )
        self._spawn_service = self.create_service(
            Trigger,
            spawn_service,
            self._on_spawn,
        )
        self._despawn_service = self.create_service(
            Trigger,
            despawn_service,
            self._on_despawn,
        )
        self._publish_state()
        self.get_logger().info(
            f"Lifecycle controller ready on {spawn_service} and {despawn_service}"
        )

        if _is_truthy(self.declare_parameter("spawn_on_start", False).value):
            self._set_spawned(True, Trigger.Response())

        republish_hz = float(
            self.declare_parameter("state_republish_hz", 1.0).value
        )
        if republish_hz > 0.0:
            self.create_timer(1.0 / republish_hz, self._republish_state_if_spawned)

    def _republish_state_if_spawned(self) -> None:
        if self._is_spawned:
            self._publish_state()

    def _publish_state(self) -> None:
        state = Bool()
        state.data = self._is_spawned
        self._state_publisher.publish(state)

    def _set_spawned(
        self, spawned: bool, response: Trigger.Response
    ) -> Trigger.Response:
        if self._is_spawned == spawned:
            response.success = True
            response.message = (
                "Turtle is already spawned"
                if spawned
                else "Turtle is already despawned"
            )
            self._publish_state()
            return response

        self._is_spawned = spawned
        self._publish_state()
        response.success = True
        response.message = "Turtle spawned" if spawned else "Turtle despawned"
        self.get_logger().info(response.message)
        return response

    def _on_spawn(
        self,
        request: Trigger.Request,
        response: Trigger.Response,
    ) -> Trigger.Response:
        del request
        return self._set_spawned(True, response)

    def _on_despawn(
        self,
        request: Trigger.Request,
        response: Trigger.Response,
    ) -> Trigger.Response:
        del request
        return self._set_spawned(False, response)


def main() -> None:
    rclpy.init()
    node = LifecycleNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
