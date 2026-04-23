package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for a parameter describe request. */
public record ParamDescribeResponse(
    long requestId,
    String nodeName,
    String paramName,
    boolean found,
    String paramType,
    String description,
    boolean readOnly,
    String constraints) {
  public ParamDescribeResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    paramType = paramType == null ? "" : paramType;
    description = description == null ? "" : description;
    constraints = constraints == null ? "" : constraints;
  }
}
