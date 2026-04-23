package net.roscraft.bridge;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.InetSocketAddress;
import java.net.PortUnreachableException;
import java.net.StandardProtocolFamily;
import java.net.StandardSocketOptions;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.Arrays;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import roscraft.bridge.fbs.BridgePacket;

/** UDP network-backed {@link RoscraftBridge} implementation. */
public final class NetworkBridge extends RoscraftBridge {

  private static final Logger LOGGER = LoggerFactory.getLogger(NetworkBridge.class);

  public record Config(String host, int port, int maxDatagramSize) {
    public static final int DEFAULT_MAX_DATAGRAM_SIZE = 65_507;

    public Config(String host, int port) {
      this(host, port, DEFAULT_MAX_DATAGRAM_SIZE);
    }

    public Config {
      Objects.requireNonNull(host, "host must not be null");
      if (port < 1 || port > 65_535) {
        throw new IllegalArgumentException("port must be in [1, 65535], got: " + port);
      }
      if (maxDatagramSize < 1) {
        throw new IllegalArgumentException(
            "maxDatagramSize must be positive, got: " + maxDatagramSize);
      }
    }
  }

  private final Config config;
  private final DatagramChannel channel;
  private final ByteBuffer recvBuffer;
  private final FlatBufferPacketBuilder packetBuilder = new FlatBufferPacketBuilder();
  private final FlatBufferPacketDispatcher packetDispatcher =
      new FlatBufferPacketDispatcher(this::callback);
  private final AtomicBoolean closed = new AtomicBoolean(false);
  private final AtomicBoolean seenInboundPacket = new AtomicBoolean(false);

  public NetworkBridge(Config config) {
    this.config = Objects.requireNonNull(config, "config must not be null");
    this.recvBuffer = ByteBuffer.allocateDirect(config.maxDatagramSize());

    try {
      channel = DatagramChannel.open(StandardProtocolFamily.INET);
      channel.setOption(StandardSocketOptions.SO_REUSEADDR, true);
      channel.configureBlocking(false);
      channel.connect(new InetSocketAddress(config.host(), config.port()));
    } catch (IOException e) {
      throw new UncheckedIOException(
          "Failed to open UDP channel to " + config.host() + ":" + config.port(), e);
    }
  }

  @Override
  public void tick() {
    checkOpen();
    drainInbound();
  }

