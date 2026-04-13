#include <doctest/doctest.h>

#include <roscraft/bridge/jni/config.hpp>

#include "support/fake_jni.hpp"

using namespace roscraft::bridge::jni;

TEST_SUITE("bridge::jni::BridgeConfig") {
  TEST_CASE("bridge::jni::BridgeConfig::Valid") {
    SUBCASE("Default configuration is invalid") {
      constexpr BridgeConfig config;

      CHECK_EQ(config.jvm, nullptr);
      CHECK_FALSE(config.Valid());
    }

    SUBCASE("Configuration with JVM pointer is valid") {
      tests::FakeJavaVM fake_vm;
      const BridgeConfig config{.jvm = fake_vm.Vm()};

      CHECK_NE(config.jvm, nullptr);
      CHECK(config.Valid());
    }
  }
}
