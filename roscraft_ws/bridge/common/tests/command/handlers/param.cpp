#include <doctest/doctest.h>

#include <roscraft/bridge/command/handler/param.hpp>

#include "test_utils.hpp"

#include <memory_resource>

using namespace roscraft::bridge;

namespace {

[[nodiscard]] auto BuildParamListPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::ParamListPacket,
      [](flatbuffers::FlatBufferBuilder& fbb) {
        std::vector<flatbuffers::Offset<flatbuffers::String>> prefixes;
        prefixes.push_back(fbb.CreateString("/robot"));
        return fbs::CreateParamListPacketDirect(fbb, 1U, "/node", &prefixes, 2U,
                                                true, "^foo", 0.5);
      });
}

[[nodiscard]] auto BuildParamGetPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ParamGetPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateParamGetPacketDirect(
                                  fbb, 2U, "/node", "foo", true, 0.75);
                            });
}

[[nodiscard]] auto BuildParamSetPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ParamSetPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateParamSetPacketDirect(
                                  fbb, 3U, "/node", "foo", "123", 1.25);
                            });
}

[[nodiscard]] auto BuildParamDescribePacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ParamDescribePacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateParamDescribePacketDirect(
                                  fbb, 4U, "/node", "foo", 2.0);
                            });
}

[[nodiscard]] auto BuildParamDumpPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(
      fbs::PacketPayload::ParamDumpPacket,
      [](flatbuffers::FlatBufferBuilder& fbb) {
        std::vector<flatbuffers::Offset<flatbuffers::String>> prefixes;
        prefixes.push_back(fbb.CreateString("/robot"));
        return fbs::CreateParamDumpPacketDirect(fbb, 5U, "/node", &prefixes,
                                                3.5);
      });
}

[[nodiscard]] auto BuildParamLoadPacket() -> std::vector<uint8_t> {
  return tests::BuildPacket(fbs::PacketPayload::ParamLoadPacket,
                            [](flatbuffers::FlatBufferBuilder& fbb) {
                              return fbs::CreateParamLoadPacketDirect(
                                  fbb, 6U, "/node", "foo: 42\nbar: true", 1.5,
                                  false);
                            });
}

}  // namespace

