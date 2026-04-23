package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for a parameter load request. */
public record ParamLoadResponse(
    long requestId, String nodeName, boolean success, String reason, long paramsLoaded) {
  public ParamLoadResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    reason = reason == null ? "" : reason;
  }
}
