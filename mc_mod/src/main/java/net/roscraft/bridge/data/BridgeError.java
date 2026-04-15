package net.roscraft.bridge.data;

/**
 * Error response from the ROS bridge.
 *
 * @param requestId echoes the request ID from the originating command
 * @param errorCode machine-readable error code (e.g., "SUBSCRIBE_FAILED")
 * @param errorMessage human-readable error description
 */
public record BridgeError(long requestId, String errorCode, String errorMessage) {}
