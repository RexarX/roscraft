#include <pch.hpp>

#include <roscraft/bridge/nodes/ros/service/call.hpp>

#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/bridge/command/types/service.hpp>
#include <roscraft/bridge/nodes/ros/common.hpp>
#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>
#include <roscraft/memory/arena_allocator.hpp>

#include <rclcpp/generic_client.hpp>
#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

ServiceCallNode::ServiceCallNode(CommandQueue& incoming, CommandQueue& outgoing,
                                 std::pmr::memory_resource* allocator)
    : rclcpp::Node("roscraft_service_call_node"),
      incoming_(incoming),
      outgoing_(outgoing),
      service_call_consumer_(incoming.MakeConsumerToken<ServiceCallCmd>()),
      service_call_response_producer_(
          outgoing.MakeProducerToken<ServiceCallResponseCmd>()),
      allocator_(allocator) {
  using namespace std::chrono_literals;
  client_group_ =
      this->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
  poll_timer_ = this->create_wall_timer(50ms, [this] { OnPollTimer(); });
}

ServiceCallNode::~ServiceCallNode() {
  for (auto& [request_id, timer] : repeat_timers_) {
    timer->cancel();
  }
  repeat_timers_.clear();
}

void ServiceCallNode::DrainServiceCallCommands() {
  auto& storage = incoming_.get().TypedStorage<ServiceCallCmd>();

  ServiceCallCmd cmd(allocator_);
  while (storage.Dequeue(service_call_consumer_, cmd)) {
    DispatchServiceCall(cmd);
  }
}

void ServiceCallNode::DispatchServiceCall(const ServiceCallCmd& cmd) {
  if (cmd.service_name.empty() || cmd.service_type.empty()) [[unlikely]] {
    SendResponse(cmd.request_id, cmd.service_name, cmd.service_type, false, {},
                 "Service name and service type must be non-empty");
    return;
  }

  if (cmd.timeout_seconds < 0.0) [[unlikely]] {
    SendResponse(cmd.request_id, cmd.service_name, cmd.service_type, false, {},
                 "timeout_seconds must be >= 0.0");
    return;
  }

  if (cmd.rate_hz < 0.0) [[unlikely]] {
    SendResponse(cmd.request_id, cmd.service_name, cmd.service_type, false, {},
                 "rate_hz must be >= 0.0");
    return;
  }

  ClearRepeatTimer(cmd.request_id);

  if (!InvokeServiceCall(
          cmd.request_id, cmd.service_name, cmd.service_type,
          std::span<const uint8_t>(cmd.payload.data(), cmd.payload.size()),
          cmd.timeout_seconds)) {
    return;
  }

  uint32_t repeat_count = cmd.repeat_count;
  if (repeat_count == 0) {
    return;
  }

  double rate_hz = cmd.rate_hz;
  if (rate_hz <= 0.0) {
    rate_hz = 1.0;
  }

  auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(1.0 / rate_hz));
  if (period <= std::chrono::nanoseconds::zero()) {
    period = std::chrono::milliseconds(1);
  }

  std::vector<uint8_t> payload(cmd.payload.begin(), cmd.payload.end());
  auto remaining = std::make_shared<uint32_t>(repeat_count);
  auto timer = this->create_wall_timer(
      period,
      [this, request_id = cmd.request_id,
       service_name = std::string(cmd.service_name),
       service_type = std::string(cmd.service_type),
       payload = std::move(payload), timeout_seconds = cmd.timeout_seconds,
       remaining = std::move(remaining)]() {
        if (*remaining == 0) {
          ClearRepeatTimer(request_id);
          return;
        }

        --(*remaining);
        if (!InvokeServiceCall(
                request_id, service_name, service_type,
                std::span<const uint8_t>(payload.data(), payload.size()),
                timeout_seconds)) {
          ClearRepeatTimer(request_id);
          return;
        }

        if (*remaining == 0) {
          ClearRepeatTimer(request_id);
        }
      });
  repeat_timers_[cmd.request_id] = std::move(timer);
}

bool ServiceCallNode::InvokeServiceCall(uint64_t request_id,
                                        std::string_view service_name,
                                        std::string_view service_type,
                                        std::span<const uint8_t> payload,
                                        double timeout_seconds) {
  std::vector<uint8_t> response_payload;

  const auto timeout = ResolveTimeout(timeout_seconds);
  const auto result = CallGenericService(service_name, service_type, payload,
                                         timeout, response_payload);

  if (!result.has_value()) [[unlikely]] {
    SendResponse(request_id, service_name, service_type, false, {},
                 result.error());
    return false;
  }

  SendResponse(request_id, service_name, service_type, true,
               std::span<const uint8_t>(response_payload.data(),
                                        response_payload.size()),
               *result);
  return true;
}

