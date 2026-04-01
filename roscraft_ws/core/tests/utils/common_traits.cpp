#include <doctest/doctest.h>

#include <roscraft/utils/common_traits.hpp>

#include <string>

namespace {

// Test types for PolymorphicConvertible
struct Base {
  virtual ~Base() = default;
  virtual int Value() const { return 1; }
};

struct Derived : Base {
  int Value() const override { return 2; }
};

struct Unrelated {
  virtual ~Unrelated() = default;
};

struct NonPolymorphic {
  int value = 0;
};

struct AnotherNonPolymorphic {
  float data = 0.0f;
};

}  // namespace

TEST_SUITE("utils::CommonTraits") {
  TEST_CASE("utils::ArithmeticTrait: concept validation") {
    SUBCASE("Integral types") {
      CHECK(roscraft::utils::ArithmeticTrait<int>);
      CHECK(roscraft::utils::ArithmeticTrait<unsigned long>);
      CHECK(roscraft::utils::ArithmeticTrait<short>);
      CHECK(roscraft::utils::ArithmeticTrait<long long>);
    }

    SUBCASE("Floating point types") {
      CHECK(roscraft::utils::ArithmeticTrait<float>);
      CHECK(roscraft::utils::ArithmeticTrait<double>);
      CHECK(roscraft::utils::ArithmeticTrait<long double>);
    }

    SUBCASE("Non-arithmetic types") {
      CHECK_FALSE(roscraft::utils::ArithmeticTrait<std::string>);
      CHECK_FALSE(roscraft::utils::ArithmeticTrait<void*>);
      CHECK_FALSE(roscraft::utils::ArithmeticTrait<std::string>);
    }
  }

  TEST_CASE("utils::UniqueTypes: concept validation") {
    SUBCASE("Basic usage") {
      CHECK(roscraft::utils::UniqueTypes<int>);
      CHECK(roscraft::utils::UniqueTypes<int, char>);
    }

    SUBCASE("Const and reference types") {
      CHECK(roscraft::utils::UniqueTypes<int&, char>);
      CHECK(roscraft::utils::UniqueTypes<const int, char>);
      CHECK(roscraft::utils::UniqueTypes<const int&, char>);

      CHECK_FALSE(roscraft::utils::UniqueTypes<int&, int>);
      CHECK_FALSE(roscraft::utils::UniqueTypes<const int, int>);
      CHECK_FALSE(roscraft::utils::UniqueTypes<const int&, int>);
    }
  }

  TEST_CASE("utils::PolymorphicConvertible: concept validation") {
    SUBCASE("Standard convertible types") {
      // Types that are std::convertible_to should satisfy
      // PolymorphicConvertible
      CHECK(roscraft::utils::PolymorphicConvertible<int, double>);
      CHECK(roscraft::utils::PolymorphicConvertible<float, double>);
      CHECK(roscraft::utils::PolymorphicConvertible<int, long>);
      CHECK(roscraft::utils::PolymorphicConvertible<short, int>);
    }

    SUBCASE("Same types") {
      CHECK(roscraft::utils::PolymorphicConvertible<int, int>);
      CHECK(roscraft::utils::PolymorphicConvertible<double, double>);
      CHECK(roscraft::utils::PolymorphicConvertible<Base, Base>);
    }

    SUBCASE("Polymorphic: derived to base") {
      CHECK(roscraft::utils::PolymorphicConvertible<Derived&, Base&>);
      CHECK(roscraft::utils::PolymorphicConvertible<Derived*, Base*>);
      CHECK(roscraft::utils::PolymorphicConvertible<Derived, Base>);
    }

    SUBCASE("Polymorphic: base to derived (allowed by concept)") {
      // PolymorphicConvertible allows both directions for polymorphic types
      CHECK(roscraft::utils::PolymorphicConvertible<Base&, Derived&>);
      CHECK(roscraft::utils::PolymorphicConvertible<Base, Derived>);
    }

    SUBCASE("Unrelated polymorphic types") {
      // Unrelated types should not be convertible
      CHECK_FALSE(roscraft::utils::PolymorphicConvertible<Base, Unrelated>);
      CHECK_FALSE(roscraft::utils::PolymorphicConvertible<Derived, Unrelated>);
      CHECK_FALSE(roscraft::utils::PolymorphicConvertible<Unrelated, Base>);
    }

    SUBCASE("Non-polymorphic types that are not convertible") {
      CHECK_FALSE(roscraft::utils::PolymorphicConvertible<std::string, int>);
      CHECK_FALSE(
          roscraft::utils::PolymorphicConvertible<NonPolymorphic,
                                                  AnotherNonPolymorphic>);
    }

    SUBCASE("Reference types with polymorphism") {
      CHECK(roscraft::utils::PolymorphicConvertible<Derived&, Base&>);
      CHECK(
          roscraft::utils::PolymorphicConvertible<const Derived&, const Base&>);
    }

    SUBCASE("Pointer conversions (standard convertible)") {
      CHECK(roscraft::utils::PolymorphicConvertible<Derived*, Base*>);
    }

    SUBCASE("Const correctness") {
      CHECK(roscraft::utils::PolymorphicConvertible<int, const int>);
      CHECK(roscraft::utils::PolymorphicConvertible<Derived&, const Base&>);
    }

    SUBCASE("Mixed polymorphic and non-polymorphic") {
      // Polymorphic type and non-polymorphic type
      CHECK_FALSE(
          roscraft::utils::PolymorphicConvertible<Base, NonPolymorphic>);
      CHECK_FALSE(
          roscraft::utils::PolymorphicConvertible<NonPolymorphic, Base>);
    }

    SUBCASE("Empty pack / single type edge cases") {
      // Single type to itself
      CHECK(roscraft::utils::PolymorphicConvertible<void*, void*>);
    }
  }

}  // TEST_SUITE
