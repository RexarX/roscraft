package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/** Response payload for a parameter list request. */
public record ParamListResponse(
    long requestId,
    String nodeName,
    List<String> names,
    List<String> prefixes,
    List<String> types) {
  public ParamListResponse {
    Objects.requireNonNull(nodeName, "nodeName must not be null");
    Objects.requireNonNull(names, "names must not be null");
    Objects.requireNonNull(prefixes, "prefixes must not be null");
    Objects.requireNonNull(types, "types must not be null");
    names = List.copyOf(names);
    prefixes = List.copyOf(prefixes);
    types = List.copyOf(types);
  }
}
