package net.roscraft.bridge.data;

import java.util.Arrays;
import java.util.Objects;

/** Response payload for a service call request. */
public record ServiceCallResponse(
    long requestId,
    String serviceName,
    String serviceType,
    boolean success,
    byte[] responsePayload,
    String resultText) {
  public ServiceCallResponse {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    Objects.requireNonNull(serviceType, "serviceType must not be null");
    responsePayload = responsePayload == null
        ? new byte[0]
        : Arrays.copyOf(responsePayload, responsePayload.length);
    resultText = resultText == null ? "" : resultText;
  }

  public int payloadLength() {
    return responsePayload.length;
  }
}