TEST_SUITE("bridge::param handlers") {
  TEST_CASE("bridge::ParamListHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamListCmd>();
    out.Register<ParamListResponseCmd>();

    auto handler = ParamListHandler::From(in, out);

    const auto bytes = BuildParamListPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamListCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamListCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 1U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.prefixes.size(), 1U);
    CHECK_EQ(cmd.prefixes[0], "/robot");
    CHECK_EQ(cmd.depth, 2U);
    CHECK(cmd.include_types);
    CHECK_EQ(cmd.filter_regex, "^foo");
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(0.5));
  }

  TEST_CASE("bridge::ParamListHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamListCmd>();
    out.Register<ParamListResponseCmd>();

    auto handler = ParamListHandler::From(in, out);

    ParamListResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 11U;
    cmd.node_name = "/node";
    cmd.names.emplace_back("foo");
    cmd.prefixes.emplace_back("/robot");
    cmd.types.emplace_back("integer");
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamListResponsePacket);

    const auto* response = packet->payload_as_ParamListResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 11U);
    CHECK_EQ(response->node_name()->str(), "/node");
    REQUIRE(response->names() != nullptr);
    REQUIRE(response->prefixes() != nullptr);
    REQUIRE(response->types() != nullptr);
    CHECK_EQ(response->names()->Get(0)->str(), "foo");
    CHECK_EQ(response->prefixes()->Get(0)->str(), "/robot");
    CHECK_EQ(response->types()->Get(0)->str(), "integer");
    CHECK_EQ(out.CommandCount<ParamListResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ParamGetHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamGetCmd>();
    out.Register<ParamGetResponseCmd>();

    auto handler = ParamGetHandler::From(in, out);

    const auto bytes = BuildParamGetPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamGetCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamGetCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 2U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.param_name, "foo");
    CHECK(cmd.hide_type);
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(0.75));
  }

  TEST_CASE("bridge::ParamGetHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamGetCmd>();
    out.Register<ParamGetResponseCmd>();

    auto handler = ParamGetHandler::From(in, out);

    ParamGetResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 12U;
    cmd.node_name = "/node";
    cmd.param_name = "foo";
    cmd.found = true;
    cmd.param_type = "integer";
    cmd.value_text = "42";
    cmd.type_hidden = false;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamGetResponsePacket);

    const auto* response = packet->payload_as_ParamGetResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 12U);
    CHECK_EQ(response->node_name()->str(), "/node");
    CHECK_EQ(response->param_name()->str(), "foo");
    CHECK(response->found());
    CHECK_EQ(response->param_type()->str(), "integer");
    CHECK_EQ(response->value_text()->str(), "42");
    CHECK_FALSE(response->type_hidden());
    CHECK_EQ(out.CommandCount<ParamGetResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ParamSetHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamSetCmd>();
    out.Register<ParamSetResponseCmd>();

    auto handler = ParamSetHandler::From(in, out);

    const auto bytes = BuildParamSetPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamSetCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamSetCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 3U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.param_name, "foo");
    CHECK_EQ(cmd.value_text, "123");
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(1.25));
  }

  TEST_CASE("bridge::ParamSetHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamSetCmd>();
    out.Register<ParamSetResponseCmd>();

    auto handler = ParamSetHandler::From(in, out);

    ParamSetResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 13U;
    cmd.node_name = "/node";
    cmd.param_name = "foo";
    cmd.success = true;
    cmd.reason = "updated";
    cmd.param_type = "integer";
    cmd.value_text = "123";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamSetResponsePacket);

    const auto* response = packet->payload_as_ParamSetResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 13U);
    CHECK_EQ(response->node_name()->str(), "/node");
    CHECK_EQ(response->param_name()->str(), "foo");
    CHECK(response->success());
    CHECK_EQ(response->reason()->str(), "updated");
    CHECK_EQ(response->param_type()->str(), "integer");
    CHECK_EQ(response->value_text()->str(), "123");
    CHECK_EQ(out.CommandCount<ParamSetResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ParamDescribeHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamDescribeCmd>();
    out.Register<ParamDescribeResponseCmd>();

    auto handler = ParamDescribeHandler::From(in, out);

    const auto bytes = BuildParamDescribePacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamDescribeCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamDescribeCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 4U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.param_name, "foo");
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(2.0));
  }

  TEST_CASE("bridge::ParamDescribeHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamDescribeCmd>();
    out.Register<ParamDescribeResponseCmd>();

    auto handler = ParamDescribeHandler::From(in, out);

    ParamDescribeResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 14U;
    cmd.node_name = "/node";
    cmd.param_name = "foo";
    cmd.found = true;
    cmd.param_type = "integer";
    cmd.description = "demo parameter";
    cmd.read_only = false;
    cmd.constraints = "0..100";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamDescribeResponsePacket);

    const auto* response = packet->payload_as_ParamDescribeResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 14U);
    CHECK_EQ(response->node_name()->str(), "/node");
    CHECK_EQ(response->param_name()->str(), "foo");
    CHECK(response->found());
    CHECK_EQ(response->param_type()->str(), "integer");
    CHECK_EQ(response->description()->str(), "demo parameter");
    CHECK_FALSE(response->read_only());
    CHECK_EQ(response->constraints()->str(), "0..100");
    CHECK_EQ(out.CommandCount<ParamDescribeResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ParamDumpHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamDumpCmd>();
    out.Register<ParamDumpResponseCmd>();

    auto handler = ParamDumpHandler::From(in, out);

    const auto bytes = BuildParamDumpPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamDumpCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamDumpCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 5U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.prefixes.size(), 1U);
    CHECK_EQ(cmd.prefixes[0], "/robot");
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(3.5));
  }

  TEST_CASE("bridge::ParamDumpHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamDumpCmd>();
    out.Register<ParamDumpResponseCmd>();

    auto handler = ParamDumpHandler::From(in, out);

    ParamDumpResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 15U;
    cmd.node_name = "/node";
    cmd.yaml_text = "foo: 42";
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamDumpResponsePacket);

    const auto* response = packet->payload_as_ParamDumpResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 15U);
    CHECK_EQ(response->node_name()->str(), "/node");
    CHECK_EQ(response->yaml_text()->str(), "foo: 42");
    CHECK_EQ(out.CommandCount<ParamDumpResponseCmd>(), 0U);
  }

  TEST_CASE("bridge::ParamLoadHandler::Receive") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamLoadCmd>();
    out.Register<ParamLoadResponseCmd>();

    auto handler = ParamLoadHandler::From(in, out);

    const auto bytes = BuildParamLoadPacket();
    const auto* packet = tests::ParsePacket(bytes);
    REQUIRE(packet != nullptr);

    handler.Receive(in, *packet, *std::pmr::get_default_resource());

    ParamLoadCmd cmd(std::pmr::get_default_resource());
    CHECK(in.TypedStorage<ParamLoadCmd>().Dequeue(cmd));
    CHECK_EQ(cmd.request_id, 6U);
    CHECK_EQ(cmd.node_name, "/node");
    CHECK_EQ(cmd.yaml_text, "foo: 42\nbar: true");
    CHECK_EQ(cmd.timeout_seconds, doctest::Approx(1.5));
    CHECK_FALSE(cmd.use_wildcard);
  }

  TEST_CASE("bridge::ParamLoadHandler::DrainAndFlush") {
    CommandQueue in;
    CommandQueue out;
    in.Register<ParamLoadCmd>();
    out.Register<ParamLoadResponseCmd>();

    auto handler = ParamLoadHandler::From(in, out);

    ParamLoadResponseCmd cmd(std::pmr::get_default_resource());
    cmd.request_id = 16U;
    cmd.node_name = "/node";
    cmd.success = true;
    cmd.reason = "loaded";
    cmd.params_loaded = 2U;
    out.Enqueue(std::move(cmd));

    tests::CollectingSink sink;
    flatbuffers::FlatBufferBuilder fbb;
    handler.DrainAndFlush(out, sink, fbb);

    REQUIRE_EQ(sink.packets.size(), 1U);
    const auto* packet = tests::ParsePacket(sink.packets[0]);
    REQUIRE(packet != nullptr);
    CHECK_EQ(packet->payload_type(),
             fbs::PacketPayload::ParamLoadResponsePacket);

    const auto* response = packet->payload_as_ParamLoadResponsePacket();
    REQUIRE(response != nullptr);
    CHECK_EQ(response->request_id(), 16U);
    CHECK_EQ(response->node_name()->str(), "/node");
    CHECK(response->success());
    CHECK_EQ(response->reason()->str(), "loaded");
    CHECK_EQ(response->params_loaded(), 2U);
    CHECK_EQ(out.CommandCount<ParamLoadResponseCmd>(), 0U);
  }
}
