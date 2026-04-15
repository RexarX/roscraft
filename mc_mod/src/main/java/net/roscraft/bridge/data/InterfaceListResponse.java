package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Available ROS interface types grouped by kind.
 *
 * @param requestId echoes the originating request ID
 * @param messages message interfaces (e.g. std_msgs/msg/String)
 * @param services service interfaces (e.g. std_srvs/srv/Empty)
 * @param actions action interfaces (e.g. nav2_msgs/action/NavigateToPose)
 */
public record InterfaceListResponse(
    long requestId, List<String> messages, List<String> services, List<String> actions) {
  /** Canonical constructor — null-checks and defensive copies. */
  public InterfaceListResponse {
    Objects.requireNonNull(messages, "messages must not be null");
    Objects.requireNonNull(services, "services must not be null");
    Objects.requireNonNull(actions, "actions must not be null");
    messages = List.copyOf(messages);
    services = List.copyOf(services);
    actions = List.copyOf(actions);
  }
}
