#!/usr/bin/env python3
import json
import sys

import rclpy
from rclpy.node import Node
from std_msgs.msg import String


class AddonEventDemo(Node):
    def __init__(self) -> None:
        super().__init__("roscraft_addon_event_demo")
        self._pub = self.create_publisher(String, "/roscraft/addon/event_in", 10)
        self._sub = self.create_subscription(
            String,
            "/roscraft/addon/event_out",
            self._on_response,
            10,
        )
        self._timer = self.create_timer(5.0, self._on_timer)
        self._request_id = 0
        self.get_logger().info("AddonEvent demo started.")

    def _on_timer(self) -> None:
        self._request_id += 1
        evt = {
            "request_id": self._request_id,
            "addon_id": "ping",
            "event_type": "ping",
            "encoding": "utf-8",
            "response": False,
            "payload": f"hello from ROS {self._request_id}",
        }
        msg = String()
        msg.data = json.dumps(evt)
        self._pub.publish(msg)
        self.get_logger().info(
            f"Sent AddonEvent(id={self._request_id}, addon=ping, type=ping)"
        )

    def _on_response(self, msg: String) -> None:
        try:
            data = json.loads(msg.data)
        except json.JSONDecodeError:
            data = {"raw": msg.data}
        self.get_logger().info(f"AddonEvent response: {json.dumps(data)}")


def main() -> None:
    rclpy.init(args=sys.argv)
    node = AddonEventDemo()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
