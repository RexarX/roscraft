package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * Result of an interface-definition lookup.
 *
 * @param requestId echoes the originating request ID
 * @param interfaceType interface type string (e.g. {@code std_msgs/msg/String})
 * @param definition full interface definition text when found
 * @param found whether the interface definition was found
 */
public record InterfaceShowResponse(
    long requestId, String interfaceType, String definition, boolean found) {
  /** Canonical constructor — null-check and normalization. */
  public InterfaceShowResponse {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    definition = definition == null ? "" : definition;
  }
}
