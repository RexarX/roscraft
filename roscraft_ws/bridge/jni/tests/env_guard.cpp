#include <doctest/doctest.h>

#include <roscraft/bridge/jni/env_guard.hpp>

#include "support/fake_jni.hpp"

using namespace roscraft::bridge::jni;

TEST_SUITE("bridge::jni::JniEnvGuard") {
  TEST_CASE("bridge::jni::JniEnvGuard::JniEnvGuard") {
    SUBCASE("Null JVM keeps guard invalid") {
      const JniEnvGuard guard(nullptr);

      CHECK_FALSE(guard.Valid());
      CHECK_EQ(guard.Env(), nullptr);
    }

    SUBCASE("Already-attached thread reuses existing JNIEnv") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_OK;

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK(guard.Valid());
      CHECK_EQ(guard.Env(), fake_env.Env());
      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 0);
      CHECK_EQ(fake_vm.last_get_env_version, JNI_VERSION_1_8);
    }

    SUBCASE("Detached thread attaches successfully") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_OK;

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK(guard.Valid());
      CHECK_EQ(guard.Env(), fake_env.Env());
      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 1);
      CHECK_EQ(fake_vm.last_get_env_version, JNI_VERSION_1_8);
      CHECK_EQ(fake_vm.last_attach_version, JNI_VERSION_1_8);
      CHECK(fake_vm.last_attach_name.empty());
      CHECK_EQ(fake_vm.last_attach_group, nullptr);
    }

    SUBCASE("Attach failure keeps guard invalid") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK_FALSE(guard.Valid());
      CHECK_EQ(guard.Env(), nullptr);
      CHECK_EQ(fake_vm.get_env_calls, 1);
      CHECK_EQ(fake_vm.attach_calls, 1);
    }
  }

  TEST_CASE("bridge::jni::JniEnvGuard::~JniEnvGuard") {
    SUBCASE("Does not detach when thread was already attached") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_OK;

      {
        const JniEnvGuard guard(fake_vm.Vm());
        CHECK(guard.Valid());
      }

      CHECK_EQ(fake_vm.detach_calls, 0);
    }

    SUBCASE("Detaches when thread was newly attached") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_OK;

      {
        const JniEnvGuard guard(fake_vm.Vm());
        CHECK(guard.Valid());
      }

      CHECK_EQ(fake_vm.detach_calls, 1);
    }

    SUBCASE("Does not detach when attach failed") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;

      {
        const JniEnvGuard guard(fake_vm.Vm());
        CHECK_FALSE(guard.Valid());
      }

      CHECK_EQ(fake_vm.detach_calls, 0);
    }
  }

  TEST_CASE("bridge::jni::JniEnvGuard::Env") {
    SUBCASE("Env returns attached JNI environment") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK_EQ(guard.Env(), fake_env.Env());
    }

    SUBCASE("Env returns nullptr when no environment is available") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK_EQ(guard.Env(), nullptr);
    }
  }

  TEST_CASE("bridge::jni::JniEnvGuard::Valid") {
    SUBCASE("Valid is true when Env is available") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK(guard.Valid());
    }

    SUBCASE("Valid is false when Env is unavailable") {
      tests::FakeJniEnv fake_env;
      tests::FakeJavaVM fake_vm;
      fake_vm.Bind(fake_env);
      fake_vm.get_env_result = JNI_EDETACHED;
      fake_vm.attach_result = JNI_ERR;

      const JniEnvGuard guard(fake_vm.Vm());

      CHECK_FALSE(guard.Valid());
    }
  }
}
