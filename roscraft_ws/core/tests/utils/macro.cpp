#include <doctest/doctest.h>

#include <roscraft/utils/macro.hpp>

#include <array>
#include <string>

TEST_SUITE("utils::Macro") {
  TEST_CASE("utils::ROSCRAFT_BIT: bit shifting macro") {
    SUBCASE("Bit 0") {
      CHECK_EQ(ROSCRAFT_BIT(0), 1);
    }

    SUBCASE("Bit 1") {
      CHECK_EQ(ROSCRAFT_BIT(1), 2);
    }

    SUBCASE("Bit 2") {
      CHECK_EQ(ROSCRAFT_BIT(2), 4);
    }

    SUBCASE("Bit 3") {
      CHECK_EQ(ROSCRAFT_BIT(3), 8);
    }

    SUBCASE("Bit 4") {
      CHECK_EQ(ROSCRAFT_BIT(4), 16);
    }

    SUBCASE("Bit 7") {
      CHECK_EQ(ROSCRAFT_BIT(7), 128);
    }

    SUBCASE("Bit 8") {
      CHECK_EQ(ROSCRAFT_BIT(8), 256);
    }

    SUBCASE("Bit 15") {
      CHECK_EQ(ROSCRAFT_BIT(15), 32768);
    }

    SUBCASE("Bit 16") {
      CHECK_EQ(ROSCRAFT_BIT(16), 65536);
    }

    SUBCASE("Power of two relationship") {
      for (int i = 0; i < 16; ++i) {
        CHECK_EQ(ROSCRAFT_BIT(i), 1 << i);
      }
    }

    SUBCASE("Usage in bitmask") {
      constexpr int kFlagA = ROSCRAFT_BIT(0);
      constexpr int kFlagB = ROSCRAFT_BIT(1);
      constexpr int kFlagC = ROSCRAFT_BIT(2);

      int flags = kFlagA | kFlagC;

      CHECK((flags & kFlagA) != 0);
      CHECK((flags & kFlagB) == 0);
      CHECK((flags & kFlagC) != 0);
    }
  }

  TEST_CASE("utils::ROSCRAFT_STRINGIFY: stringification macro") {
    SUBCASE("Stringify integer literal") {
      const char* str = ROSCRAFT_STRINGIFY(42);
      CHECK_EQ(std::string(str), "42");
    }

    SUBCASE("Stringify identifier") {
      const char* str = ROSCRAFT_STRINGIFY(hello_world);
      CHECK_EQ(std::string(str), "hello_world");
    }

    SUBCASE("Stringify expression") {
      const char* str = ROSCRAFT_STRINGIFY(1 + 2);
      CHECK_EQ(std::string(str), "1 + 2");
    }

    SUBCASE("Stringify type") {
      const char* str = ROSCRAFT_STRINGIFY(int);
      CHECK_EQ(std::string(str), "int");
    }

    SUBCASE("Stringify template-like syntax") {
      const char* str = ROSCRAFT_STRINGIFY(std::vector<int>);
      CHECK_EQ(std::string(str), "std::vector<int>");
    }

    SUBCASE("Stringify with parentheses") {
      const char* str = ROSCRAFT_STRINGIFY((a, b, c));
      CHECK_EQ(std::string(str), "(a, b, c)");
    }

    SUBCASE("Stringify macro argument") {
#define TEST_VALUE 123
      const char* str = ROSCRAFT_STRINGIFY(TEST_VALUE);
      // ROSCRAFT_STRINGIFY should expand the macro first via
      // ROSCRAFT_STRINGIFY_IMPL
      CHECK_EQ(std::string(str), "123");
#undef TEST_VALUE
    }
  }

  TEST_CASE("utils::ROSCRAFT_CONCAT: concatenation macro") {
    SUBCASE("Concatenate identifiers to form variable name") {
      // ROSCRAFT_CONCAT joins tokens together
      int ROSCRAFT_CONCAT(test_, var) = 42;
      CHECK_EQ(test_var, 42);
    }

    SUBCASE("Concatenate to form function name") {
      auto ROSCRAFT_CONCAT(get_, value) = []() { return 100; };
      CHECK_EQ(get_value(), 100);
    }

    SUBCASE("Concatenate numbers") {
      constexpr int ROSCRAFT_CONCAT(var, 1) = 10;
      constexpr int ROSCRAFT_CONCAT(var, 2) = 20;
      constexpr int ROSCRAFT_CONCAT(var, 3) = 30;

      CHECK_EQ(var1, 10);
      CHECK_EQ(var2, 20);
      CHECK_EQ(var3, 30);
    }

    SUBCASE("Concatenate with underscore") {
      int ROSCRAFT_CONCAT(my, _variable) = 99;
      CHECK_EQ(my_variable, 99);
    }
  }

  TEST_CASE("utils::ROSCRAFT_ANONYMOUS_VAR: anonymous variable generation") {
    SUBCASE("Creates unique variables on different lines") {
      // Each ROSCRAFT_ANONYMOUS_VAR on a different line should create a unique
      // variable
      [[maybe_unused]] int ROSCRAFT_ANONYMOUS_VAR(test_) = 1;
      [[maybe_unused]] int ROSCRAFT_ANONYMOUS_VAR(test_) = 2;
      [[maybe_unused]] int ROSCRAFT_ANONYMOUS_VAR(test_) = 3;

      // If they were the same name, this wouldn't compile
      CHECK(true);
    }

    SUBCASE("Variable is usable") {
      int ROSCRAFT_ANONYMOUS_VAR(counter_) = 42;
      // We can use the variable by knowing the line number, but typically
      // anonymous variables are meant to be unused after initialization
      CHECK(true);
    }

    SUBCASE("Works with different prefixes") {
      [[maybe_unused]] int ROSCRAFT_ANONYMOUS_VAR(a_) = 1;
      [[maybe_unused]] float ROSCRAFT_ANONYMOUS_VAR(b_) = 2.0f;
      [[maybe_unused]] double ROSCRAFT_ANONYMOUS_VAR(c_) = 3.0;

      CHECK(true);
    }

    SUBCASE("Useful for RAII guards") {
      int counter = 0;

      struct Guard {
        int& ref;
        explicit Guard(int& r) : ref(r) { ++ref; }
        ~Guard() { ++ref; }
      };

      CHECK_EQ(counter, 0);
      {
        [[maybe_unused]] Guard ROSCRAFT_ANONYMOUS_VAR(guard_)(counter);
        CHECK_EQ(counter, 1);
      }
      CHECK_EQ(counter, 2);
    }
  }

  TEST_CASE("utils::ROSCRAFT_BIT: constexpr usage") {
    SUBCASE("Can be used in constexpr context") {
      constexpr int bit0 = ROSCRAFT_BIT(0);
      constexpr int bit5 = ROSCRAFT_BIT(5);
      constexpr int bit10 = ROSCRAFT_BIT(10);

      static_assert(bit0 == 1, "Bit 0 should be 1");
      static_assert(bit5 == 32, "Bit 5 should be 32");
      static_assert(bit10 == 1024, "Bit 10 should be 1024");

      CHECK_EQ(bit0, 1);
      CHECK_EQ(bit5, 32);
      CHECK_EQ(bit10, 1024);
    }

    SUBCASE("Can be used in template arguments") {
      std::array<int, ROSCRAFT_BIT(3)> arr;
      CHECK_EQ(arr.size(), 8);
    }

    SUBCASE("Can be used in switch case") {
      int value = 4;
      int result = 0;

      switch (value) {
        case ROSCRAFT_BIT(0):
          result = 1;
          break;
        case ROSCRAFT_BIT(1):
          result = 2;
          break;
        case ROSCRAFT_BIT(2):
          result = 3;
          break;
        default:
          result = 0;
          break;
      }

      CHECK_EQ(result, 3);
    }
  }

  TEST_CASE("utils::Macro combinations") {
    SUBCASE("STRINGIFY and CONCAT together") {
      const char* str = ROSCRAFT_STRINGIFY(ROSCRAFT_CONCAT(hello, _world));
      // The inner CONCAT should be expanded first
      CHECK_EQ(std::string(str), "hello_world");
    }

    SUBCASE("BIT in expressions") {
      constexpr int flags = ROSCRAFT_BIT(0) | ROSCRAFT_BIT(2) | ROSCRAFT_BIT(4);
      CHECK_EQ(flags, 1 + 4 + 16);
      CHECK_EQ(flags, 21);
    }
  }

  TEST_CASE("utils::ROSCRAFT_STRINGIFY_IMPL: direct usage") {
    SUBCASE("Stringify without macro expansion") {
      const char* str = ROSCRAFT_STRINGIFY_IMPL(test);
      CHECK_EQ(std::string(str), "test");
    }
  }

  TEST_CASE("utils::ROSCRAFT_CONCAT_IMPL: direct usage") {
    SUBCASE("Concatenate directly") {
      int ROSCRAFT_CONCAT_IMPL(direct_, concat) = 999;
      CHECK_EQ(direct_concat, 999);
    }
  }

}  // TEST_SUITE
