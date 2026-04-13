#include <doctest/doctest.h>

#include <roscraft/bridge/network/config.hpp>

#include <cstdint>

using namespace roscraft::bridge::network;

TEST_SUITE("bridge::network::BridgeConfig") {
  TEST_CASE("bridge::network::BridgeConfig defaults") {
    constexpr BridgeConfig config;

    CHECK_EQ(BridgeConfig::kDefaultHost.View(), "127.0.0.1");
    CHECK_EQ(BridgeConfig::kDefaultPort, static_cast<uint16_t>(7401));

    CHECK_EQ(config.host.View(), BridgeConfig::kDefaultHost.View());
    CHECK_EQ(config.port, BridgeConfig::kDefaultPort);
    CHECK_FALSE(config.allow_multiple_connections);
  }

  TEST_CASE("bridge::network::BridgeConfig::From") {
    SUBCASE("Returns defaults when no command-line args are provided") {
      char arg0[] = "roscraft_bridge_network";
      char* argv[] = {arg0};

      const BridgeConfig config = BridgeConfig::From(1, argv);

      CHECK_EQ(config.host.View(), BridgeConfig::kDefaultHost.View());
      CHECK_EQ(config.port, BridgeConfig::kDefaultPort);
      CHECK_FALSE(config.allow_multiple_connections);
    }

    SUBCASE("Parses host, port and allow-multiple-connections flag") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--host";
      char arg2[] = "127.0.0.42";
      char arg3[] = "--port";
      char arg4[] = "8401";
      char arg5[] = "--allow-multiple-connections";
      char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

      const BridgeConfig config = BridgeConfig::From(6, argv);

      CHECK_EQ(config.host.View(), "127.0.0.42");
      CHECK_EQ(config.port, static_cast<uint16_t>(8401));
      CHECK(config.allow_multiple_connections);
    }

    SUBCASE("Parses short-form host and port flags") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "-h";
      char arg2[] = "127.0.0.7";
      char arg3[] = "-p";
      char arg4[] = "7402";
      char* argv[] = {arg0, arg1, arg2, arg3, arg4};

      const BridgeConfig config = BridgeConfig::From(5, argv);

      CHECK_EQ(config.host.View(), "127.0.0.7");
      CHECK_EQ(config.port, static_cast<uint16_t>(7402));
      CHECK_FALSE(config.allow_multiple_connections);
    }

    SUBCASE("Parses allow-multiple-connections with explicit host and port") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--host";
      char arg2[] = "127.0.0.9";
      char arg3[] = "--port";
      char arg4[] = "7411";
      char arg5[] = "--allow-multiple-connections";
      char* argv[] = {arg0, arg1, arg2, arg3, arg4, arg5};

      const BridgeConfig config = BridgeConfig::From(6, argv);

      CHECK_EQ(config.host.View(), "127.0.0.9");
      CHECK_EQ(config.port, static_cast<uint16_t>(7411));
      CHECK(config.allow_multiple_connections);
    }

    SUBCASE("Parses upper-bound port value") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--port";
      char arg2[] = "65535";
      char* argv[] = {arg0, arg1, arg2};

      const BridgeConfig config = BridgeConfig::From(3, argv);

      CHECK_EQ(config.port, static_cast<uint16_t>(65535));
    }

    SUBCASE("Returns defaults when port parsing fails") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--port";
      char arg2[] = "not-a-port";
      char* argv[] = {arg0, arg1, arg2};

      const BridgeConfig config = BridgeConfig::From(3, argv);

      CHECK_EQ(config.host.View(), BridgeConfig::kDefaultHost.View());
      CHECK_EQ(config.port, BridgeConfig::kDefaultPort);
      CHECK_FALSE(config.allow_multiple_connections);
    }

    SUBCASE("Returns defaults when port is out of uint16 range") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--port";
      char arg2[] = "65536";
      char* argv[] = {arg0, arg1, arg2};

      const BridgeConfig config = BridgeConfig::From(3, argv);

      CHECK_EQ(config.host.View(), BridgeConfig::kDefaultHost.View());
      CHECK_EQ(config.port, BridgeConfig::kDefaultPort);
      CHECK_FALSE(config.allow_multiple_connections);
    }

    SUBCASE("Returns defaults when unknown argument is provided") {
      char arg0[] = "roscraft_bridge_network";
      char arg1[] = "--unknown";
      char* argv[] = {arg0, arg1};

      const BridgeConfig config = BridgeConfig::From(2, argv);

      CHECK_EQ(config.host.View(), BridgeConfig::kDefaultHost.View());
      CHECK_EQ(config.port, BridgeConfig::kDefaultPort);
      CHECK_FALSE(config.allow_multiple_connections);
    }
  }
}
