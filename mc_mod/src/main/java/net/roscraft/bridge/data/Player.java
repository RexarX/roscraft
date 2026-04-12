package net.roscraft.bridge.data;

import java.util.Objects;

/**
 * Position and identity of a single Minecraft player.
 *
 * @param name Player username.
 * @param x World X coordinate.
 * @param y World Y coordinate (height).
 * @param z World Z coordinate.
 */
public record Player(String name, float x, float y, float z) {
    /** Canonical constructor — null-check on {@code name}. */
    public Player {
        Objects.requireNonNull(name, "name must not be null");
    }
}
