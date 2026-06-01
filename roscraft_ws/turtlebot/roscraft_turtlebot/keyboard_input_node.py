from __future__ import annotations

import os
import select
import sys
import time
from contextlib import contextmanager
from typing import Final, Iterator

import rclpy
from rclpy.client import Client
from rclpy.node import Node
from std_msgs.msg import String
from std_srvs.srv import Trigger

DEFAULT_OUTPUT_TOPIC: Final = "roscraft/turtlebot/movement/cmd"
DEFAULT_SPAWN_SERVICE: Final = "roscraft/turtlebot/lifecycle/spawn"
DEFAULT_DESPAWN_SERVICE: Final = "roscraft/turtlebot/lifecycle/despawn"
HELP_TEXT: Final = (
    "Roscraft Turtlebot keyboard controls:\n"
    "  w forward   s backward   a turn left   d turn right\n"
    "  x stop      space up     c down       q quit        h help\n"
)
KEY_COMMANDS: Final[dict[str, str]] = {
    "w": "forward",
    "s": "backward",
    "a": "left",
    "d": "right",
    "x": "stop",
    " ": "up",
    "c": "down",
}


def _is_truthy(raw_value: object) -> bool:
    if isinstance(raw_value, bool):
        return raw_value
    return str(raw_value).strip().lower() in {"1", "true", "yes", "on"}


def _normalize_namespace(namespace: object) -> str:
    normalized = str(namespace).strip().strip("/")
    if normalized.lower() in {"root", "_"}:
        return ""
    return normalized


def _resolve_namespaced(name: str, namespace: str) -> str:
    trimmed = name.strip()
    if trimmed.startswith("/"):
        return trimmed
    relative = trimmed.lstrip("/")
    if not namespace:
        return f"/{relative}"
    return f"/{namespace}/{relative}"


@contextmanager
def _unix_raw_input() -> Iterator[None]:
    import termios
    import tty

    fd = sys.stdin.fileno()
    original_settings = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        yield
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, original_settings)


class KeyboardInputNode(Node):
    def __init__(self) -> None:
        super().__init__("roscraft_turtlebot_keyboard_input")
        turtle_namespace = _normalize_namespace(
            self.declare_parameter("turtle_namespace", "turtle1").value
        )
        output_topic = self.declare_parameter(
            "output_topic",
            DEFAULT_OUTPUT_TOPIC,
        ).value
        spawn_service = self.declare_parameter(
            "spawn_service",
            DEFAULT_SPAWN_SERVICE,
        ).value
        despawn_service = self.declare_parameter(
            "despawn_service",
            DEFAULT_DESPAWN_SERVICE,
        ).value
        self._auto_spawn = _is_truthy(
            self.declare_parameter("auto_spawn", True).value
        )
        self._auto_despawn = _is_truthy(
            self.declare_parameter("auto_despawn", True).value
        )

        node_namespace = self.get_namespace().strip("/")
        if not node_namespace:
            output_topic = _resolve_namespaced(str(output_topic), turtle_namespace)
            spawn_service = _resolve_namespaced(str(spawn_service), turtle_namespace)
            despawn_service = _resolve_namespaced(str(despawn_service), turtle_namespace)
            display_ns = turtle_namespace or "root"
            self.get_logger().info(
                f"Node is in the root namespace; using turtle_namespace={display_ns!r}"
            )

        self._publisher = self.create_publisher(String, output_topic, 10)
        self._spawn_client = self.create_client(Trigger, spawn_service)
        self._despawn_client = self.create_client(Trigger, despawn_service)
        self._is_spawned = False
        self._interactive_session = False

        self.get_logger().info(f"Publishing movement commands to {output_topic}")

    def _publish_command(self, command: str) -> None:
        msg = String()
        msg.data = command
        self._publisher.publish(msg)

    def _print_help(self) -> None:
        self.get_logger().info(HELP_TEXT)

    def _spin_once(self) -> None:
        rclpy.spin_once(self, timeout_sec=0)

    def _call_trigger_service(
        self,
        client: Client,
        label: str,
    ) -> Trigger.Response | None:
        if not client.service_is_ready():
            if not client.wait_for_service(timeout_sec=5.0):
                self.get_logger().error(
                    f"{label} service unavailable: {client.srv_name}"
                )
                return None

        future = client.call_async(Trigger.Request())
        rclpy.spin_until_future_complete(self, future, timeout_sec=5.0)
        if not future.done():
            self.get_logger().error(f"{label} service call timed out")
            return None

        exception = future.exception()
        if exception is not None:
            self.get_logger().error(f"{label} service call failed: {exception}")
            return None

        return future.result()

    def _call_spawn_service(self) -> None:
        response = self._call_trigger_service(self._spawn_client, "Spawn")
        if response is None:
            self.get_logger().error(
                "Spawn failed. If using turtlebot.launch.py, run without "
                "-p turtle_namespace:=root (default is turtle1)."
            )
            return

        self._is_spawned = response.success
        self.get_logger().info(
            f"Spawn service: success={response.success} message={response.message}"
        )

    def _call_despawn_service(self) -> None:
        if not self._is_spawned:
            return

        response = self._call_trigger_service(self._despawn_client, "Despawn")
        if response is None:
            return

        if response.success:
            self._is_spawned = False
        self.get_logger().info(
            f"Despawn service: success={response.success} message={response.message}"
        )

    def shutdown_lifecycle(self) -> None:
        if not self._interactive_session or not self._auto_despawn:
            return
        self._call_despawn_service()

    def _run_windows(self) -> None:
        import msvcrt

        while rclpy.ok():
            self._spin_once()
            if not msvcrt.kbhit():
                time.sleep(0.05)
                continue

            key = msvcrt.getwch()
            if key in {"\x00", "\xe0"}:
                if msvcrt.kbhit():
                    msvcrt.getwch()
                continue

            if self._handle_key(key):
                break

    def _run_unix(self) -> None:
        with _unix_raw_input():
            while rclpy.ok():
                self._spin_once()
                ready, _, _ = select.select([sys.stdin], [], [], 0.1)
                if not ready:
                    continue

                key = sys.stdin.read(1)
                if self._handle_key(key):
                    break

    def _handle_key(self, key: str) -> bool:
        lowered = key.lower()
        if lowered == "q":
            self._publish_command("stop")
            self.get_logger().info("Exiting keyboard control.")
            return True

        if lowered == "h":
            self._print_help()
            return False

        command = KEY_COMMANDS.get(lowered)
        if command is None:
            return False

        self._publish_command(command)
        return False

    def run(self) -> bool:
        if not sys.stdin.isatty():
            self.get_logger().warn(
                "stdin is not a TTY; keyboard control is disabled in this process. "
                "Lifecycle spawn may still run via spawn_on_start. For keyboard input, "
                "run in a separate terminal: "
                "ros2 run roscraft_turtlebot roscraft_turtlebot_keyboard "
                "(defaults to turtle_namespace:=turtle1)"
            )
            return False

        self._interactive_session = True
        if self._auto_spawn:
            self._call_spawn_service()

        self._print_help()
        if os.name == "nt":
            self._run_windows()
        else:
            self._run_unix()
        return True


def main() -> None:
    rclpy.init()
    node = KeyboardInputNode()
    try:
        node.run()
    except KeyboardInterrupt:
        if node._interactive_session:
            node._publish_command("stop")
    finally:
        node.shutdown_lifecycle()
        node.destroy_node()
        rclpy.shutdown()
