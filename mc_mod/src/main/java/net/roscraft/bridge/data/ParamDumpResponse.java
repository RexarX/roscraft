package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for a parameter dump request. */
public record ParamDumpResponse(long requestId, String nodeName, String yamlText) {
  public ParamDumpResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    yamlText = yamlText == null ? "" : yamlText;
  }
}
