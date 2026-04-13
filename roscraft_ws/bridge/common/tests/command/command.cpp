#include <doctest/doctest.h>

#include <roscraft/bridge/command/command.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>

using namespace roscraft::bridge;

namespace {

struct BasicCmd {
  int value = 0;
};

struct NamedCmd {
  static constexpr std::string_view kName = "NamedCmd";

  int value = 0;
};

struct NonDefaultCmd {
  explicit NonDefaultCmd(int) {}
};

struct NonMovableCmd {
  NonMovableCmd() = default;
  NonMovableCmd(const NonMovableCmd&) = default;
  NonMovableCmd(NonMovableCmd&&) = delete;
  ~NonMovableCmd() = default;

  NonMovableCmd& operator=(const NonMovableCmd&) = default;
  NonMovableCmd& operator=(NonMovableCmd&&) = delete;
};

static_assert(CommandTrait<BasicCmd>);
static_assert(CommandTrait<NamedCmd>);
static_assert(!CommandTrait<NonDefaultCmd>);
static_assert(!CommandTrait<NonMovableCmd>);
static_assert(!CommandTrait<int&>);

static_assert(!CommandWithNameTrait<BasicCmd>);
static_assert(CommandWithNameTrait<NamedCmd>);

}  // namespace

TEST_SUITE("bridge::CommandType") {
  TEST_CASE("bridge::CommandTypeIndex alias") {
    CHECK_EQ(CommandTypeIndex::From<BasicCmd>(),
             roscraft::utils::TypeIndex::From<BasicCmd>());
  }

  TEST_CASE("bridge::CommandTrait") {
    CHECK(CommandTrait<BasicCmd>);
    CHECK(CommandTrait<NamedCmd>);
    CHECK_FALSE(CommandTrait<NonDefaultCmd>);
    CHECK_FALSE(CommandTrait<NonMovableCmd>);
    CHECK_FALSE(CommandTrait<int&>);
  }

  TEST_CASE("bridge::CommandWithNameTrait") {
    CHECK_FALSE(CommandWithNameTrait<BasicCmd>);
    CHECK(CommandWithNameTrait<NamedCmd>);
  }

  TEST_CASE("bridge::CommandNameOf (template)") {
    SUBCASE("Returns custom name when kName is present") {
      constexpr std::string_view name = CommandNameOf<NamedCmd>();
      CHECK_EQ(name, "NamedCmd");
    }

    SUBCASE("Falls back to type name when kName is absent") {
      constexpr std::string_view name = CommandNameOf<BasicCmd>();
      CHECK_EQ(name, "BasicCmd");
    }
  }

  TEST_CASE("bridge::CommandNameOf (instance overload)") {
    SUBCASE("Uses custom name from instance type") {
      constexpr std::string_view name = CommandNameOf(NamedCmd{});
      CHECK_EQ(name, "NamedCmd");
    }

    SUBCASE("Uses type name for unnamed instance type") {
      constexpr std::string_view name = CommandNameOf(BasicCmd{});
      CHECK_EQ(name, "BasicCmd");
    }
  }
}
