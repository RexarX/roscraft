package net.roscraft.mod.bridge;

import java.util.Locale;
import java.util.Objects;
import net.roscraft.bridge.BridgeCallback;
import net.roscraft.bridge.NetworkBridge;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.mod.RoscraftConfig;
import net.roscraft.mod.RoscraftMod;

public final class BridgeManager {

  private final BridgeCallback callback;
  private RoscraftConfig config;
  private volatile RoscraftBridge bridge;

  public BridgeManager(RoscraftConfig initialConfig, BridgeCallback callback) {
    this.config = Objects.requireNonNull(initialConfig, "initialConfig must not be null");
    this.callback = Objects.requireNonNull(callback, "callback must not be null");
  }

  public synchronized RoscraftConfig config() {
    return config;
  }

  public synchronized boolean isConnected() {
    return bridge != null;
  }

  public synchronized boolean isJniAvailable() {
    return BridgeFactory.isJniAvailable();
  }

  public synchronized String selectedBridgeType() {
    return config.bridgeType();
  }

  public synchronized String activeBridgeType() {
    if (bridge == null) {
      return "disconnected";
    }
    if (bridge instanceof NetworkBridge) {
      return "network";
    }
    return "jni";
  }

  public synchronized String activeEndpoint() {
    if (bridge instanceof NetworkBridge networkBridge) {
      var activeConfig = networkBridge.config();
      return "udp://" + activeConfig.host() + ":" + activeConfig.port();
    }
    return "-";
  }

  public synchronized boolean networkHasSeenInboundTraffic() {
    if (bridge instanceof NetworkBridge networkBridge) {
      return networkBridge.hasSeenInboundTraffic();
    }
    return true;
  }

  public synchronized RoscraftBridge getBridge() {
    return bridge;
  }

  public synchronized RoscraftBridge requireConnectedBridge() {
    if (bridge == null) {
      throw new IllegalStateException(
          "ROS bridge is disconnected. Run /ros connection connect first.");
    }
    return bridge;
  }

  public synchronized OperationResult connect() {
    if (bridge != null) {
      return OperationResult.success("Already connected using " + activeBridgeType() + ".");
    }
    return connectInternal();
  }

  public synchronized OperationResult disconnect() {
    if (bridge == null) {
      return OperationResult.success("Already disconnected.");
    }
    disconnectInternal();
    return OperationResult.success("Disconnected ROS bridge.");
  }

  public synchronized OperationResult setBridgeType(String value) {
    String normalized = normalizeBridgeType(value);
    if (normalized == null) {
      return OperationResult.failure(
          "Invalid bridge mode '" + value + "'. Expected 'network' or 'jni'.");
    }
    if ("jni".equals(normalized) && !BridgeFactory.isJniAvailable()) {
      return OperationResult.failure(
          "JNI bridge is unavailable in this build. Select network mode instead.");
    }

    if (normalized.equals(config.bridgeType())) {
      return OperationResult.success("Bridge mode is already '" + normalized + "'.");
    }

    config = config.withBridgeType(normalized);
    config.save();

    if (bridge != null) {
      return reconnect("Bridge mode set to '" + normalized + "'.");
    }
    return OperationResult.success(
        "Bridge mode set to '" + normalized + "'. Run /ros connection connect to apply.");
  }

  public synchronized OperationResult setNetworkHost(String value) {
    try {
      config = config.withNetworkHost(value);
    } catch (IllegalArgumentException e) {
      return OperationResult.failure(e.getMessage());
    }
    config.save();

    if (bridge != null && config.isNetwork()) {
      return reconnect("Network host updated to " + config.networkHost() + ".");
    }
    return OperationResult.success("Network host updated to " + config.networkHost() + ".");
  }

  public synchronized OperationResult setNetworkPort(int value) {
    try {
      config = config.withNetworkPort(value);
    } catch (IllegalArgumentException e) {
      return OperationResult.failure(e.getMessage());
    }
    config.save();

    if (bridge != null && config.isNetwork()) {
      return reconnect("Network port updated to " + config.networkPort() + ".");
    }
    return OperationResult.success("Network port updated to " + config.networkPort() + ".");
  }

  public synchronized OperationResult setNetworkEndpoint(String host, int port) {
    try {
      config = new RoscraftConfig(config.bridgeType(), host, port);
    } catch (IllegalArgumentException e) {
      return OperationResult.failure(e.getMessage());
    }
    config.save();

    if (bridge != null && config.isNetwork()) {
      return reconnect(
          "Network endpoint updated to " + config.networkHost() + ":" + config.networkPort() + ".");
    }
    return OperationResult.success(
        "Network endpoint updated to " + config.networkHost() + ":" + config.networkPort() + ".");
  }

  public synchronized void tick() {
    if (bridge != null) {
      bridge.tick();
    }
  }

  public synchronized void close() {
    disconnectInternal();
  }

  private OperationResult reconnect(String reason) {
    disconnectInternal();
    OperationResult reconnectResult = connectInternal();
    if (!reconnectResult.success()) {
      RoscraftMod.LOGGER.warn(
          "Bridge reconnect failed after {}: {} — bridge is now disconnected.",
          reason,
          reconnectResult.message());
      return OperationResult.failure(reason + " Reconnect failed: " + reconnectResult.message());
    }
    return OperationResult.success(reason + " " + reconnectResult.message());
  }

  private OperationResult connectInternal() {
    if (config.isJni() && !BridgeFactory.isJniAvailable()) {
      return OperationResult.failure(
          "JNI bridge is unavailable in this build. Select network mode instead.");
    }

    try {
      RoscraftBridge newBridge = BridgeFactory.create(config);
      newBridge.registerCallback(callback);
      bridge = newBridge;
      logConnectionEstablished();
      if ("network".equals(activeBridgeType())) {
        return OperationResult.success(
            "Connected using network bridge at " + activeEndpoint() + ".");
      }
      return OperationResult.success("Connected using JNI bridge.");
    } catch (RuntimeException e) {
      bridge = null;
      return OperationResult.failure("Failed to connect: " + e.getMessage());
    }
  }

  private void disconnectInternal() {
    if (bridge == null) {
      return;
    }
    logConnectionClosed();
    bridge.close();
    bridge = null;
  }

  private void logConnectionEstablished() {
    if (bridge instanceof NetworkBridge networkBridge) {
      var activeConfig = networkBridge.config();
      RoscraftMod.LOGGER.info(
          "Network bridge connected: udp://{}:{}", activeConfig.host(), activeConfig.port());
      return;
    }
    RoscraftMod.LOGGER.info("JNI bridge connected.");
  }

  private void logConnectionClosed() {
    if (bridge instanceof NetworkBridge networkBridge) {
      var activeConfig = networkBridge.config();
      RoscraftMod.LOGGER.info(
          "Network bridge disconnected: udp://{}:{}", activeConfig.host(), activeConfig.port());
      return;
    }
    RoscraftMod.LOGGER.info("JNI bridge disconnected.");
  }

  private static String normalizeBridgeType(String value) {
    if (value == null) {
      return null;
    }
    String normalized = value.trim().toLowerCase(Locale.ROOT);
    if ("network".equals(normalized) || "jni".equals(normalized)) {
      return normalized;
    }
    return null;
  }

  public record OperationResult(boolean success, String message) {
    static OperationResult success(String message) {
      return new OperationResult(true, message);
    }

    static OperationResult failure(String message) {
      return new OperationResult(false, message);
    }
  }
}
