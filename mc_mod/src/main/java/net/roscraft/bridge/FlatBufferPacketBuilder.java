package net.roscraft.bridge;

import com.google.flatbuffers.FlatBufferBuilder;
import java.nio.ByteBuffer;
import roscraft.bridge.fbs.ActionInfoPacket;
import roscraft.bridge.fbs.ActionSendGoalPacket;
import roscraft.bridge.fbs.BridgePacket;
import roscraft.bridge.fbs.InterfaceListPacket;
import roscraft.bridge.fbs.InterfaceShowPacket;
import roscraft.bridge.fbs.NodeInfoPacket;
import roscraft.bridge.fbs.PacketPayload;
import roscraft.bridge.fbs.ParamDescribePacket;
import roscraft.bridge.fbs.ParamDumpPacket;
import roscraft.bridge.fbs.ParamGetPacket;
import roscraft.bridge.fbs.ParamListPacket;
import roscraft.bridge.fbs.ParamLoadPacket;
import roscraft.bridge.fbs.ParamSetPacket;
import roscraft.bridge.fbs.QueryGraphPacket;
import roscraft.bridge.fbs.QueryPlayersPacket;
import roscraft.bridge.fbs.ServiceCallPacket;
import roscraft.bridge.fbs.ServiceInfoPacket;
import roscraft.bridge.fbs.TopicBwPacket;
import roscraft.bridge.fbs.TopicDelayPacket;
import roscraft.bridge.fbs.TopicHzPacket;
import roscraft.bridge.fbs.TopicInfoPacket;
import roscraft.bridge.fbs.TopicPublishMessagePacket;
import roscraft.bridge.fbs.TopicSubscribePacket;

final class FlatBufferPacketBuilder {

  private final FlatBufferBuilder fbb = new FlatBufferBuilder(512);

  ByteBuffer queryGraph(long requestId) {
    fbb.clear();
    int payload = QueryGraphPacket.createQueryGraphPacket(fbb, requestId);
    return finishPacket(PacketPayload.QueryGraphPacket, payload);
  }

