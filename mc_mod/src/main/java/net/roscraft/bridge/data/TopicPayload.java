package net.roscraft.bridge.data;

import java.util.Arrays;
import java.util.Objects;

/**
 * Raw CDR-serialized message received from a subscribed ROS2 topic.
 *
 * <p>Delivered via {@link net.roscraft.bridge.BridgeCallback#onTopicPayload}
 * whenever the subscription relay forwards a message.
 *
 * @param topicName Fully-qualified topic name (e.g. {@code /cmd_vel}).
 * @param messageType ROS message type string (e.g. {@code geometry_msgs/msg/Twist}).
 * @param payload Raw CDR bytes. A defensive copy is stored internally.
 */
public record TopicPayload(String topicName, String messageType, byte[] payload) {
    /** Canonical constructor — null-checks and defensive copy of {@code payload}. */
    public TopicPayload {
        Objects.requireNonNull(topicName, "topicName must not be null");
        Objects.requireNonNull(messageType, "messageType must not be null");
        Objects.requireNonNull(payload, "payload must not be null");
        payload = Arrays.copyOf(payload, payload.length);
    }

    /**
     * Returns a defensive copy of the raw CDR payload bytes.
     *
     * <p>Overrides the default record accessor to preserve immutability.
     */
    @Override
    public byte[] payload() {
        return Arrays.copyOf(payload, payload.length);
    }

    /** Length of the CDR payload in bytes. */
    public int payloadLength() {
        return payload.length;
    }
}
