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
    CHECK_EQ(cmd.request_id, 0);
  }

  TEST_CASE("bridge::NodeInfoCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      NodeInfoCmd cmd;

      CHECK_EQ(NodeInfoCmd::kName, "NodeInfoCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_FALSE(cmd.include_hidden);
      CHECK_EQ(cmd.node_name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      NodeInfoCmd cmd(&mr);

      CHECK_EQ(cmd.node_name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      NodeInfoCmd cmd;

      cmd.request_id = 9;
      cmd.node_name = "/node/info";
      cmd.include_hidden = true;

      CHECK_EQ(cmd.request_id, 9);
      CHECK_EQ(cmd.node_name, "/node/info");
      CHECK(cmd.include_hidden);
    }
  }

  TEST_CASE("bridge::TopicInfoCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicInfoCmd cmd;

      CHECK_EQ(TopicInfoCmd::kName, "TopicInfoCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicInfoCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicInfoCmd cmd;

      cmd.request_id = 10;
      cmd.topic_name = "/topic/info";

      CHECK_EQ(cmd.request_id, 10);
      CHECK_EQ(cmd.topic_name, "/topic/info");
    }
  }

  TEST_CASE("bridge::ServiceInfoCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      ServiceInfoCmd cmd;

      CHECK_EQ(ServiceInfoCmd::kName, "ServiceInfoCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.service_name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      ServiceInfoCmd cmd(&mr);

      CHECK_EQ(cmd.service_name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      ServiceInfoCmd cmd;

      cmd.request_id = 11;
      cmd.service_name = "/service/info";

      CHECK_EQ(cmd.request_id, 11);
      CHECK_EQ(cmd.service_name, "/service/info");
    }
  }

  TEST_CASE("bridge::InterfaceListCmd") {
    InterfaceListCmd cmd;

    CHECK_EQ(InterfaceListCmd::kName, "InterfaceListCmd");
    CHECK_EQ(cmd.request_id, 0);
    CHECK(cmd.include_messages);
    CHECK(cmd.include_services);
    CHECK(cmd.include_actions);

    cmd.request_id = 12;
    cmd.include_messages = false;
    cmd.include_services = true;
    cmd.include_actions = false;

    CHECK_EQ(cmd.request_id, 12);
    CHECK_FALSE(cmd.include_messages);
    CHECK(cmd.include_services);
    CHECK_FALSE(cmd.include_actions);
  }

  TEST_CASE("bridge::InterfaceShowCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      InterfaceShowCmd cmd;

      CHECK_EQ(InterfaceShowCmd::kName, "InterfaceShowCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.interface_type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      InterfaceShowCmd cmd(&mr);

      CHECK_EQ(cmd.interface_type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      InterfaceShowCmd cmd;

      cmd.request_id = 12;
      cmd.interface_type = "std_msgs/msg/String";

      CHECK_EQ(cmd.request_id, 12);
      CHECK_EQ(cmd.interface_type, "std_msgs/msg/String");
    }
  }

  TEST_CASE("bridge::TopicSubscribeCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicSubscribeCmd cmd;

      CHECK_EQ(TopicSubscribeCmd::kName, "TopicSubscribeCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicSubscribeCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
    }

    SUBCASE("Default option fields") {
      TopicSubscribeCmd cmd;

      CHECK_EQ(cmd.request_id, 0);
      CHECK_FALSE(cmd.once);
      CHECK_EQ(cmd.timeout_seconds, doctest::Approx(0.0));
      CHECK_FALSE(cmd.raw);
    }

    SUBCASE("Fields can be set and read") {
      TopicSubscribeCmd cmd;

      cmd.request_id = 31;
      cmd.topic_name = "/topic/echo";
      cmd.message_type = "std_msgs/msg/String";
      cmd.once = true;
      cmd.timeout_seconds = 2.5;
      cmd.raw = true;

      CHECK_EQ(cmd.request_id, 31);
      CHECK_EQ(cmd.topic_name, "/topic/echo");
      CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
      CHECK(cmd.once);
      CHECK_EQ(cmd.timeout_seconds, doctest::Approx(2.5));
      CHECK(cmd.raw);
    }
  }

  TEST_CASE("bridge::TopicPublishMessageCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicPublishMessageCmd cmd;

      CHECK_EQ(TopicPublishMessageCmd::kName, "TopicPublishMessageCmd");
      CHECK_EQ(cmd.request_id, 0);
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

      TopicPublishMessageCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.payload.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::TopicHzCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicHzCmd cmd;

      CHECK_EQ(TopicHzCmd::kName, "TopicHzCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.window, 10);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicHzCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicHzCmd cmd;

      cmd.request_id = 100;
      cmd.topic_name = "/hz_topic";
      cmd.message_type = "std_msgs/msg/String";
      cmd.window = 20;

      CHECK_EQ(cmd.request_id, 100);
      CHECK_EQ(cmd.topic_name, "/hz_topic");
      CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
      CHECK_EQ(cmd.window, 20);
    }
  }

  TEST_CASE("bridge::TopicBwCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicBwCmd cmd;

      CHECK_EQ(TopicBwCmd::kName, "TopicBwCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.window, 10);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicBwCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicBwCmd cmd;

      cmd.request_id = 200;
      cmd.topic_name = "/bw_topic";
      cmd.message_type = "sensor_msgs/msg/Image";
      cmd.window = 50;

      CHECK_EQ(cmd.request_id, 200);
      CHECK_EQ(cmd.topic_name, "/bw_topic");
      CHECK_EQ(cmd.message_type, "sensor_msgs/msg/Image");
      CHECK_EQ(cmd.window, 50);
    }
  }

  TEST_CASE("bridge::QueryPlayersCmd") {
    QueryPlayersCmd cmd;

    CHECK_EQ(QueryPlayersCmd::kName, "QueryPlayersCmd");
    CHECK_EQ(cmd.request_id, 0);
  }

  TEST_CASE("bridge::TopicEntry") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicEntry entry;

      CHECK(entry.name.empty());
      CHECK(entry.type.empty());
      CHECK_EQ(entry.name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(entry.type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 256> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicEntry entry(&mr);

      CHECK_EQ(entry.name.get_allocator().resource(), &mr);
      CHECK_EQ(entry.type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicEntry entry;

      entry.name = "/topic/test";
      entry.type = "std_msgs/msg/String";

      CHECK_EQ(entry.name, "/topic/test");
      CHECK_EQ(entry.type, "std_msgs/msg/String");
    }
  }

  TEST_CASE("bridge::ServiceEntry") {
    SUBCASE("Default ctor uses default PMR resource") {
      ServiceEntry entry;

      CHECK(entry.name.empty());
      CHECK(entry.type.empty());
      CHECK_EQ(entry.name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(entry.type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 256> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      ServiceEntry entry(&mr);

      CHECK_EQ(entry.name.get_allocator().resource(), &mr);
      CHECK_EQ(entry.type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      ServiceEntry entry;

      entry.name = "/service/test";
      entry.type = "std_srvs/srv/Empty";

      CHECK_EQ(entry.name, "/service/test");
      CHECK_EQ(entry.type, "std_srvs/srv/Empty");
    }
  }

  TEST_CASE("bridge::ActionEntry") {
    SUBCASE("Default ctor uses default PMR resource") {
      ActionEntry entry;

      CHECK(entry.name.empty());
      CHECK(entry.type.empty());
      CHECK_EQ(entry.name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(entry.type.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 256> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      ActionEntry entry(&mr);

      CHECK_EQ(entry.name.get_allocator().resource(), &mr);
      CHECK_EQ(entry.type.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      ActionEntry entry;

      entry.name = "/action/test";
      entry.type = "turtlesim/action/RotateAbsolute";

      CHECK_EQ(entry.name, "/action/test");
      CHECK_EQ(entry.type, "turtlesim/action/RotateAbsolute");
    }
  }

  TEST_CASE("bridge::NodeEntry") {
    SUBCASE("Default ctor uses default PMR resource") {
      NodeEntry entry;

      CHECK(entry.name.empty());
      CHECK_EQ(entry.name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 256> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      NodeEntry entry(&mr);

      CHECK_EQ(entry.name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      NodeEntry entry;

      entry.name = "/turtlesim";

      CHECK_EQ(entry.name, "/turtlesim");
    }
  }

  TEST_CASE("bridge::GraphSnapshotCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      GraphSnapshotCmd cmd;

      CHECK_EQ(GraphSnapshotCmd::kName, "GraphSnapshotCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.nodes.get_allocator().resource(),
               std::pmr::get_default_resource());
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

      CHECK_EQ(cmd.nodes.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.topics.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.services.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.actions.get_allocator().resource(), &mr);
    }
  }

  TEST_CASE("bridge::NodeInfoResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      NodeInfoResponseCmd cmd;

      CHECK_EQ(NodeInfoResponseCmd::kName, "NodeInfoResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_FALSE(cmd.found);
      CHECK_EQ(cmd.node_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.publishers.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.subscribers.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.services.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 1024> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      NodeInfoResponseCmd cmd(&mr);

      CHECK_EQ(cmd.node_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.publishers.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.subscribers.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.services.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      NodeInfoResponseCmd cmd;

      cmd.request_id = 19;
      cmd.node_name = "/node/info";
      cmd.found = true;
      {
        auto& publisher = cmd.publishers.emplace_back(
            cmd.publishers.get_allocator().resource());
        publisher.name = "/topic/a";
        publisher.type = "std_msgs/msg/String";
      }
      {
        auto& subscriber = cmd.subscribers.emplace_back(
            cmd.subscribers.get_allocator().resource());
        subscriber.name = "/topic/b";
        subscriber.type = "std_msgs/msg/Bool";
      }
      {
        auto& service =
            cmd.services.emplace_back(cmd.services.get_allocator().resource());
        service.name = "/service/c";
        service.type = "std_srvs/srv/Trigger";
      }

      CHECK_EQ(cmd.request_id, 19);
      CHECK_EQ(cmd.node_name, "/node/info");
      CHECK(cmd.found);
      CHECK_EQ(cmd.publishers.size(), 1);
      CHECK_EQ(cmd.subscribers.size(), 1);
      CHECK_EQ(cmd.services.size(), 1);
      CHECK_EQ(cmd.publishers.front().name, "/topic/a");
      CHECK_EQ(cmd.subscribers.front().name, "/topic/b");
      CHECK_EQ(cmd.services.front().name, "/service/c");
    }
  }

  TEST_CASE("bridge::TopicInfoResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicInfoResponseCmd cmd;

      CHECK_EQ(TopicInfoResponseCmd::kName, "TopicInfoResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.publisher_count, 0);
      CHECK_EQ(cmd.subscriber_count, 0);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.publisher_nodes.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.subscriber_nodes.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicInfoResponseCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.message_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.publisher_nodes.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.subscriber_nodes.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicInfoResponseCmd cmd;

      cmd.request_id = 20;
      cmd.topic_name = "/topic/info";
      cmd.message_type = "std_msgs/msg/String";
      cmd.publisher_count = 2;
      cmd.subscriber_count = 4;
      cmd.publisher_nodes.emplace_back("/node/pub");
      cmd.subscriber_nodes.emplace_back("/node/sub");

      CHECK_EQ(cmd.request_id, 20);
      CHECK_EQ(cmd.topic_name, "/topic/info");
      CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
      CHECK_EQ(cmd.publisher_count, 2);
      CHECK_EQ(cmd.subscriber_count, 4);
      CHECK_EQ(cmd.publisher_nodes.size(), 1);
      CHECK_EQ(cmd.subscriber_nodes.size(), 1);
      CHECK_EQ(cmd.publisher_nodes.front(), "/node/pub");
      CHECK_EQ(cmd.subscriber_nodes.front(), "/node/sub");
    }
  }

  TEST_CASE("bridge::ServiceInfoResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      ServiceInfoResponseCmd cmd;

      CHECK_EQ(ServiceInfoResponseCmd::kName, "ServiceInfoResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.client_count, 0);
      CHECK_EQ(cmd.server_count, 0);
      CHECK_EQ(cmd.service_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.service_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.client_nodes.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.server_nodes.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      ServiceInfoResponseCmd cmd(&mr);

      CHECK_EQ(cmd.service_name.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.service_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.client_nodes.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.server_nodes.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      ServiceInfoResponseCmd cmd;

      cmd.request_id = 21;
      cmd.service_name = "/service/info";
      cmd.service_type = "std_srvs/srv/Empty";
      cmd.client_count = 4U;
      cmd.server_count = 1;
      cmd.client_nodes.emplace_back("/node/client");
      cmd.server_nodes.emplace_back("/node/server");

      CHECK_EQ(cmd.request_id, 21);
      CHECK_EQ(cmd.service_name, "/service/info");
      CHECK_EQ(cmd.service_type, "std_srvs/srv/Empty");
      CHECK_EQ(cmd.client_count, 4U);
      CHECK_EQ(cmd.server_count, 1);
      CHECK_EQ(cmd.client_nodes.size(), 1);
      CHECK_EQ(cmd.server_nodes.size(), 1);
      CHECK_EQ(cmd.client_nodes.front(), "/node/client");
      CHECK_EQ(cmd.server_nodes.front(), "/node/server");
    }
  }

  TEST_CASE("bridge::InterfaceListResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      InterfaceListResponseCmd cmd;

      CHECK_EQ(InterfaceListResponseCmd::kName, "InterfaceListResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.messages.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.services.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.actions.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 1024> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      InterfaceListResponseCmd cmd(&mr);

      CHECK_EQ(cmd.messages.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.services.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.actions.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      InterfaceListResponseCmd cmd;

      cmd.request_id = 22;
      cmd.messages.emplace_back("std_msgs/msg/String");
      cmd.services.emplace_back("std_srvs/srv/Trigger");
      cmd.actions.emplace_back("example_interfaces/action/Fibonacci");

      CHECK_EQ(cmd.request_id, 22);
      CHECK_EQ(cmd.messages.size(), 1);
      CHECK_EQ(cmd.services.size(), 1);
      CHECK_EQ(cmd.actions.size(), 1);
      CHECK_EQ(cmd.messages.front(), "std_msgs/msg/String");
      CHECK_EQ(cmd.services.front(), "std_srvs/srv/Trigger");
      CHECK_EQ(cmd.actions.front(), "example_interfaces/action/Fibonacci");
    }
  }

  TEST_CASE("bridge::InterfaceShowResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      InterfaceShowResponseCmd cmd;

      CHECK_EQ(InterfaceShowResponseCmd::kName, "InterfaceShowResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_FALSE(cmd.found);
      CHECK_EQ(cmd.interface_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.definition.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      InterfaceShowResponseCmd cmd(&mr);

      CHECK_EQ(cmd.interface_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.definition.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      InterfaceShowResponseCmd cmd;

      cmd.request_id = 22;
      cmd.interface_type = "std_msgs/msg/String";
      cmd.definition = "string data\n";
      cmd.found = true;

      CHECK_EQ(cmd.request_id, 22);
      CHECK_EQ(cmd.interface_type, "std_msgs/msg/String");
      CHECK_EQ(cmd.definition, "string data\n");
      CHECK(cmd.found);
    }
  }

  TEST_CASE("bridge::TopicPayloadCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicPayloadCmd cmd;

      CHECK_EQ(TopicPayloadCmd::kName, "TopicPayloadCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.message_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_FALSE(cmd.raw);
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

    SUBCASE("Fields can be set and read") {
      TopicPayloadCmd cmd;

      cmd.request_id = 41;
      cmd.topic_name = "/topic/echo";
      cmd.message_type = "std_msgs/msg/String";
      cmd.raw = true;
      cmd.payload.assign({1, 2, 4});

      CHECK_EQ(cmd.request_id, 41);
      CHECK_EQ(cmd.topic_name, "/topic/echo");
      CHECK_EQ(cmd.message_type, "std_msgs/msg/String");
      CHECK(cmd.raw);
      CHECK_EQ(cmd.payload.size(), 3);
      CHECK_EQ(cmd.payload[0], 1);
      CHECK_EQ(cmd.payload[1], 2);
      CHECK_EQ(cmd.payload[2], 4);
    }
  }

  TEST_CASE("bridge::TopicHzResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicHzResponseCmd cmd;

      CHECK_EQ(TopicHzResponseCmd::kName, "TopicHzResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.frequency, 0.0);
      CHECK_EQ(cmd.window, 0);
      CHECK_EQ(cmd.message_count, 0);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicHzResponseCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicHzResponseCmd cmd;

      cmd.request_id = 300;
      cmd.topic_name = "/hz_result";
      cmd.frequency = 42.5;
      cmd.window = 10;
      cmd.message_count = 11;

      CHECK_EQ(cmd.request_id, 300);
      CHECK_EQ(cmd.topic_name, "/hz_result");
      CHECK_EQ(cmd.frequency, doctest::Approx(42.5));
      CHECK_EQ(cmd.window, 10);
      CHECK_EQ(cmd.message_count, 11);
    }
  }

  TEST_CASE("bridge::TopicBwResponseCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      TopicBwResponseCmd cmd;

      CHECK_EQ(TopicBwResponseCmd::kName, "TopicBwResponseCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.bytes_per_second, 0.0);
      CHECK_EQ(cmd.window, 0);
      CHECK_EQ(cmd.message_count, 0);
      CHECK_EQ(cmd.total_bytes, 0LL);
      CHECK_EQ(cmd.topic_name.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      TopicBwResponseCmd cmd(&mr);

      CHECK_EQ(cmd.topic_name.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      TopicBwResponseCmd cmd;

      cmd.request_id = 400;
      cmd.topic_name = "/bw_result";
      cmd.bytes_per_second = 1024.0;
      cmd.window = 20;
      cmd.message_count = 21;
      cmd.total_bytes = 65536;

      CHECK_EQ(cmd.request_id, 400);
      CHECK_EQ(cmd.topic_name, "/bw_result");
      CHECK_EQ(cmd.bytes_per_second, doctest::Approx(1024.0));
      CHECK_EQ(cmd.window, 20);
      CHECK_EQ(cmd.message_count, 21);
      CHECK_EQ(cmd.total_bytes, 65536);
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
      CHECK_EQ(cmd.request_id, 0);
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

  TEST_CASE("bridge::ErrorCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      ErrorCmd cmd;

      CHECK_EQ(ErrorCmd::kName, "ErrorCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_EQ(cmd.error_code.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.error_message.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      ErrorCmd cmd(&mr);

      CHECK_EQ(cmd.error_code.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.error_message.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      ErrorCmd cmd;

      cmd.request_id = 42;
      cmd.error_code = "SUBSCRIBE_FAILED";
      cmd.error_message = "Message type is not of the form package/type";

      CHECK_EQ(cmd.request_id, 42);
      CHECK_EQ(cmd.error_code, "SUBSCRIBE_FAILED");
      CHECK_EQ(cmd.error_message,
               "Message type is not of the form package/type");
    }
  }

  TEST_CASE("bridge::AddonEventCmd") {
    SUBCASE("Default ctor uses default PMR resource") {
      AddonEventCmd cmd;

      CHECK_EQ(AddonEventCmd::kName, "AddonEventCmd");
      CHECK_EQ(cmd.request_id, 0);
      CHECK_FALSE(cmd.response);
      CHECK_EQ(cmd.addon_id.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.event_type.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.encoding.get_allocator().resource(),
               std::pmr::get_default_resource());
      CHECK_EQ(cmd.payload.get_allocator().resource(),
               std::pmr::get_default_resource());
    }

    SUBCASE("Explicit ctor uses provided PMR resource") {
      std::array<std::byte, 512> buffer{};
      std::pmr::monotonic_buffer_resource mr(buffer.data(), buffer.size());

      AddonEventCmd cmd(&mr);

      CHECK_EQ(cmd.addon_id.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.event_type.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.encoding.get_allocator().resource(), &mr);
      CHECK_EQ(cmd.payload.get_allocator().resource(), &mr);
    }

    SUBCASE("Fields can be set and read") {
      AddonEventCmd cmd;

      cmd.request_id = 42;
      cmd.addon_id = "ping";
      cmd.event_type = "hello";
      cmd.encoding = "json";
      cmd.response = true;
      cmd.payload = {0x01, 0x02, 0x03};

      CHECK_EQ(cmd.request_id, 42);
      CHECK_EQ(cmd.addon_id, "ping");
      CHECK_EQ(cmd.event_type, "hello");
      CHECK_EQ(cmd.encoding, "json");
      CHECK(cmd.response);
      CHECK_EQ(cmd.payload.size(), 3U);
      CHECK_EQ(cmd.payload[0], 0x01);
      CHECK_EQ(cmd.payload[1], 0x02);
      CHECK_EQ(cmd.payload[2], 0x03);
    }
  }
}
