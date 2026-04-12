package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * List of currently connected players.
 *
 * <p>Delivered via {@link net.roscraft.bridge.BridgeCallback#onPlayerList}
 * in response to a {@code queryPlayers} call.
 *
 * @param requestId Opaque identifier echoed from the originating query.
 * @param players Immutable list of players present at query time.
 */
public record PlayerList(long requestId, List<Player> players) {
    /** Canonical constructor — null-check and defensive copy. */
    public PlayerList {
        Objects.requireNonNull(players, "players must not be null");
        players = List.copyOf(players);
    }

    /** Returns the number of players in this list. */
    public int size() {
        return players.size();
    }

    /** Returns {@code true} when no players are present. */
    public boolean isEmpty() {
        return players.isEmpty();
    }
}
