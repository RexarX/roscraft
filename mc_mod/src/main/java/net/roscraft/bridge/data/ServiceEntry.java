package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * A service name and its associated service type.
 *
 * @param name Fully-qualified service name (e.g. "/turtle1/set_pen").
 * @param type Service type (e.g. "turtlesim/srv/SetPen").
 */
public record ServiceEntry(String name, String type) {
  /** Canonical constructor — null-checks. */
  public ServiceEntry {
    Objects.requireNonNull(name, "name must not be null");
    Objects.requireNonNull(type, "type must not be null");
  }
}
