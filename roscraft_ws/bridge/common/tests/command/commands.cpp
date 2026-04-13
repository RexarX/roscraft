#include <doctest/doctest.h>

#include <roscraft/bridge/command/commands.hpp>

#include <array>
#include <cstdint>
#include <memory_resource>
#include <string_view>
#include <vector>

using namespace roscraft::bridge;

TEST_SUITE("bridge::Commands") {
  TEST_CASE("bridge::QueryGraphCmd") {
    QueryGraphCmd cmd;

    CHECK_EQ(QueryGraphCmd::kName, "QueryGraphCmd");
    CHECK_EQ(cmd.request_id, 0U);
  }

  TEST_CASE("bridge::SubscribeTopicCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      SubscribeTopicCmd cmd;

      CHECK_EQ(SubscribeTopicCmd::kName, "SubscribeTopicCmd");
      CHECK_EQ(cmd.request_id, 0U);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      SubscribeTopicCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::PublishMessageCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      PublishMessageCmd cmd;

      CHECK_EQ(PublishMessageCmd::kName, "PublishMessageCmd");
      CHECK_EQ(cmd.request_id, 0U);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.payload.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      PublishMessageCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.payload.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::QueryPlayersCmd") {
    QueryPlayersCmd cmd;

    CHECK_EQ(QueryPlayersCmd::kName, "QueryPlayersCmd");
    CHECK_EQ(cmd.request_id, 0U);
  }

  TEST_CASE("bridge::GraphSnapshotCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      GraphSnapshotCmd cmd;

      CHECK_EQ(GraphSnapshotCmd::kName, "GraphSnapshotCmd");
      CHECK_EQ(cmd.request_id, 0U);
      CHECK_EQ(cmd.topics.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.services.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.actions.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 1024> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      GraphSnapshotCmd cmd(&mr);

      CHECK_EQ(cmd.topics.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.services.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.actions.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::TopicPayloadCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicPayloadCmd cmd;

      CHECK_EQ(TopicPayloadCmd::kName, "TopicPayloadCmd");
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.payload.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicPayloadCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.payload.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::Player") {
    SUBCASE("Default ctor initializes defaults and allocator") {
      Player player;

      CHECK(player.name.empty());
      CHECK_EQ(player.x, 0.0F);
      CHECK_EQ(player.y, 0.0F);
      CHECK_EQ(player.z, 0.0F);
      CHECK_EQ(player.name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 256> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      Player player(&mr);

      CHECK_EQ(player.name.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::PlayerListCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      PlayerListCmd cmd;

      CHECK_EQ(PlayerListCmd::kName, "PlayerListCmd");
      CHECK_EQ(cmd.request_id, 0U);
      CHECK_EQ(cmd.players.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      PlayerListCmd cmd(&mr);

      CHECK_EQ(cmd.players.get_allocator().resource(), &mr);
    }
  }
}
