package net.roscraft.bridge.data;

import java.util.Arrays;
import java.util.Objects;

/** Final result payload for action goals. */
public record ActionResult(
    long requestId,
    String actionName,
    String actionType,
    boolean success,
    byte[] resultPayload,
    String resultText) {
  public ActionResult {
    Objects.requireNonNull(actionName, "actionName must not be null");
    Objects.requireNonNull(actionType, "actionType must not be null");
    resultPayload =
        resultPayload == null ? new byte[0] : Arrays.copyOf(resultPayload, resultPayload.length);
    resultText = resultText == null ? "" : resultText;
  }

  public int payloadLength() {
    return resultPayload.length;
  }
}
