#include <doctest/doctest.h>

#include <roscraft/bridge/jni/command/callback.hpp>

#include <array>
#include <span>

#include "../support/fake_jni.hpp"

using namespace roscraft::bridge::jni;

TEST_SUITE("bridge::jni::BridgeCallback") {
  TEST_CASE("bridge::jni::BridgeCallback::Init") {
    tests::FakeJniEnv fake_env;
    BridgeCallback callback;

    callback.Init(fake_env.Env(), fake_env.callback_object);

    CHECK(callback.Valid());
    CHECK_EQ(fake_env.new_global_ref_calls, 1);
    CHECK_EQ(fake_env.get_object_class_calls, 1);
    CHECK_EQ(fake_env.get_method_id_calls, 1);
    CHECK_EQ(fake_env.last_method_name, "onPacket");
    CHECK_EQ(fake_env.last_method_signature, "([B)V");
    CHECK_EQ(fake_env.delete_local_ref_calls, 1);
    CHECK_EQ(fake_env.deleted_local_refs.size(), 1U);
    CHECK_EQ(fake_env.deleted_local_refs[0], fake_env.callback_class);
  }

  TEST_CASE("bridge::jni::BridgeCallback::Destroy") {
    tests::FakeJniEnv fake_env;
    BridgeCallback callback;

    callback.Init(fake_env.Env(), fake_env.callback_object);
    REQUIRE(callback.Valid());

    callback.Destroy(fake_env.Env());

    CHECK_FALSE(callback.Valid());
    CHECK_EQ(fake_env.delete_global_ref_calls, 1);
    CHECK_EQ(fake_env.deleted_global_refs.size(), 1U);
    CHECK_EQ(fake_env.deleted_global_refs[0], fake_env.callback_object);

    callback.Destroy(fake_env.Env());
    CHECK_EQ(fake_env.delete_global_ref_calls, 1);
  }

  TEST_CASE("bridge::jni::BridgeCallback::SendPacket") {
    tests::FakeJniEnv fake_env;
    BridgeCallback callback;

    callback.Init(fake_env.Env(), fake_env.callback_object);
    REQUIRE(callback.Valid());

    SUBCASE("Sends non-empty packet bytes to callback") {
      const std::array<uint8_t, 4> packet{9U, 8U, 7U, 6U};

      callback.SendPacket(fake_env.Env(), packet);

      CHECK_EQ(fake_env.new_byte_array_calls, 1);
      CHECK_EQ(fake_env.set_byte_array_region_calls, 1);
      CHECK_EQ(fake_env.call_void_method_calls, 1);
      CHECK_EQ(fake_env.delete_local_ref_calls, 2);
      CHECK_EQ(fake_env.callback_packets.size(), 1U);
      CHECK(fake_env.callback_packets[0] ==
            std::vector<uint8_t>(packet.begin(), packet.end()));
    }

    SUBCASE("Sends empty packet without writing byte region") {
      callback.SendPacket(fake_env.Env(), std::span<const uint8_t>{});

      CHECK_EQ(fake_env.new_byte_array_calls, 1);
      CHECK_EQ(fake_env.set_byte_array_region_calls, 0);
      CHECK_EQ(fake_env.call_void_method_calls, 1);
      CHECK_EQ(fake_env.callback_packets.size(), 1U);
      CHECK(fake_env.callback_packets[0].empty());
    }

    SUBCASE("Skips callback invocation when byte-array allocation fails") {
      fake_env.fail_new_byte_array = true;

      const std::array<uint8_t, 2> packet{1U, 2U};
      callback.SendPacket(fake_env.Env(), packet);

      CHECK_EQ(fake_env.new_byte_array_calls, 1);
      CHECK_EQ(fake_env.call_void_method_calls, 0);
      CHECK_EQ(fake_env.set_byte_array_region_calls, 0);
    }
  }

  TEST_CASE("bridge::jni::BridgeCallback::Valid") {
    tests::FakeJniEnv fake_env;
    BridgeCallback callback;

    CHECK_FALSE(callback.Valid());

    callback.Init(fake_env.Env(), fake_env.callback_object);
    CHECK(callback.Valid());

    callback.Destroy(fake_env.Env());
    CHECK_FALSE(callback.Valid());
  }
}
