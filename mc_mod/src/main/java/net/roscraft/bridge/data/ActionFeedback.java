package net.roscraft.bridge.data;

import java.util.Arrays;
import java.util.Objects;

/** Streaming feedback payload for action goals. */
public record ActionFeedback(
    long requestId,
    String actionName,
    String actionType,
    byte[] feedbackPayload,
    String feedbackText) {
  public ActionFeedback {
    Objects.requireNonNull(actionName, "actionName must not be null");
    Objects.requireNonNull(actionType, "actionType must not be null");
    feedbackPayload = feedbackPayload == null
        ? new byte[0]
        : Arrays.copyOf(feedbackPayload, feedbackPayload.length);
    feedbackText = feedbackText == null ? "" : feedbackText;
  }

  public int payloadLength() {
    return feedbackPayload.length;
  }
}
