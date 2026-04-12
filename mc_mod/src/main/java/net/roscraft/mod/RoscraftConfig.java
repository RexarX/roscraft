package net.roscraft.mod;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import java.io.IOException;
import java.io.Reader;
import java.io.Writer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Locale;
import java.util.Objects;
import net.fabricmc.loader.api.FabricLoader;
import org.slf4j.LoggerFactory;

/**
 * Mod configuration loaded from {@code config/roscraft.json}.
 *
 * <p>Missing or corrupt config files are replaced with defaults and
 * re-written to disk so the user has a template to edit.
 *
 * <p>All fields are intentionally package-private; access is via the record
 * accessors exposed by this class.
 *
 * @param bridgeType Which bridge backend to use: {@code "jni"} or {@code "network"}.
 * @param networkHost Host of the {@code roscraft_bridge_server} process (used when {@code bridgeType} is {@code "network"}).
 * @param networkPort UDP port of the bridge server.
 */
public record RoscraftConfig(String bridgeType, String networkHost, int networkPort) {
    // -------------------------------------------------------------------------
    // Defaults
    // -------------------------------------------------------------------------

    private static final String DEFAULT_BRIDGE_TYPE = "network";
    private static final String DEFAULT_NETWORK_HOST = "127.0.0.1";
    private static final int DEFAULT_NETWORK_PORT = 7401;
    private static final String CONFIG_FILE_NAME = "roscraft.json";

    // -------------------------------------------------------------------------
    // Canonical constructor — validation
    // -------------------------------------------------------------------------

    public RoscraftConfig {
        bridgeType =
                Objects.requireNonNull(bridgeType, "bridgeType must not be null")
                        .toLowerCase(Locale.ROOT);
        networkHost = Objects.requireNonNull(networkHost, "networkHost must not be null").trim();

        if (!bridgeType.equals("jni") && !bridgeType.equals("network")) {
            throw new IllegalArgumentException(
                    "bridgeType must be 'jni' or 'network', got: " + bridgeType);
        }
        if (networkHost.isEmpty()) {
            throw new IllegalArgumentException("networkHost must not be blank");
        }
        if (networkPort < 1 || networkPort > 65_535) {
            throw new IllegalArgumentException(
                    "networkPort must be in [1, 65535], got: " + networkPort);
        }
    }

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    /**
     * Load config from disk, or create a default config if none exists.
     *
     * @return The loaded (or default) config instance.
     */
    public static RoscraftConfig load() {
        var logger = LoggerFactory.getLogger(RoscraftMod.MOD_ID);
        Path configPath = configPath();

        if (!Files.exists(configPath)) {
            var defaults = defaultConfig();
            defaults.save(configPath);
            logger.info("Created default Roscraft config at {}", configPath);
            return defaults;
        }

        try (Reader reader = Files.newBufferedReader(configPath)) {
            var raw = new Gson().fromJson(reader, RawConfig.class);
            return new RoscraftConfig(
                    coalesce(raw.bridgeType, DEFAULT_BRIDGE_TYPE),
                    coalesce(raw.networkHost, DEFAULT_NETWORK_HOST),
                    raw.networkPort > 0 ? raw.networkPort : DEFAULT_NETWORK_PORT);
        } catch (Exception e) {
            logger.warn("Failed to load Roscraft config ({}), using defaults.", e.getMessage());
            return defaultConfig();
        }
    }

    /** Save this config to the default {@code config/roscraft.json} path. */
    public void save() {
        save(configPath());
    }

    /** Save this config to {@code path}. */
    public void save(Path path) {
        var gson = new GsonBuilder().setPrettyPrinting().create();
        var raw = new RawConfig();
        raw.bridgeType = bridgeType;
        raw.networkHost = networkHost;
        raw.networkPort = networkPort;
        try (Writer writer = Files.newBufferedWriter(path)) {
            gson.toJson(raw, writer);
        } catch (IOException e) {
            LoggerFactory.getLogger(RoscraftMod.MOD_ID)
                    .warn("Failed to save Roscraft config: {}", e.getMessage());
        }
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /** Returns {@code true} when the JNI backend is configured. */
    public boolean isJni() {
        return "jni".equals(bridgeType);
    }

    /** Returns {@code true} when the network backend is configured. */
    public boolean isNetwork() {
        return "network".equals(bridgeType);
    }

    /** Return a new config instance with default values. */
    public static RoscraftConfig defaultConfig() {
        return new RoscraftConfig(DEFAULT_BRIDGE_TYPE, DEFAULT_NETWORK_HOST, DEFAULT_NETWORK_PORT);
    }

    /** Returns the canonical path of {@code config/roscraft.json}. */
    public static Path configPath() {
        return FabricLoader.getInstance().getConfigDir().resolve(CONFIG_FILE_NAME);
    }

    /** Return a copy with an updated bridge type. */
    public RoscraftConfig withBridgeType(String value) {
        return new RoscraftConfig(value, networkHost, networkPort);
    }

    /** Return a copy with an updated network host. */
    public RoscraftConfig withNetworkHost(String value) {
        return new RoscraftConfig(bridgeType, value, networkPort);
    }

    /** Return a copy with an updated network port. */
    public RoscraftConfig withNetworkPort(int value) {
        return new RoscraftConfig(bridgeType, networkHost, value);
    }

    private static String coalesce(String value, String fallback) {
        return (value != null && !value.isBlank()) ? value : fallback;
    }

    // ---- GSON data carrier --------------------------------------------------

    @SuppressWarnings("FieldMayBeFinal")
    private static final class RawConfig {

        String bridgeType = DEFAULT_BRIDGE_TYPE;
        String networkHost = DEFAULT_NETWORK_HOST;
        int networkPort = DEFAULT_NETWORK_PORT;
    }
}
