from __future__ import annotations

import os
import select
import sys
import time
from contextlib import contextmanager
from typing import Final, Iterator

import rclpy
from rclpy.node import Node
from std_msgs.msg import String

DEFAULT_OUTPUT_TOPIC: Final = "roscraft/turtlebot/movement/cmd"
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
        output_topic = self.declare_parameter(
            "output_topic",
            DEFAULT_OUTPUT_TOPIC,
        ).value
        self._publisher = self.create_publisher(String, output_topic, 10)
        self.get_logger().info(f"Publishing movement commands to {output_topic}")

    def _publish_command(self, command: str) -> None:
        msg = String()
        msg.data = command
        self._publisher.publish(msg)

    def _print_help(self) -> None:
        self.get_logger().info(HELP_TEXT)

    def _run_windows(self) -> None:
        import msvcrt

        while rclpy.ok():
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

    def run(self) -> None:
        if not sys.stdin.isatty():
            raise RuntimeError("Keyboard input requires an interactive terminal.")

        self._print_help()
        if os.name == "nt":
            self._run_windows()
        else:
            self._run_unix()


def main() -> None:
    rclpy.init()
    node = KeyboardInputNode()
    try:
        node.run()
    except KeyboardInterrupt:
        node._publish_command("stop")
    finally:
        node.destroy_node()
        rclpy.shutdown()
