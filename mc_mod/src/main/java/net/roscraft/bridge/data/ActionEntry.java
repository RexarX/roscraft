package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * An action name and its associated action type.
 *
 * @param name Fully-qualified action base name (e.g. "/rotate_absolute").
 * @param type Action type (e.g. "turtlesim/action/RotateAbsolute").
 */
public record ActionEntry(String name, String type) {
  /** Canonical constructor — null-checks. */
  public ActionEntry {
    Objects.requireNonNull(name, "name must not be null");
    Objects.requireNonNull(type, "type must not be null");
  }
}
