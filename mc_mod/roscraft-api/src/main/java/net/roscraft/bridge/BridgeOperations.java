package net.roscraft.bridge;

/**
 * Operations that addons can invoke on the ROS bridge.
 *
 * <p>This is the narrowed bridge API exposed to addons via
 * {@link net.roscraft.mod.addon.AddonContext#bridgeIfConnected()}. It excludes lifecycle
 * methods ({@code tick}, {@code close}, {@code registerCallback}) so addons
 * cannot accidentally shut down the bridge or replace the global callback.
 *
 * <p>Operations are grouped into domain sub-interfaces accessed via accessor methods:
 * {@link #topics()}, {@link #params()}, {@link #services()}, {@link #actions()},
 * {@link #graph()}. Boolean-heavy overloads have been replaced with options records
 * for readability at call sites.
 */
public interface BridgeOperations {

  TopicOps topics();

  ParamOps params();

  ServiceOps services();

  ActionOps actions();

  GraphOps graph();

  long queryPlayers();

  long sendRawPacket(byte[] flatbufferPayload);

  // ── Sub-interfaces ────────────────────────────────────────────────────

  interface TopicOps {
    long subscribe(String topic, String type);

    long subscribe(String topic, String type, SubscribeOptions opts);

    long unsubscribe(String topic);

    long publish(String topic, String type, byte[] payload);

    long publish(String topic, String type, byte[] payload, PublishOptions opts);

    long hz(String topic, String type, int window);

    long hz(String topic, String type, int window, HzOptions opts);

    long bw(String topic, String type, int window);

    long bw(String topic, String type, int window, BwOptions opts);

    long delay(String topic, String type, int window);

    record SubscribeOptions(boolean once, double timeoutSeconds, boolean raw) {
      public static SubscribeOptions defaults() {
        return new SubscribeOptions(false, 0.0, false);
      }
    }

    record PublishOptions(boolean once, double rateHz, int times, String qosProfile) {
      public static PublishOptions defaults() {
        return new PublishOptions(false, 0.0, 1, "default");
      }
    }

    record HzOptions(boolean wallTime) {
      public static HzOptions defaults() {
        return new HzOptions(false);
      }
    }

    record BwOptions(boolean wallTime) {
      public static BwOptions defaults() {
        return new BwOptions(false);
      }
    }
  }

  interface ParamOps {
    long list(String nodeName, ParamListOptions opts);

    long get(String nodeName, String paramName, ParamGetOptions opts);

    long set(String nodeName, String paramName, String valueText, double timeoutSeconds);

    long describe(String nodeName, String paramName, double timeoutSeconds);

    long dump(String nodeName, String[] prefixes, double timeoutSeconds);

    long load(String nodeName, String yamlText, ParamLoadOptions opts);

    record ParamGetOptions(boolean hideType, double timeoutSeconds) {
      public static ParamGetOptions defaults() {
        return new ParamGetOptions(false, 0.0);
      }
    }

    record ParamListOptions(
        String[] prefixes,
        int depth,
        boolean includeTypes,
        String filterRegex,
        double timeoutSeconds) {
      public static ParamListOptions defaults() {
        return new ParamListOptions(new String[0], 0, false, "", 0.0);
      }
    }

    record ParamLoadOptions(double timeoutSeconds, boolean useWildcard) {
      public static ParamLoadOptions defaults() {
        return new ParamLoadOptions(0.0, true);
      }
    }
  }

  interface ServiceOps {
    long call(String name, String type, byte[] payload, ServiceCallOptions opts);

    record ServiceCallOptions(double timeoutSeconds, int repeatCount, double rateHz) {
      public static ServiceCallOptions defaults() {
        return new ServiceCallOptions(5.0, 1, 0.0);
      }
    }
  }

  interface ActionOps {
    long info(String name, boolean includeHidden);

    long sendGoal(String name, String type, byte[] goalPayload, ActionGoalOptions opts);

    record ActionGoalOptions(boolean feedback, double timeoutSeconds) {
      public static ActionGoalOptions defaults() {
        return new ActionGoalOptions(false, 0.0);
      }
    }
  }

  interface GraphOps {
    long snapshot();

    long nodeInfo(String nodeName, boolean includeHidden);

    long topicInfo(String topicName);

    long serviceInfo(String serviceName);

    long interfaceList(boolean messages, boolean services, boolean actions);

    long interfaceShow(String interfaceType);
  }
}
