#include <doctest/doctest.h>

#include <roscraft/bridge/nodes/ros/details/introspection_codec.hpp>

#include <rosidl_runtime_c/message_type_support_struct.h>
#include <rclcpp/rclcpp.hpp>
#include <rosidl_typesupport_introspection_cpp/identifier.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

using namespace roscraft::bridge::details;

namespace {

class ScopedRosContext {
public:
  ScopedRosContext() {
    if (!rclcpp::ok()) {
      int argc = 0;
      rclcpp::init(argc, nullptr);
      owns_context_ = true;
    }
  }

  ScopedRosContext(const ScopedRosContext&) = delete;
  ScopedRosContext(ScopedRosContext&&) = delete;
  ~ScopedRosContext() {
    if (owns_context_ && rclcpp::ok()) {
      rclcpp::shutdown();
    }
  }

  auto operator=(const ScopedRosContext&) -> ScopedRosContext& = delete;
  auto operator=(ScopedRosContext&&) -> ScopedRosContext& = delete;

private:
  bool owns_context_ = false;
};

}  // namespace

TEST_SUITE("bridge::details::IntrospectionCodec") {
  TEST_CASE("bridge::details::IntrospectionMembers") {
    ScopedRosContext ros_context;

    SUBCASE("Returns valid members for known message type") {
      const auto introspection =
          LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);
      CHECK_GT(introspection->message_members->member_count_, 0U);
    }

    SUBCASE("Returns nullptr for null type support") {
      const auto* members = ToIntrospectionMembers(nullptr);
      CHECK_EQ(members, nullptr);
    }

    SUBCASE("Returns nullptr for cpp type support identifier") {
      const auto introspection =
          LoadMessageIntrospection("std_msgs/msg/String");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      rosidl_message_type_support_t invalid_ts{};
      invalid_ts.typesupport_identifier = "rosidl_typesupport_cpp";
      invalid_ts.data = introspection->message_members;
      const auto* members = ToIntrospectionMembers(&invalid_ts);
      CHECK_EQ(members, nullptr);
    }
  }

  TEST_CASE("bridge::details::FindMemberByName") {
    ScopedRosContext ros_context;

    SUBCASE("Finds existing member by name") {
      const auto introspection =
          LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto* member =
          FindMemberByName(*introspection->message_members, "header");
      REQUIRE_NE(member, nullptr);
      CHECK_GE(member->offset_, 0U);
    }

    SUBCASE("Returns nullptr for nonexistent member") {
      const auto introspection =
          LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto* member =
          FindMemberByName(*introspection->message_members, "nonexistent");
      CHECK_EQ(member, nullptr);
    }

    SUBCASE("Finds sec member in builtin_interfaces/msg/Time") {
      const auto introspection =
          LoadMessageIntrospection("builtin_interfaces/msg/Time");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto* sec_member =
          FindMemberByName(*introspection->message_members, "sec");
      REQUIRE_NE(sec_member, nullptr);
      CHECK_GE(sec_member->offset_, 0U);

      const auto* nanosec_member =
          FindMemberByName(*introspection->message_members, "nanosec");
      REQUIRE_NE(nanosec_member, nullptr);
    }
  }

  TEST_CASE("bridge::details::FindMemberOffset") {
    ScopedRosContext ros_context;

    SUBCASE("Finds offset for existing member") {
      const auto introspection =
          LoadMessageIntrospection("builtin_interfaces/msg/Time");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto sec_offset =
          FindMemberOffset(*introspection->message_members, "sec");
      REQUIRE(sec_offset.has_value());
      CHECK_GE(*sec_offset, 0U);

      const auto nanosec_offset =
          FindMemberOffset(*introspection->message_members, "nanosec");
      REQUIRE(nanosec_offset.has_value());
      CHECK_GE(*nanosec_offset, 0U);
    }

    SUBCASE("Returns nullopt for nonexistent member") {
      const auto introspection =
          LoadMessageIntrospection("builtin_interfaces/msg/Time");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto offset =
          FindMemberOffset(*introspection->message_members, "nonexistent");
      CHECK_FALSE(offset.has_value());
    }

    SUBCASE("Finds header offset in PointStamped") {
      const auto introspection =
          LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());
      REQUIRE_NE(introspection->message_members, nullptr);

      const auto offset =
          FindMemberOffset(*introspection->message_members, "header");
      REQUIRE(offset.has_value());
    }
  }

  TEST_CASE("bridge::details::LoadDelayStampExtractor") {
    ScopedRosContext ros_context;

    SUBCASE("Loads extractor for header stamp message") {
      const auto extractor =
          LoadDelayStampExtractor("geometry_msgs/msg/PointStamped");
      REQUIRE(extractor.has_value());

      CHECK_NE(extractor->message_introspection.message_typesupport, nullptr);
      CHECK_NE(extractor->message_introspection.message_members, nullptr);
      CHECK(extractor->offsets.uses_header_field);
      CHECK_GT(extractor->offsets.message_size, 0U);
    }

    SUBCASE("Loads extractor for builtin time message") {
      const auto extractor =
          LoadDelayStampExtractor("builtin_interfaces/msg/Time");
      REQUIRE(extractor.has_value());

      CHECK_NE(extractor->message_introspection.message_typesupport, nullptr);
      CHECK_NE(extractor->message_introspection.message_members, nullptr);
      CHECK_FALSE(extractor->offsets.uses_header_field);
      CHECK_GT(extractor->offsets.message_size, 0U);
    }

    SUBCASE("Fails for unsupported message layout") {
      const auto extractor = LoadDelayStampExtractor("std_msgs/msg/String");
      CHECK_FALSE(extractor.has_value());
      REQUIRE_FALSE(extractor.has_value());
      CHECK_EQ(extractor.error().code,
               IntrospectionCodecErrorCode::kUnsupportedFieldType);
    }

    SUBCASE("Fails for unknown message type") {
      const auto extractor = LoadDelayStampExtractor("not_a_valid_type");
      CHECK_FALSE(extractor.has_value());
      REQUIRE_FALSE(extractor.has_value());
      CHECK_EQ(extractor.error().code,
               IntrospectionCodecErrorCode::kTypeSupportLoadFailed);
    }
  }

  TEST_CASE("bridge::details::ExtractStampSecondsFromCdr") {
    ScopedRosContext ros_context;

    SUBCASE("Extracts header stamp seconds") {
      const auto introspection =
          LoadMessageIntrospection("geometry_msgs/msg/PointStamped");
      REQUIRE(introspection.has_value());

      constexpr std::string_view kYaml =
          "header:\n"
          "  stamp:\n"
          "    sec: 18\n"
          "    nanosec: 125000000\n"
          "  frame_id: map\n"
          "point:\n"
          "  x: 1.0\n"
          "  y: 2.0\n"
          "  z: 3.0\n";
      const auto payload = SerializeYamlToCdr(kYaml, *introspection);
      REQUIRE(payload.has_value());

      const auto extractor =
          LoadDelayStampExtractor("geometry_msgs/msg/PointStamped");
      REQUIRE(extractor.has_value());

      const auto stamp_seconds =
          ExtractStampSecondsFromCdr(*payload, *extractor);
      REQUIRE(stamp_seconds.has_value());
      CHECK_EQ(*stamp_seconds, doctest::Approx(18.125));
    }

    SUBCASE("Extracts builtin time seconds") {
      const auto introspection =
          LoadMessageIntrospection("builtin_interfaces/msg/Time");
      REQUIRE(introspection.has_value());

      constexpr std::string_view kYaml =
          "sec: 7\n"
          "nanosec: 750000000\n";
      const auto payload = SerializeYamlToCdr(kYaml, *introspection);
      REQUIRE(payload.has_value());

      const auto extractor =
          LoadDelayStampExtractor("builtin_interfaces/msg/Time");
      REQUIRE(extractor.has_value());

      const auto stamp_seconds =
          ExtractStampSecondsFromCdr(*payload, *extractor);
      REQUIRE(stamp_seconds.has_value());
      CHECK_EQ(*stamp_seconds, doctest::Approx(7.75));
    }

    SUBCASE("Returns nullopt for malformed payload") {
      const auto extractor =
          LoadDelayStampExtractor("builtin_interfaces/msg/Time");
      REQUIRE(extractor.has_value());

      const std::vector<uint8_t> payload{0x01, 0x02, 0x03, 0x04};
      const auto stamp_seconds =
          ExtractStampSecondsFromCdr(payload, *extractor);
      CHECK_FALSE(stamp_seconds.has_value());
    }

    SUBCASE("Returns nullopt for incomplete extractor") {
      const DelayStampExtractor extractor;
      const std::vector<uint8_t> payload;

      const auto stamp_seconds = ExtractStampSecondsFromCdr(payload, extractor);
      CHECK_FALSE(stamp_seconds.has_value());
    }
  }
}
