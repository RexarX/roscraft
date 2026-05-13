#pragma once

#include <roscraft/bridge/command/handler/action.hpp>
#include <roscraft/bridge/command/handler/addon.hpp>
#include <roscraft/bridge/command/handler/error.hpp>
#include <roscraft/bridge/command/handler/graph.hpp>
#include <roscraft/bridge/command/handler/interface.hpp>
#include <roscraft/bridge/command/handler/node.hpp>
#include <roscraft/bridge/command/handler/param.hpp>
#include <roscraft/bridge/command/handler/player.hpp>
#include <roscraft/bridge/command/handler/service.hpp>
#include <roscraft/bridge/command/handler/topic.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/generated/bridge_packets.hpp>

#include <memory_resource>
#include <tuple>

namespace roscraft::bridge {

class CommandHandlerRegistry;

/// @brief Ordered tuple of all handler types that implement `DrainAndFlush`.
/// @details Pass this as the `TupleT` template argument to
/// `CommandHandlerRegistry::DrainAndFlushAll<DrainAndFlushHandlerTypes>(...)`.
using DrainAndFlushHandlerTypes = std::tuple<
    ActionInfoHandler, ActionSendGoalHandler, ActionFeedbackHandler,
    AddonEventHandler, ErrorHandler, GraphHandler, InterfaceListHandler,
    InterfaceShowHandler, NodeInfoHandler, ParamListHandler, ParamGetHandler,
    ParamSetHandler, ParamDescribeHandler, ParamDumpHandler, ParamLoadHandler,
    PlayerListHandler, ServiceInfoHandler, ServiceCallHandler, TopicInfoHandler,
    TopicHzHandler, TopicBwHandler, TopicDelayHandler, TopicPayloadHandler>;

/// @brief Dispatches an incoming bridge packet to the matching receive handler.
/// @details Routes by `pkt.payload_type()`. Unknown payload types are silently
/// ignored — the caller should log or assert upstream if that is undesirable.
/// @param registry Handler registry owning all registered handlers
/// @param in Incoming command queue
/// @param pkt Decoded bridge packet
/// @param arena Frame-scoped PMR memory resource
inline void DispatchReceive(CommandHandlerRegistry& registry, CommandQueue& in,
                            const fbs::BridgePacket& pkt,
                            std::pmr::memory_resource& arena) {
  switch (pkt.payload_type()) {
    case fbs::PacketPayload::ActionInfoPacket:
      registry.Receive<ActionInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ActionSendGoalPacket:
      registry.Receive<ActionSendGoalHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::QueryGraphPacket:
      registry.Receive<GraphHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::InterfaceListPacket:
      registry.Receive<InterfaceListHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::InterfaceShowPacket:
      registry.Receive<InterfaceShowHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::NodeInfoPacket:
      registry.Receive<NodeInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamListPacket:
      registry.Receive<ParamListHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamGetPacket:
      registry.Receive<ParamGetHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamSetPacket:
      registry.Receive<ParamSetHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamDescribePacket:
      registry.Receive<ParamDescribeHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamDumpPacket:
      registry.Receive<ParamDumpHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ParamLoadPacket:
      registry.Receive<ParamLoadHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::QueryPlayersPacket:
      registry.Receive<PlayerListHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicInfoPacket:
      registry.Receive<TopicInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicSubscribePacket:
      registry.Receive<TopicSubscribeHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicUnsubscribePacket:
      registry.Receive<TopicUnsubscribeHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicPublishMessagePacket:
      registry.Receive<TopicPublishMessageHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicHzPacket:
      registry.Receive<TopicHzHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicBwPacket:
      registry.Receive<TopicBwHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::TopicDelayPacket:
      registry.Receive<TopicDelayHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ServiceInfoPacket:
      registry.Receive<ServiceInfoHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::ServiceCallPacket:
      registry.Receive<ServiceCallHandler>(in, pkt, arena);
      return;
    case fbs::PacketPayload::AddonEventPacket:
      registry.Receive<AddonEventHandler>(in, pkt, arena);
      return;
    default:
      return;
  }
}

}  // namespace roscraft::bridge
