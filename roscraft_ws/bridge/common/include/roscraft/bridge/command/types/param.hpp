#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Request parameter list for a node.
struct ParamListCmd {
  static constexpr std::string_view kName = "ParamListCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::vector<std::pmr::string> prefixes;
  uint32_t depth = 0;
  bool include_types = false;
  std::pmr::string filter_regex;
  double timeout_seconds = 0.0;

  ParamListCmd() : ParamListCmd(std::pmr::get_default_resource()) {}
  explicit ParamListCmd(std::pmr::memory_resource* mr)
      : node_name(mr), prefixes(mr), filter_regex(mr) {}
};

/// @brief Request one parameter value.
struct ParamGetCmd {
  static constexpr std::string_view kName = "ParamGetCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  bool hide_type = false;
  double timeout_seconds = 0.0;

  ParamGetCmd() : ParamGetCmd(std::pmr::get_default_resource()) {}
  explicit ParamGetCmd(std::pmr::memory_resource* mr)
      : node_name(mr), param_name(mr) {}
};

/// @brief Request setting one parameter value.
struct ParamSetCmd {
  static constexpr std::string_view kName = "ParamSetCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  std::pmr::string value_text;
  double timeout_seconds = 0.0;

  ParamSetCmd() : ParamSetCmd(std::pmr::get_default_resource()) {}
  explicit ParamSetCmd(std::pmr::memory_resource* mr)
      : node_name(mr), param_name(mr), value_text(mr) {}
};

/// @brief Request parameter descriptor details.
struct ParamDescribeCmd {
  static constexpr std::string_view kName = "ParamDescribeCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  double timeout_seconds = 0.0;

  ParamDescribeCmd() : ParamDescribeCmd(std::pmr::get_default_resource()) {}
  explicit ParamDescribeCmd(std::pmr::memory_resource* mr)
      : node_name(mr), param_name(mr) {}
};

/// @brief Request parameter dump as YAML text.
struct ParamDumpCmd {
  static constexpr std::string_view kName = "ParamDumpCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::vector<std::pmr::string> prefixes;
  double timeout_seconds = 0.0;

  ParamDumpCmd() : ParamDumpCmd(std::pmr::get_default_resource()) {}
  explicit ParamDumpCmd(std::pmr::memory_resource* mr)
      : node_name(mr), prefixes(mr) {}
};

/// @brief Request parameter load from YAML text.
struct ParamLoadCmd {
  static constexpr std::string_view kName = "ParamLoadCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string yaml_text;
  double timeout_seconds = 0.0;
  bool use_wildcard = true;

  ParamLoadCmd() : ParamLoadCmd(std::pmr::get_default_resource()) {}
  explicit ParamLoadCmd(std::pmr::memory_resource* mr)
      : node_name(mr), yaml_text(mr) {}
};

/// @brief Response for `ParamListCmd`.
struct ParamListResponseCmd {
  static constexpr std::string_view kName = "ParamListResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::vector<std::pmr::string> names;
  std::pmr::vector<std::pmr::string> prefixes;
  std::pmr::vector<std::pmr::string> types;

  ParamListResponseCmd()
      : ParamListResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamListResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr), names(mr), prefixes(mr), types(mr) {}
};

/// @brief Response for `ParamGetCmd`.
struct ParamGetResponseCmd {
  static constexpr std::string_view kName = "ParamGetResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  bool found = false;
  std::pmr::string param_type;
  std::pmr::string value_text;
  bool type_hidden = false;

  ParamGetResponseCmd()
      : ParamGetResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamGetResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr), param_name(mr), param_type(mr), value_text(mr) {}
};

/// @brief Response for `ParamSetCmd`.
struct ParamSetResponseCmd {
  static constexpr std::string_view kName = "ParamSetResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  bool success = false;
  std::pmr::string reason;
  std::pmr::string param_type;
  std::pmr::string value_text;

  ParamSetResponseCmd()
      : ParamSetResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamSetResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr),
        param_name(mr),
        reason(mr),
        param_type(mr),
        value_text(mr) {}
};

/// @brief Response for `ParamDescribeCmd`.
struct ParamDescribeResponseCmd {
  static constexpr std::string_view kName = "ParamDescribeResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string param_name;
  std::pmr::string param_type;
  std::pmr::string description;
  std::pmr::string constraints;
  bool read_only = false;

  bool found = false;

  ParamDescribeResponseCmd()
      : ParamDescribeResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamDescribeResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr),
        param_name(mr),
        param_type(mr),
        description(mr),
        constraints(mr) {}
};

/// @brief Response for `ParamDumpCmd`.
struct ParamDumpResponseCmd {
  static constexpr std::string_view kName = "ParamDumpResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  std::pmr::string yaml_text;

  ParamDumpResponseCmd()
      : ParamDumpResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamDumpResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr), yaml_text(mr) {}
};

/// @brief Response for `ParamLoadCmd`.
struct ParamLoadResponseCmd {
  static constexpr std::string_view kName = "ParamLoadResponseCmd";

  uint64_t request_id = 0;
  std::pmr::string node_name;
  bool success = false;
  std::pmr::string reason;
  uint32_t params_loaded = 0;

  ParamLoadResponseCmd()
      : ParamLoadResponseCmd(std::pmr::get_default_resource()) {}
  explicit ParamLoadResponseCmd(std::pmr::memory_resource* mr)
      : node_name(mr), reason(mr) {}
};

}  // namespace roscraft::bridge
