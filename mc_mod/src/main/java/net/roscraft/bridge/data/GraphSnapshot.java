package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Immutable snapshot of the live ROS2 graph.
 *
 * <p>Delivered via {@link net.roscraft.bridge.BridgeCallback#onGraphSnapshot}
 * in response to a {@code queryGraph} call.
 *
 * @param requestId Opaque identifier echoed from the originating query.
 * @param topics Sorted list of fully-qualified topic names.
 * @param services Sorted list of fully-qualified service names.
 * @param actions Sorted list of fully-qualified action base names.
 */
public record GraphSnapshot(
        long requestId, List<String> topics, List<String> services, List<String> actions) {
    /** Canonical constructor — defensive copies and null-checks. */
    public GraphSnapshot {
        Objects.requireNonNull(topics, "topics must not be null");
        Objects.requireNonNull(services, "services must not be null");
        Objects.requireNonNull(actions, "actions must not be null");
        topics = List.copyOf(topics);
        services = List.copyOf(services);
        actions = List.copyOf(actions);
    }

    /** Returns {@code true} when all three lists are empty. */
    public boolean isEmpty() {
        return topics.isEmpty() && services.isEmpty() && actions.isEmpty();
    }
}
