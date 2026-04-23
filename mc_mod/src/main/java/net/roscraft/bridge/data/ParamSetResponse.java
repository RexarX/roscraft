package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for a parameter set request. */
public record ParamSetResponse(
    long requestId,
    String nodeName,
    String paramName,
    boolean success,
    String reason,
    String paramType,
    String valueText) {
  public ParamSetResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    reason = reason == null ? "" : reason;
    paramType = paramType == null ? "" : paramType;
    valueText = valueText == null ? "" : valueText;
  }
}
