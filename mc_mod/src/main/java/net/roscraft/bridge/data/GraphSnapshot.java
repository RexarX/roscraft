package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Immutable snapshot of the live ROS2 graph.
 *
 * <p>
 * Delivered via {@link net.roscraft.bridge.BridgeCallback#onGraphSnapshot} in
 * response to a {@code queryGraph} call.
 *
 * @param requestId Opaque identifier echoed from the originating query.
 * @param nodes Sorted list of ROS2 node names.
 * @param topics Sorted list of topic entries (name + type).
 * @param services Sorted list of service entries (name + type).
 * @param actions Sorted list of action entries (name + type).
 */
public record GraphSnapshot(
    long requestId,
    List<NodeEntry> nodes,
    List<TopicEntry> topics,
    List<ServiceEntry> services,
    List<ActionEntry> actions) {
  /** Canonical constructor — defensive copies and null-checks. */
  public GraphSnapshot {
    Objects.requireNonNull(nodes, "nodes must not be null");
    Objects.requireNonNull(topics, "topics must not be null");
    Objects.requireNonNull(services, "services must not be null");
    Objects.requireNonNull(actions, "actions must not be null");
    nodes = List.copyOf(nodes);
    topics = List.copyOf(topics);
    services = List.copyOf(services);
    actions = List.copyOf(actions);
  }

  /** Returns {@code true} when all four lists are empty. */
  public boolean isEmpty() {
    return (nodes.isEmpty() && topics.isEmpty() && services.isEmpty() && actions.isEmpty());
  }
}
