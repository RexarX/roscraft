package net.roscraft.bridge;

import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Abstract base class for all RoscraftBridge implementations.
 *
 * <p>Subclasses provide the transport mechanism (JNI or network), while this
 * class manages the shared callback reference and request-ID generation.
 *
 * <p><b>Lifecycle:</b>
 * <ol>
 *   <li>Construct the concrete subclass.</li>
 *   <li>Call {@link #registerCallback} to install an event listener.</li>
 *   <li>Call {@link #tick} from your game loop (server tick, etc.).</li>
 *   <li>Call {@link #close} during mod shutdown.</li>
 * </ol>
 *
 * <p><b>Thread safety:</b> {@link #registerCallback} is thread-safe.
 * All other methods, including {@link #tick}, must be called from a single
 * owner thread unless subclasses document otherwise.
 */
public abstract class RoscraftBridge implements AutoCloseable {

    // -------------------------------------------------------------------------
    // Request-ID generator (shared across all instances)
    // -------------------------------------------------------------------------

    private static final AtomicLong REQUEST_COUNTER = new AtomicLong(1L);

    /** Allocates a monotonically increasing request identifier. */
    protected static long nextRequestId() {
        return REQUEST_COUNTER.getAndIncrement();
    }

    // -------------------------------------------------------------------------
    // Callback
    // -------------------------------------------------------------------------

    private final AtomicReference<BridgeCallback> callback =
            new AtomicReference<>(new BridgeCallback() {}); // no-op default

    /**
     * Install (or replace) the callback that receives outgoing ROS2 commands.
     *
     * @param callback Non-null callback implementation.
     * @throws NullPointerException if {@code callback} is {@code null}.
     */
    public final void registerCallback(BridgeCallback callback) {
        this.callback.set(Objects.requireNonNull(callback, "callback must not be null"));
    }

    /**
     * Returns the currently registered callback (never {@code null}).
     *
     * <p>Used by subclasses to deliver events.
     */
    protected final BridgeCallback callback() {
        return callback.get();
    }

    // -------------------------------------------------------------------------
    // Abstract transport API
    // -------------------------------------------------------------------------

    /**
     * Drive one application tick.
     *
     * <p>Drains the outgoing command queue and delivers events to the registered
     * {@link BridgeCallback}. Should be called once per server tick or game loop
     * iteration from the owning thread.
     */
    public abstract void tick();

    /**
     * Request a full ROS2 graph snapshot.
     *
     * <p>The response is delivered asynchronously via
     * {@link BridgeCallback#onGraphSnapshot}.
     *
     * @return The request ID that will be echoed in the response.
     */
    public abstract long queryGraph();

    /**
     * Subscribe to a ROS2 topic.
     *
     * <p>Subsequent messages are pushed via
     * {@link BridgeCallback#onTopicPayload}. Duplicate subscription requests
     * for the same topic are silently ignored by the bridge.
     *
     * @param topicName Fully-qualified topic name (e.g. {@code /cmd_vel}).
     * @param messageType ROS message type string (e.g. {@code geometry_msgs/msg/Twist}).
     * @return The request ID associated with this subscription.
     * @throws NullPointerException if either argument is {@code null}.
     */
    public abstract long subscribeTopic(String topicName, String messageType);

    /**
     * Publish a raw CDR-serialized message onto a ROS2 topic.
     *
     * @param topicName Destination topic.
     * @param messageType ROS message type string.
     * @param payload Raw CDR bytes — copied internally before this method returns.
     * @return The request ID associated with this publish.
     * @throws NullPointerException if any argument is {@code null}.
     */
    public abstract long publishMessage(String topicName, String messageType, byte[] payload);

    /**
     * Request the current list of connected players.
     *
     * <p>The response is delivered asynchronously via
     * {@link BridgeCallback#onPlayerList}.
     *
     * @return The request ID that will be echoed in the response.
     */
    public abstract long queryPlayers();

    // -------------------------------------------------------------------------
    // AutoCloseable
    // -------------------------------------------------------------------------

    /**
     * Shut down the bridge and release all native or network resources.
     *
     * <p>Safe to call multiple times; subsequent calls are no-ops.
     */
    @Override
    public abstract void close();
}
