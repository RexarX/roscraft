package net.roscraft.bridge;

/**
 * Operations that addons can invoke on the ROS bridge.
 *
 * <p>This is the narrowed bridge API exposed to addons via
 * {@link net.roscraft.mod.addon.AddonContext#bridge()}. It excludes lifecycle
 * methods ({@code tick}, {@code close}, {@code registerCallback}) so addons
 * cannot accidentally shut down the bridge or replace the global callback.
 */
public interface BridgeOperations {

  long queryGraph();

  long nodeInfo(String nodeName, boolean includeHidden);

  long topicInfo(String topicName);

  long serviceInfo(String serviceName);

  long interfaceList(boolean includeMessages, boolean includeServices, boolean includeActions);

  long interfaceShow(String interfaceType);

  long subscribeTopic(String topicName, String messageType);

  long subscribeTopic(
      String topicName, String messageType, boolean once, double timeoutSeconds, boolean raw);

  long unsubscribeTopic(String topicName);

  long publishMessage(String topicName, String messageType, byte[] payload);

  long publishMessage(
      String topicName,
      String messageType,
      byte[] payload,
      boolean once,
      double rateHz,
      int times,
      String qosProfile);

  long topicHz(String topicName, String messageType, int window);

  long topicHz(String topicName, String messageType, int window, boolean wallTime);

  long topicBw(String topicName, String messageType, int window);

  long topicBw(String topicName, String messageType, int window, boolean wallTime);

  long topicDelay(String topicName, String messageType, int window);

  long serviceCall(
      String serviceName,
      String serviceType,
      byte[] payload,
      double timeoutSeconds,
      int repeatCount,
      double rateHz);

  long paramList(
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds);

  long paramGet(String nodeName, String paramName, boolean hideType, double timeoutSeconds);

  long paramSet(String nodeName, String paramName, String valueText, double timeoutSeconds);

  long paramDescribe(String nodeName, String paramName, double timeoutSeconds);

  long paramDump(String nodeName, String[] prefixes, double timeoutSeconds);

  long paramLoad(String nodeName, String yamlText, double timeoutSeconds, boolean useWildcard);

  long actionInfo(String actionName, boolean includeHidden);

  long actionSendGoal(
      String actionName,
      String actionType,
      byte[] goalPayload,
      boolean feedback,
      double timeoutSeconds);

  long queryPlayers();

  long sendAddonEvent(
      String addonId, String eventType, String encoding, byte[] payload, boolean response);
}
