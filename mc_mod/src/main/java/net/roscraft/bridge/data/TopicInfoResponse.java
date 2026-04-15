package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Detailed information about a ROS2 topic.
 *
 * @param requestId echoes the originating request ID
 * @param topicName fully-qualified topic name
 * @param messageType resolved topic type (empty when unknown)
 * @param publisherCount number of discovered publishers
 * @param subscriberCount number of discovered subscribers
 * @param publisherNodes fully-qualified node names publishing this topic
 * @param subscriberNodes fully-qualified node names subscribing to this topic
 */
public record TopicInfoResponse(
    long requestId,
    String topicName,
    String messageType,
    long publisherCount,
    long subscriberCount,
    List<String> publisherNodes,
    List<String> subscriberNodes) {
  /** Canonical constructor — null-check and normalization. */
  public TopicInfoResponse {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(publisherNodes, "publisherNodes must not be null");
    Objects.requireNonNull(subscriberNodes, "subscriberNodes must not be null");
    messageType = messageType == null ? "" : messageType;
    publisherNodes = List.copyOf(publisherNodes);
    subscriberNodes = List.copyOf(subscriberNodes);
  }

  /** Returns whether a topic type is present. */
  public boolean hasMessageType() {
    return !messageType.isBlank();
  }
}
