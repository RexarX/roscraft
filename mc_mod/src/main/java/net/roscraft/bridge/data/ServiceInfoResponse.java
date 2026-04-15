package net.roscraft.bridge.data;

import java.util.List;
import java.util.Objects;

/**
 * Detailed information about a ROS2 service.
 *
 * @param requestId echoes the originating request ID
 * @param serviceName fully-qualified service name
 * @param serviceType resolved service type (empty when unknown)
 * @param clientCount number of discovered clients
 * @param serverCount number of discovered servers
 * @param clientNodes fully-qualified node names acting as clients for this service
 * @param serverNodes fully-qualified node names acting as servers for this service
 */
public record ServiceInfoResponse(
    long requestId,
    String serviceName,
    String serviceType,
    long clientCount,
    long serverCount,
    List<String> clientNodes,
    List<String> serverNodes) {
  /** Canonical constructor — null-check and normalization. */
  public ServiceInfoResponse {
    Objects.requireNonNull(serviceName, "serviceName must not be null");
    Objects.requireNonNull(clientNodes, "clientNodes must not be null");
    Objects.requireNonNull(serverNodes, "serverNodes must not be null");
    serviceType = serviceType == null ? "" : serviceType;
    clientNodes = List.copyOf(clientNodes);
    serverNodes = List.copyOf(serverNodes);
  }

  /** Returns whether a service type is present. */
  public boolean hasServiceType() {
    return !serviceType.isBlank();
  }
}
