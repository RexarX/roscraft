#pragma once

#include <roscraft/bridge/command/types/common.hpp>

#include <cstdint>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

namespace roscraft::bridge {

/// @brief Request player positions from the mod.
struct QueryPlayersCmd {
  static constexpr std::string_view kName = "QueryPlayersCmd";

  uint64_t request_id = 0;
};

/// @brief Response to QueryPlayersCmd.
struct PlayerListCmd {
  static constexpr std::string_view kName = "PlayerListCmd";

  uint64_t request_id = 0;
  std::pmr::vector<Player> players;

  PlayerListCmd() : PlayerListCmd(std::pmr::get_default_resource()) {}
  explicit PlayerListCmd(std::pmr::memory_resource* mr) : players(mr) {}
};

}  // namespace roscraft::bridge
