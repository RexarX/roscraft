package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for a parameter get request. */
public record ParamGetResponse(
    long requestId,
    String nodeName,
    String paramName,
    boolean found,
    String paramType,
    String valueText,
    boolean typeHidden) {
  public ParamGetResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    paramType = paramType == null ? "" : paramType;
    valueText = valueText == null ? "" : valueText;
  }
}
