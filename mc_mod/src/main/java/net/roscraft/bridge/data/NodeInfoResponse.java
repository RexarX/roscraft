package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Detailed graph information for a ROS2 node.
 *
 * @param requestId echoes the originating request ID
 * @param nodeName fully-qualified node name
 * @param publishers topics published by this node
 * @param subscribers topics subscribed by this node
 * @param services services offered/used by this node
 * @param found whether the node exists in the current graph
 */
public record NodeInfoResponse(
    long requestId,
    String nodeName,
    List<TopicEntry> publishers,
    List<TopicEntry> subscribers,
    List<ServiceEntry> services,
    boolean found) {
  /** Canonical constructor — null-checks and defensive copies. */
  public NodeInfoResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(publishers, "publishers must not be null");
    Objects.requireNonNull(subscribers, "subscribers must not be null");
    Objects.requireNonNull(services, "services must not be null");
    publishers = List.copyOf(publishers);
    subscribers = List.copyOf(subscribers);
    services = List.copyOf(services);
  }
}
