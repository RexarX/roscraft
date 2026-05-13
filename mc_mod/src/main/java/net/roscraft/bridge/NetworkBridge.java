package net.roscraft.bridge;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.net.InetSocketAddress;
import java.net.PortUnreachableException;
import java.net.StandardProtocolFamily;
import java.net.StandardSocketOptions;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import roscraft.bridge.fbs.BridgePacket;

/** UDP network-backed {@link RoscraftBridge} implementation. */
public final class NetworkBridge extends AbstractPacketBridge {

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
    checkOpen("NetworkBridge");
    drainInbound();
  }

  @Override
  protected void sendPacket(ByteBuffer buf) {
    try {
      channel.write(buf);
    } catch (IOException e) {
      LOGGER.warn(
          "Send error to udp://{}:{}: {}", config.host(), config.port(), describeException(e));
    }
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

    seenInboundPacket.set(true);
    packetDispatcher.dispatch(BridgePacket.getRootAsBridgePacket(buf));
  }

  private static String describeException(Throwable throwable) {
    String message = throwable.getMessage();
    if (message == null || message.isBlank()) {
      return throwable.getClass().getSimpleName();
    }
    return throwable.getClass().getSimpleName() + ": " + message;
  }
}
