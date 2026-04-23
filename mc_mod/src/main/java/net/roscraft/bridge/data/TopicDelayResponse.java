package net.roscraft.bridge.data;

/** Delay statistics for a ROS2 topic based on header stamps. */
public record TopicDelayResponse(
    long requestId,
    String topicName,
    double averageDelay,
    double minDelay,
    double maxDelay,
    int window,
    int messageCount) {
  public TopicDelayResponse {
    if (topicName == null) {
      topicName = "";
    }
  }
}