auto ServiceCallNode::CallGenericService(std::string_view service_name,
                                         std::string_view service_type,
                                         std::span<const uint8_t> payload,
                                         std::chrono::nanoseconds timeout,
                                         std::vector<uint8_t>& response_payload)
    -> std::expected<std::string, std::string> {
  auto entry = EnsureGenericClient(service_name, service_type);
  if (!entry) [[unlikely]] {
    return std::unexpected(entry.error());
  }

  auto& client_entry = entry->get();
  auto& client = client_entry.client;
  auto& introspection = client_entry.introspection;
  if (!client->wait_for_service(timeout)) [[unlikely]] {
    const auto it = generic_service_clients_.find(service_name);
    if (it != generic_service_clients_.end()) {
      generic_service_clients_.erase(it);
    }
    return std::unexpected("Service unavailable before timeout");
  }

  const auto request_cdr =
      SerializeRequestPayload(payload, introspection.request);
  if (!request_cdr) [[unlikely]] {
    return std::unexpected(request_cdr.error().message);
  }

  auto request_message = details::DynamicMessage::Create(
      introspection.request, details::MessageInitialization::kZero);
  if (!request_message) [[unlikely]] {
    return std::unexpected(request_message.error().message);
  }

  if (!request_cdr->empty()) {
    const auto deserialize_request = details::DeserializeCdrToMessage(
        *request_cdr, introspection.request, request_message->Data());
    if (!deserialize_request) [[unlikely]] {
      return std::unexpected(deserialize_request.error().message);
    }
  }

  rclcpp::GenericClient::FutureAndRequestId future(
      rclcpp::GenericClient::Future{}, 0);
  try {
    future = client->async_send_request(request_message->Data());
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to send service request: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to send service request");
  }

  if (future.future.wait_for(timeout) != std::future_status::ready)
      [[unlikely]] {
    client->remove_pending_request(future.request_id);
    return std::unexpected("Service response timeout");
  }

  auto response_raw = future.future.get();
  if (!response_raw) [[unlikely]] {
    return std::unexpected("Service response was null");
  }

  const auto response_cdr = details::SerializeMessageToCdr(
      introspection.response, response_raw.get());
  if (!response_cdr) [[unlikely]] {
    return std::unexpected(response_cdr.error().message);
  }
  response_payload = *response_cdr;

  const auto response_text =
      DeserializeResponsePayload(response_payload, introspection.response);
  if (!response_text) [[unlikely]] {
    return std::unexpected(response_text.error().message);
  }

  return *response_text;
}

auto ServiceCallNode::EnsureGenericClient(std::string_view service_name,
                                          std::string_view service_type)
    -> std::expected<std::reference_wrapper<GenericServiceClientEntry>,
                     std::string> {
  std::string service_name_key(service_name);
  const auto existing_it = generic_service_clients_.find(service_name_key);
  if (existing_it != generic_service_clients_.end()) {
    if (existing_it->second.service_type != service_type) [[unlikely]] {
      return std::unexpected(std::format(
          "Service '{}' already cached with type '{}', requested '{}'",
          service_name, existing_it->second.service_type, service_type));
    }

    return std::ref(existing_it->second);
  }

  auto service_introspection = details::LoadServiceIntrospection(service_type);
  if (!service_introspection) [[unlikely]] {
    return std::unexpected(service_introspection.error().message);
  }

  rclcpp::GenericClient::SharedPtr client;
  try {
    client =
        this->create_generic_client(service_name_key, std::string(service_type),
                                    rclcpp::ServicesQoS(), client_group_);
  } catch (const std::exception& ex) {
    return std::unexpected(
        std::format("Failed to create generic client: {}", ex.what()));
  } catch (...) {
    return std::unexpected("Failed to create generic client");
  }

  if (client == nullptr) [[unlikely]] {
    return std::unexpected("Failed to create generic client");
  }

  GenericServiceClientEntry entry{};
  entry.service_type.assign(service_type);
  entry.client = client;
  entry.introspection = std::move(*service_introspection);

  const auto [inserted_it, inserted] = generic_service_clients_.emplace(
      std::move(service_name_key), std::move(entry));
  ROSCRAFT_ASSERT(inserted, "Generic client insertion should succeed for '{}'!",
                  service_name);
  return std::ref(inserted_it->second);
}

auto ServiceCallNode::SerializeRequestPayload(
    std::span<const uint8_t> payload,
    const details::MessageIntrospection& introspection)
    -> details::IntrospectionCodecResult<std::vector<uint8_t>> {
  if (payload.empty()) {
    return details::IntrospectionCodecResult<std::vector<uint8_t>>(
        std::vector<uint8_t>{});
  }

  const auto payload_text = std::string_view(
      std::launder(reinterpret_cast<const char*>(payload.data())),
      payload.size());
  return details::SerializeYamlToCdr(payload_text, introspection);
}

auto ServiceCallNode::DeserializeResponsePayload(
    std::span<const uint8_t> payload,
    const details::MessageIntrospection& introspection)
    -> details::IntrospectionCodecResult<std::string> {
  if (payload.empty()) {
    return details::IntrospectionCodecResult<std::string>(std::string("{}"));
  }

  return details::DeserializeCdrToYaml(payload, introspection);
}

void ServiceCallNode::OnPollTimer() {
  DrainServiceCallCommands();
  scratch_arena_.Reset();
}

void ServiceCallNode::ClearRepeatTimer(uint64_t request_id) {
  const auto it = repeat_timers_.find(request_id);
  if (it == repeat_timers_.end()) [[unlikely]] {
    return;
  }

  it->second->cancel();
  repeat_timers_.erase(it);
}

void ServiceCallNode::SendResponse(uint64_t request_id,
                                   std::string_view service_name,
                                   std::string_view service_type, bool success,
                                   std::span<const uint8_t> response_payload,
                                   std::string_view result_text) {
  ServiceCallResponseCmd cmd(allocator_);
  cmd.request_id = request_id;
  cmd.service_name = std::pmr::string(service_name, allocator_);
  cmd.service_type = std::pmr::string(service_type, allocator_);
  cmd.success = success;
  cmd.response_payload.assign(response_payload.begin(), response_payload.end());
  cmd.result_text = std::pmr::string(result_text, allocator_);

  outgoing_.get().Enqueue(service_call_response_producer_, std::move(cmd));
}

}  // namespace roscraft::bridge
