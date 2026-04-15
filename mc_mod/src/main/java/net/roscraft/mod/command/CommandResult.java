package net.roscraft.mod.command;

/**
 * Result of command-logic execution.
 *
 * @param success whether execution succeeded
 * @param message human-readable message
 * @param requestId request ID associated with an async bridge call (0 when none)
 */
public record CommandResult(boolean success, String message, long requestId) {
  /** Creates a success result without a request ID. */
  public static CommandResult success(String message) {
    return new CommandResult(true, message, 0L);
  }

  /** Creates a success result with a request ID. */
  public static CommandResult success(String message, long requestId) {
    return new CommandResult(true, message, requestId);
  }

  /** Creates a failure result. */
  public static CommandResult failure(String message) {
    return new CommandResult(false, message, 0L);
  }
}
