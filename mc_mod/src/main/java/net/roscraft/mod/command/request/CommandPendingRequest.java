package net.roscraft.mod.command.request;

import java.util.UUID;

/** A {@code /ros} command waiting for its bridge response. */
public record CommandPendingRequest(
    CommandRequestKind kind, UUID requesterUuid, long createdAtMillis, String metadata) {}
