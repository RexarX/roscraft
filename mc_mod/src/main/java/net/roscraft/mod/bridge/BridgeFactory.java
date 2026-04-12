package net.roscraft.mod.bridge;

import net.roscraft.bridge.JniBridge;
import net.roscraft.bridge.NetworkBridge;
import net.roscraft.bridge.RoscraftBridge;
import net.roscraft.build.BuildInfo;
import net.roscraft.mod.RoscraftConfig;
import net.roscraft.mod.RoscraftMod;

/**
 * Creates the correct {@link RoscraftBridge} implementation based on the
 * active {@link RoscraftConfig}.
 */
public final class BridgeFactory {

    private BridgeFactory() {}

    /** Returns whether this build includes the JNI bridge native library. */
    public static boolean isJniAvailable() {
        return BuildInfo.JNI_AVAILABLE;
    }

    /**
     * Instantiate and return the bridge specified by {@code config}.
     *
     * @param config Mod configuration.
     * @return A fully constructed bridge ready to receive {@link net.roscraft.bridge.BridgeCallback} registrations.
     * @throws IllegalStateException if the native library cannot be loaded (JNI mode) or the UDP
     * channel cannot be opened (network mode).
     */
    public static RoscraftBridge create(RoscraftConfig config) {
        if (config.isJni() && BuildInfo.JNI_AVAILABLE) {
            RoscraftMod.LOGGER.info("Using JNI bridge (in-process ROS2)");
            return JniBridge.create();
        }

        if (config.isJni()) {
            RoscraftMod.LOGGER.warn(
                    "JNI bridge requested, but native library is unavailable; "
                            + "falling back to network bridge.");
        }

        RoscraftMod.LOGGER.info(
                "Using network bridge → udp://{}:{}", config.networkHost(), config.networkPort());
        return new NetworkBridge(
                new NetworkBridge.Config(config.networkHost(), config.networkPort()));
    }
}
