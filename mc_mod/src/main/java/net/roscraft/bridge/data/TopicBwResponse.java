package net.roscraft.bridge.data;

/**
 * Bandwidth statistics for a ROS2 topic.
 *
 * @param requestId echoes the originating request ID
 * @param topicName fully-qualified topic name
 * @param bytesPerSecond average bytes per second over the last window
 * @param window number of seconds over which bandwidth is averaged
 * @param messageCount number of messages received in the last window
 * @param totalBytes total number of bytes received
 */
public record TopicBwResponse(
    long requestId,
    String topicName,
    double bytesPerSecond,
    int window,
    int messageCount,
    long totalBytes) {
  public TopicBwResponse {
    if (topicName == null) {
      topicName = "";
    }
  }
}