  @Override
  public long queryGraph() {
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.queryGraph(id));
    return id;
  }

  @Override
  public long nodeInfo(String nodeName, boolean includeHidden) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.nodeInfo(id, nodeName, includeHidden));
    return id;
  }

  @Override
  public long topicInfo(String topicName) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicInfo(id, topicName));
    return id;
  }

  @Override
  public long serviceInfo(String serviceName) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.serviceInfo(id, serviceName));
    return id;
  }

  @Override
  public long interfaceList(
      boolean includeMessages, boolean includeServices, boolean includeActions) {
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.interfaceList(id, includeMessages, includeServices, includeActions));
    return id;
  }

  @Override
  public long interfaceShow(String interfaceType) {
    Objects.requireNonNull(interfaceType, "interfaceType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.interfaceShow(id, interfaceType));
    return id;
  }

  @Override
  public long subscribeTopic(String topicName, String messageType) {
    return subscribeTopic(topicName, messageType, false, 0.0, false);
  }

  @Override
  public long subscribeTopic(
      String topicName, String messageType, boolean once, double timeoutSeconds, boolean raw) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicSubscribe(id, topicName, messageType, once, timeoutSeconds, raw));
    return id;
  }

  @Override
  public long publishMessage(String topicName, String messageType, byte[] payload) {
    return publishMessage(topicName, messageType, payload, false, 0.0, 0, "default");
  }

  @Override
  public long publishMessage(
      String topicName,
      String messageType,
      byte[] payload,
      boolean once,
      double rateHz,
      int times,
      String qosProfile) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    Objects.requireNonNull(qosProfile, "qosProfile must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicPublishMessage(
        id, topicName, messageType, payload, once, rateHz, Math.max(0, times), qosProfile));
    return id;
  }

  @Override
  public long queryPlayers() {
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.queryPlayers(id));
    return id;
  }

  @Override
  public long topicHz(String topicName, String messageType, int window) {
    return topicHz(topicName, messageType, window, false);
  }

  @Override
  public long topicHz(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicHz(id, topicName, messageType, window, wallTime));
    return id;
  }

  @Override
  public long topicBw(String topicName, String messageType, int window) {
    return topicBw(topicName, messageType, window, false);
  }

  @Override
  public long topicBw(String topicName, String messageType, int window, boolean wallTime) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicBw(id, topicName, messageType, window, wallTime));
    return id;
  }

  @Override
  public long topicDelay(String topicName, String messageType, int window) {
    Objects.requireNonNull(topicName, "topicName must not be null");
    Objects.requireNonNull(messageType, "messageType must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.topicDelay(id, topicName, messageType, Math.max(1, window)));
    return id;
  }

  @Override
  public long serviceCall(
      String serviceName,
      String serviceType,
      byte[] payload,
      double timeoutSeconds,
      int repeatCount,
      double rateHz) {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    Objects.requireNonNull(serviceType, "serviceType must not be null");
    Objects.requireNonNull(payload, "payload must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.serviceCall(
        id,
        serviceName,
        serviceType,
        payload,
        Math.max(0.0, timeoutSeconds),
        Math.max(0, repeatCount),
        Math.max(0.0, rateHz)));
    return id;
  }

  @Override
  public long paramList(
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.paramList(
        id,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0, depth),
        includeTypes,
        filterRegex == null ? "" : filterRegex,
        Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramGet(String nodeName, String paramName, boolean hideType, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(
        packetBuilder.paramGet(id, nodeName, paramName, hideType, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramSet(String nodeName, String paramName, String valueText, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    Objects.requireNonNull(valueText, "valueText must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(
        packetBuilder.paramSet(id, nodeName, paramName, valueText, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramDescribe(String nodeName, String paramName, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(paramName, "paramName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.paramDescribe(id, nodeName, paramName, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramDump(String nodeName, String[] prefixes, double timeoutSeconds) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.paramDump(
        id,
        nodeName,
        prefixes == null ? new String[0] : Arrays.copyOf(prefixes, prefixes.length),
        Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public long paramLoad(
      String nodeName, String yamlText, double timeoutSeconds, boolean useWildcard) {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(yamlText, "yamlText must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.paramLoad(
        id, nodeName, yamlText, Math.max(0.0, timeoutSeconds), useWildcard));
    return id;
  }

  @Override
  public long actionInfo(String actionName, boolean includeHidden) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.actionInfo(id, actionName, includeHidden));
    return id;
  }

  @Override
  public long actionSendGoal(
      String actionName,
      String actionType,
      byte[] goalPayload,
      boolean feedback,
      double timeoutSeconds) {
    Objects.requireNonNull(actionName, "actionName must not be null");
    Objects.requireNonNull(actionType, "actionType must not be null");
    Objects.requireNonNull(goalPayload, "goalPayload must not be null");
    checkOpen();
    long id = nextRequestId();
    sendPacket(packetBuilder.actionSendGoal(
        id, actionName, actionType, goalPayload, feedback, Math.max(0.0, timeoutSeconds)));
    return id;
  }

  @Override
  public void close() {
    if (closed.compareAndSet(false, true)) {
      try {
        channel.close();
      } catch (IOException e) {
        LOGGER.warn("Error closing UDP channel: {}", describeException(e));
      }
    }
  }

  public Config config() {
    return config;
  }

  public boolean hasSeenInboundTraffic() {
    return seenInboundPacket.get();
  }

  private void drainInbound() {
    try {
      while (true) {
        recvBuffer.clear();
        if (channel.receive(recvBuffer) == null) {
          break;
        }
        seenInboundPacket.set(true);
        recvBuffer.flip();

        byte[] bytes = new byte[recvBuffer.remaining()];
        recvBuffer.get(bytes);
        dispatch(ByteBuffer.wrap(bytes));
      }
    } catch (IOException e) {
      if (e instanceof PortUnreachableException) {
        LOGGER.warn(
            "UDP destination unreachable at udp://{}:{}; verify bridge host/port and firewall/WSL networking.",
            config.host(),
            config.port());
        return;
      }
      LOGGER.warn(
          "Receive error from udp://{}:{}: {}", config.host(), config.port(), describeException(e));
    }
  }

  private void dispatch(ByteBuffer buf) {
    if (!BridgePacket.BridgePacketBufferHasIdentifier(buf)) {
      LOGGER.warn("Dropping datagram with unknown file identifier.");
      return;
    }

    packetDispatcher.dispatch(BridgePacket.getRootAsBridgePacket(buf));
  }

  private void sendPacket(ByteBuffer buf) {
    try {
      channel.write(buf);
    } catch (IOException e) {
      LOGGER.warn(
          "Send error to udp://{}:{}: {}", config.host(), config.port(), describeException(e));
    }
  }

  private void checkOpen() {
    if (closed.get()) {
      throw new IllegalStateException("NetworkBridge has been closed.");
    }
  }

  private static String describeException(Throwable throwable) {
    String message = throwable.getMessage();
    if (message == null || message.isBlank()) {
      return throwable.getClass().getSimpleName();
    }
    return throwable.getClass().getSimpleName() + ": " + message;
  }
}