  ByteBuffer nodeInfo(long requestId, String nodeName, boolean includeHidden) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int payload =
        NodeInfoPacket.createNodeInfoPacket(fbb, requestId, nodeNameOffset, includeHidden);
    return finishPacket(PacketPayload.NodeInfoPacket, payload);
  }

  ByteBuffer topicInfo(long requestId, String topicName) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int payload = TopicInfoPacket.createTopicInfoPacket(fbb, requestId, topicOffset);
    return finishPacket(PacketPayload.TopicInfoPacket, payload);
  }

  ByteBuffer serviceInfo(long requestId, String serviceName) {
    fbb.clear();
    int serviceOffset = fbb.createString(serviceName);
    int payload = ServiceInfoPacket.createServiceInfoPacket(fbb, requestId, serviceOffset);
    return finishPacket(PacketPayload.ServiceInfoPacket, payload);
  }

  ByteBuffer interfaceList(
      long requestId, boolean includeMessages, boolean includeServices, boolean includeActions) {
    fbb.clear();
    int payload = InterfaceListPacket.createInterfaceListPacket(
        fbb, requestId, includeMessages, includeServices, includeActions);
    return finishPacket(PacketPayload.InterfaceListPacket, payload);
  }

  ByteBuffer interfaceShow(long requestId, String interfaceType) {
    fbb.clear();
    int typeOffset = fbb.createString(interfaceType);
    int payload = InterfaceShowPacket.createInterfaceShowPacket(fbb, requestId, typeOffset);
    return finishPacket(PacketPayload.InterfaceShowPacket, payload);
  }

  ByteBuffer topicSubscribe(
      long requestId,
      String topicName,
      String messageType,
      boolean once,
      double timeoutSeconds,
      boolean raw) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload = TopicSubscribePacket.createTopicSubscribePacket(
        fbb, requestId, topicOffset, typeOffset, once, timeoutSeconds, raw);
    return finishPacket(PacketPayload.TopicSubscribePacket, payload);
  }

  ByteBuffer topicPublishMessage(
      long requestId,
      String topicName,
      String messageType,
      byte[] rawPayload,
      boolean once,
      double rateHz,
      int times,
      String qosProfile) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payloadOffset = TopicPublishMessagePacket.createPayloadVector(fbb, rawPayload);
    int qosOffset = fbb.createString(qosProfile);
    int payload = TopicPublishMessagePacket.createTopicPublishMessagePacket(
        fbb, requestId, topicOffset, typeOffset, payloadOffset, once, rateHz, times, qosOffset);
    return finishPacket(PacketPayload.TopicPublishMessagePacket, payload);
  }

  ByteBuffer queryPlayers(long requestId) {
    fbb.clear();
    int payload = QueryPlayersPacket.createQueryPlayersPacket(fbb, requestId);
    return finishPacket(PacketPayload.QueryPlayersPacket, payload);
  }

  ByteBuffer topicHz(
      long requestId, String topicName, String messageType, int window, boolean wallTime) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload = TopicHzPacket.createTopicHzPacket(
        fbb, requestId, topicOffset, typeOffset, window, wallTime);
    return finishPacket(PacketPayload.TopicHzPacket, payload);
  }

  ByteBuffer topicBw(
      long requestId, String topicName, String messageType, int window, boolean wallTime) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload = TopicBwPacket.createTopicBwPacket(
        fbb, requestId, topicOffset, typeOffset, window, wallTime);
    return finishPacket(PacketPayload.TopicBwPacket, payload);
  }

  ByteBuffer topicDelay(long requestId, String topicName, String messageType, int window) {
    fbb.clear();
    int topicOffset = fbb.createString(topicName);
    int typeOffset = fbb.createString(messageType);
    int payload = TopicDelayPacket.createTopicDelayPacket(
        fbb, requestId, topicOffset, typeOffset, Integer.toUnsignedLong(window));
    return finishPacket(PacketPayload.TopicDelayPacket, payload);
  }

  ByteBuffer serviceCall(
      long requestId,
      String serviceName,
      String serviceType,
      byte[] payloadBytes,
      double timeoutSeconds,
      int repeatCount,
      double rateHz) {
    fbb.clear();
    int serviceNameOffset = fbb.createString(serviceName);
    int serviceTypeOffset = fbb.createString(serviceType);
    int payloadOffset = ServiceCallPacket.createPayloadVector(fbb, payloadBytes);
    int payload = ServiceCallPacket.createServiceCallPacket(
        fbb,
        requestId,
        serviceNameOffset,
        serviceTypeOffset,
        payloadOffset,
        timeoutSeconds,
        repeatCount,
        rateHz);
    return finishPacket(PacketPayload.ServiceCallPacket, payload);
  }

  ByteBuffer paramList(
      long requestId,
      String nodeName,
      String[] prefixes,
      int depth,
      boolean includeTypes,
      String filterRegex,
      double timeoutSeconds) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int filterRegexOffset = fbb.createString(filterRegex);
    int[] prefixOffsets = new int[prefixes.length];
    for (int i = 0; i < prefixes.length; i++) {
      prefixOffsets[i] = fbb.createString(prefixes[i]);
    }
    int prefixesOffset = ParamListPacket.createPrefixesVector(fbb, prefixOffsets);
    int payload = ParamListPacket.createParamListPacket(
        fbb,
        requestId,
        nodeNameOffset,
        prefixesOffset,
        depth,
        includeTypes,
        filterRegexOffset,
        timeoutSeconds);
    return finishPacket(PacketPayload.ParamListPacket, payload);
  }

  ByteBuffer paramGet(
      long requestId, String nodeName, String paramName, boolean hideType, double timeoutSeconds) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int paramNameOffset = fbb.createString(paramName);
    int payload = ParamGetPacket.createParamGetPacket(
        fbb, requestId, nodeNameOffset, paramNameOffset, hideType, timeoutSeconds);
    return finishPacket(PacketPayload.ParamGetPacket, payload);
  }

  ByteBuffer paramSet(
      long requestId, String nodeName, String paramName, String valueText, double timeoutSeconds) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int paramNameOffset = fbb.createString(paramName);
    int valueTextOffset = fbb.createString(valueText);
    int payload = ParamSetPacket.createParamSetPacket(
        fbb, requestId, nodeNameOffset, paramNameOffset, valueTextOffset, timeoutSeconds);
    return finishPacket(PacketPayload.ParamSetPacket, payload);
  }

  ByteBuffer paramDescribe(
      long requestId, String nodeName, String paramName, double timeoutSeconds) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int paramNameOffset = fbb.createString(paramName);
    int payload = ParamDescribePacket.createParamDescribePacket(
        fbb, requestId, nodeNameOffset, paramNameOffset, timeoutSeconds);
    return finishPacket(PacketPayload.ParamDescribePacket, payload);
  }

  ByteBuffer paramDump(long requestId, String nodeName, String[] prefixes, double timeoutSeconds) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int[] prefixOffsets = new int[prefixes.length];
    for (int i = 0; i < prefixes.length; i++) {
      prefixOffsets[i] = fbb.createString(prefixes[i]);
    }
    int prefixesOffset = ParamDumpPacket.createPrefixesVector(fbb, prefixOffsets);
    int payload = ParamDumpPacket.createParamDumpPacket(
        fbb, requestId, nodeNameOffset, prefixesOffset, timeoutSeconds);
    return finishPacket(PacketPayload.ParamDumpPacket, payload);
  }

  ByteBuffer paramLoad(
      long requestId,
      String nodeName,
      String yamlText,
      double timeoutSeconds,
      boolean useWildcard) {
    fbb.clear();
    int nodeNameOffset = fbb.createString(nodeName);
    int yamlTextOffset = fbb.createString(yamlText);
    int payload = ParamLoadPacket.createParamLoadPacket(
        fbb, requestId, nodeNameOffset, yamlTextOffset, timeoutSeconds, useWildcard);
    return finishPacket(PacketPayload.ParamLoadPacket, payload);
  }

  ByteBuffer actionInfo(long requestId, String actionName, boolean includeHidden) {
    fbb.clear();
    int actionNameOffset = fbb.createString(actionName);
    int payload =
        ActionInfoPacket.createActionInfoPacket(fbb, requestId, actionNameOffset, includeHidden);
    return finishPacket(PacketPayload.ActionInfoPacket, payload);
  }

  ByteBuffer actionSendGoal(
      long requestId,
      String actionName,
      String actionType,
      byte[] goalPayload,
      boolean feedback,
      double timeoutSeconds) {
    fbb.clear();
    int actionNameOffset = fbb.createString(actionName);
    int actionTypeOffset = fbb.createString(actionType);
    int goalPayloadOffset = ActionSendGoalPacket.createGoalPayloadVector(fbb, goalPayload);
    int payload = ActionSendGoalPacket.createActionSendGoalPacket(
        fbb,
        requestId,
        actionNameOffset,
        actionTypeOffset,
        goalPayloadOffset,
        feedback,
        timeoutSeconds);
    return finishPacket(PacketPayload.ActionSendGoalPacket, payload);
  }

  private ByteBuffer finishPacket(byte payloadType, int payloadOffset) {
    int root = BridgePacket.createBridgePacket(fbb, payloadType, payloadOffset);
    BridgePacket.finishBridgePacketBuffer(fbb, root);
    return fbb.dataBuffer();
  }
}
