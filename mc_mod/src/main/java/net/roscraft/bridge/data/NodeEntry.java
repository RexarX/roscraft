package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * A ROS2 node name (e.g. "/turtlesim").
 *
 * @param name Fully-qualified node name.
 */
public record NodeEntry(String name) {
  /** Canonical constructor — null-checks. */
  public NodeEntry {
    Objects.requireNonNull(name, "name must not be null");
  }
}
