package net.roscraft.bridge.data;

/**
 * Frequency statistics for a ROS2 topic.
 *
 * @param requestId echoes the originating request ID
 * @param topicName fully-qualified topic name
 * @param frequency average messages per second over the last window
 * @param window number of seconds over which frequency is averaged
 * @param messageCount number of messages received in the last window
 */
public record TopicHzResponse(
    long requestId, String topicName, double frequency, int window, int messageCount) {
  public TopicHzResponse {
    if (topicName == null) {
      topicName = "";
    }
  }
}
