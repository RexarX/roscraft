package net.roscraft.bridge.data;

import java.util.Objects;

/** Response payload for action info request. */
public record ActionInfoResponse(
    long requestId,
    String actionName,
    String actionType,
    long clientCount,
    long serverCount,
    long feedbackPublisherCount,
    long feedbackSubscriberCount,
    long statusPublisherCount,
    long statusSubscriberCount) {
  public ActionInfoResponse {
    Objects.requireNonNull(actionName, "actionName must not be null");
    actionType = actionType == null ? "" : actionType;
  }

  public boolean hasActionType() {
    return !actionType.isBlank();
  }
}
