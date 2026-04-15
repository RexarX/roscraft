package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * A topic name and its associated message type.
 *
 * @param name Fully-qualified topic name (e.g. "/turtle1/cmd_vel").
 * @param type Message type (e.g. "geometry_msgs/msg/Twist").
 */
public record TopicEntry(String name, String type) {
  /** Canonical constructor — null-checks. */
  public TopicEntry {
    Objects.requireNonNull(name, "name must not be null");
    Objects.requireNonNull(type, "type must not be null");
  }
}
