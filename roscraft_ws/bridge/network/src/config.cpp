#include <pch.hpp>

#include <roscraft/bridge/network/config.hpp>
#include <roscraft/stacktrace.hpp>

#include <argparse/argparse.hpp>

#include <rclcpp/logging.hpp>

#include <cstdint>
#include <exception>
#include <string>

namespace roscraft::bridge::network {

BridgeConfig BridgeConfig::From(int argc, char* argv[]) {
  BridgeConfig config{};
  if (argc < 2) [[unlikely]] {
    return config;
  }

  argparse::ArgumentParser parser("roscraft_bridge_network");
  parser.add_argument("-h", "--host")
      .default_value(std::string{kDefaultHost})
      .help("Host ip address");
  parser.add_argument("-p", "--port")
      .default_value(std::to_string(kDefaultPort))
      .scan<'u', uint16_t>()
      .help("Port number");
  parser.add_argument("--allow-multiple-connections")
      .flag()
      .help("Allow multiple client connections");

  try {
    parser.parse_args(argc, argv);
    config.host = parser.get<std::string>("--host");
    config.port = parser.get<uint16_t>("--port");
    config.allow_multiple_connections =
        parser.get<bool>("--allow-multiple-connections");
  } catch (const std::exception& e) {
    auto st = roscraft::Stacktrace::FromCurrentException();
    RCLCPP_WARN(rclcpp::get_logger("BridgeConfig"),
                "Failed to parse command line arguments: %s!\n%s", e.what(),
                st.ToString().c_str());
  }
  return config;
}

}  // namespace roscraft::bridge::network
