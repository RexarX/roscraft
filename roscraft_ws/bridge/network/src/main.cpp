#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/network/bridge.hpp>
#include <roscraft/bridge/network/runner.hpp>

#include <memory>

using namespace roscraft::bridge;

int main(int argc, char* argv[]) {
  auto bridge = std::make_unique<network::NetworkBridge>();
  bridge->ParseArgs(argc, argv);

  App app;
  app.Init(AppConfig::From(std::move(bridge), argc, argv));

  const auto result = network::Run(app);

  return result ? 0 : network::RunnerErrorToExitCode(result.error());
}
