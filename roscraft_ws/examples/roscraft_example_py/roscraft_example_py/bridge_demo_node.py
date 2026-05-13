import rclpy
from rclpy.node import Node
from std_msgs.msg import String
from std_srvs.srv import Trigger


class BridgeDemoNode(Node):
    def __init__(self) -> None:
        super().__init__("roscraft_bridge_demo")
        self._publisher = self.create_publisher(String, "/roscraft/example/out", 10)
        self._subscription = self.create_subscription(
            String, "/roscraft/example/in", self._on_inbound, 10
        )
        self._service = self.create_service(
            Trigger, "/roscraft/example/ping", self._on_ping
        )
        self._timer = self.create_timer(1.0, self._on_timer)
        self._counter = 0
        self.get_logger().info("Roscraft bridge demo started.")

    def _on_timer(self) -> None:
        self._counter += 1
        msg = String()
        msg.data = f"roscraft demo tick {self._counter}"
        self._publisher.publish(msg)

    def _on_inbound(self, msg: String) -> None:
        self.get_logger().info(f"received: {msg.data}")

    def _on_ping(
        self, request: Trigger.Request, response: Trigger.Response
    ) -> Trigger.Response:
        response.success = True
        response.message = "pong from roscraft_example_py"
        return response


def main() -> None:
    rclpy.init()
    node = BridgeDemoNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
