#pragma once

#include <roscraft/bridge/app/config.hpp>
#include <roscraft/bridge/assert.hpp>
#include <roscraft/bridge/bridge.hpp>
#include <roscraft/bridge/command/handler_registry.hpp>
#include <roscraft/bridge/command/queue.hpp>
#include <roscraft/memory/frame_allocator.hpp>

#include <rclcpp/executors/multi_threaded_executor.hpp>
#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace roscraft::bridge {

enum class AppState : uint8_t {
  kUninitialized,
  kInitialized,
  kShuttingDown,
};

/// @brief Class for managing the entire application.
/// @details Creates a Taskflow executor for running async tasks, initialises
/// ROS2, and manages the application lifecycle.
///
/// Frame Allocator Usage:
/// The DoubleFrameAllocator provides two frames for double-buffering:
/// - Frame 0 (current): Actively being processed
/// - Frame 1 (pending): Accumulating data for next cycle
///
/// Call AdvanceFrame() after processing to swap buffers. This enables
/// lock-free producer-consumer patterns where one frame is being consumed
/// while the other is being produced.
///
/// Thread Safety:
/// - ROS callbacks run on a dedicated thread (single-threaded)
/// - Taskflow executor handles async tasks
/// - Frame advancing should be called from a single "tick" thread
class App {
public:
  /// @brief Gets the singleton application instance.
  /// @return Singleton `App` instance
  [[nodiscard]] static App& Instance() noexcept;

  ~App();
  App(const App&) = delete;
  App(App&&) = delete;

  App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;

  /// @brief Configures the `App` with the given configuration.
  /// @warning Triggers assertion if:
  /// - Configuration is invalid
  /// - `App` is already initialized
  void Init(AppConfig config);

  /// @brief Graceful shutdown. Safe to call multiple times.
  void Shutdown();

  /// @brief Runs one application tick.
  /// @details Executes one bridge tick and advances frame allocators.
  void Tick();

  /// @brief Requests application shutdown from any thread.
  /// @details Thread-safe. Wakes any thread waiting in `WaitForShutdown()`.
  void RequestShutdown() noexcept;

  /// @brief Blocks until shutdown is requested.
  /// @details Returns immediately if already shutting down.
  void WaitForShutdown();

  /// @brief Adds a node to the internal ROS executor.
  /// @warning Triggers assertion if called after `App` is initialized.
  /// @param node The node to add
  void AddNode(rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node);

  /// @brief Adds a node to the internal ROS executor.
  /// @warning Triggers assertion if called after `App` is initialized.
  /// @param node The node to add
  void AddNode(std::shared_ptr<rclcpp::Node> node);

  /// @brief Swaps frames: pending becomes current, current is reset.
  /// @details Should be called at the end of each processing cycle.
  void AdvanceFrame() noexcept { allocator_.Advance(); }

  /// @brief Resets both frames (clears all allocated memory).
  void ResetAllocator() noexcept { allocator_.Reset(); }

  /// @brief Checks if shutdown has been requested.
  /// @return `true` if shutdown has been requested, `false` otherwise
  [[nodiscard]] bool IsShutdownRequested() const noexcept {
    return shutdown_requested_.load(std::memory_order_acquire);
  }

  /// @brief Returns the current state of the `App`.
  /// @return The current state of the `App`
  [[nodiscard]] AppState State() const noexcept {
    return state_.load(std::memory_order_relaxed);
  }

  /// @brief Returns index of the currently active frame.
  /// @return Index of the currently active frame
  [[nodiscard]] size_t CurrentFrameIndex() const noexcept {
    return allocator_.FrameIndex();
  }

  /// @brief Returns index of the pending frame.
  /// @return Index of the pending frame
  [[nodiscard]] size_t PendingFrameIndex() const noexcept {
    return (CurrentFrameIndex() + 1) %
           memory::DoubleFrameAllocator::FrameCount();
  }

  /// @brief Returns current frame arena.
  /// @return Current frame arena
  [[nodiscard]] memory::ArenaAllocator& CurrentFrameArena() noexcept {
    return allocator_.Arena(CurrentFrameIndex());
  }

  /// @brief Returns current frame arena (const).
  /// @return Current frame arena (const)
  [[nodiscard]] const memory::ArenaAllocator& CurrentFrameArena()
      const noexcept {
    return allocator_.Arena(CurrentFrameIndex());
  }

  /// @brief Returns pending frame arena.
  /// @return Pending frame arena
  [[nodiscard]] memory::ArenaAllocator& PendingFrameArena() noexcept {
    return allocator_.Arena(PendingFrameIndex());
  }

  /// @brief Returns pending frame arena (const).
  /// @return Pending frame arena (const)
  [[nodiscard]] const memory::ArenaAllocator& PendingFrameArena()
      const noexcept {
    return allocator_.Arena(PendingFrameIndex());
  }

  /// @brief Returns current frame allocator as memory resource.
  /// @return Current frame allocator as memory resource
  [[nodiscard]] std::pmr::memory_resource& CurrentFrameAllocator() noexcept {
    return CurrentFrameArena();
  }

  /// @brief Returns current frame allocator as memory resource (const).
  /// @return Current frame allocator as memory resource (const)
  [[nodiscard]] const std::pmr::memory_resource& CurrentFrameAllocator()
      const noexcept {
    return CurrentFrameArena();
  }

  /// @brief Returns pending frame allocator as memory resource.
  /// @return Pending frame allocator as memory resource
  [[nodiscard]] std::pmr::memory_resource& PendingFrameAllocator() noexcept {
    return PendingFrameArena();
  }

  /// @brief Returns pending frame allocator as memory resource (const).
  /// @return Pending frame allocator as memory resource (const)
  [[nodiscard]] const std::pmr::memory_resource& PendingFrameAllocator()
      const noexcept {
    return PendingFrameArena();
  }

  /// @brief Gets the bridge used by the `App`.
  /// @return Reference to the bridge used by the `App`
  [[nodiscard]] Bridge& GetBridge() noexcept;

  /// @brief Gets the bridge used by the `App` (const).
  /// @return Const reference to the bridge used by the `App`
  [[nodiscard]] const Bridge& GetBridge() const noexcept;

  /// @brief Gets the bridge used by the `App` (templated).
  /// @tparam T The type of bridge to get
  /// @return Reference to the bridge used by the `App`
  template <BridgeTrait T>
  [[nodiscard]] T& GetBridge() noexcept;

  /// @brief Gets the bridge used by the `App` (templated, const).
  /// @tparam T The type of bridge to get
  /// @return Const reference to the bridge used by the `App`
  template <BridgeTrait T>
  [[nodiscard]] const T& GetBridge() const noexcept;

  /// @brief Gets the incoming command queue (mod -> ROS).
  /// @return Reference to the incoming command queue
  [[nodiscard]] CommandQueue& IncomingQueue() noexcept {
    return incoming_queue_;
  }

  /// @brief Gets the incoming command queue (mod -> ROS) (const).
  /// @return Const reference to the incoming command queue
  [[nodiscard]] const CommandQueue& IncomingQueue() const noexcept {
    return incoming_queue_;
  }

  /// @brief Gets the outgoing command queue (ROS -> mod).
  /// @return Reference to the outgoing command queue
  [[nodiscard]] CommandQueue& OutgoingQueue() noexcept {
    return outgoing_queue_;
  }

  /// @brief Gets the outgoing command queue (ROS -> mod) (const).
  /// @return Const reference to the outgoing command queue
  [[nodiscard]] const CommandQueue& OutgoingQueue() const noexcept {
    return outgoing_queue_;
  }

  /// @brief Gets the command handler registry.
  /// @return Reference to the command handler registry
  [[nodiscard]] CommandHandlerRegistry& HandlerRegistry() noexcept {
    return handler_registry_;
  }

  /// @brief Gets the command handler registry (const).
  /// @return Const reference to the command handler registry
  [[nodiscard]] const CommandHandlerRegistry& HandlerRegistry() const noexcept {
    return handler_registry_;
  }

  /// @brief Gets the double frame allocator.
  /// @return Reference to the double frame allocator
  [[nodiscard]] memory::DoubleFrameAllocator& Allocator() noexcept {
    return allocator_;
  }

  /// @brief Gets the double frame allocator (const).
  /// @return Const reference to the double frame allocator
  [[nodiscard]] const memory::DoubleFrameAllocator& Allocator() const noexcept {
    return allocator_;
  }

  /// @brief ROS executor used for spinning nodes.
  /// @warning Triggers assertion if ROS executor is not initialized.
  /// @return Reference to the ROS executor
  [[nodiscard]] auto RosExecutor() noexcept
      -> rclcpp::executors::MultiThreadedExecutor& {
    ROSCRAFT_ASSERT(ros_executor_.has_value(),
                    "ROS executor is not initialized!");
    return *ros_executor_;
  }

  /// @brief ROS executor used for spinning nodes (const).
  /// @warning Triggers assertion if ROS executor is not initialized.
  /// @return Const reference to the ROS executor
  [[nodiscard]] auto RosExecutor() const noexcept
      -> const rclcpp::executors::MultiThreadedExecutor& {
    ROSCRAFT_ASSERT(ros_executor_.has_value(),
                    "ROS executor is not initialized!");
    return *ros_executor_;
  }

private:
  App() = default;

  void InitROS(int argc, char* argv[]);
  void SpinROS();
  void CleanUpROS();
  void ShutdownROS();

  void RegisterAllNodes();
  void UnregisterAllNodes();

  void RegisterAllCommandTypes();
  void RegisterAllCommandHandlers();

  std::unique_ptr<Bridge> bridge_;

  CommandQueue incoming_queue_;
  CommandQueue outgoing_queue_;
  CommandHandlerRegistry handler_registry_;

  /// Double frame allocator for lock-free producer-consumer patterns.
  /// Frame 0: current (being consumed)
  /// Frame 1: pending (being produced)
  /// Advance() swaps them.
  static constexpr size_t kAllocatorFrameCapacity = 512 * 1024;
  memory::DoubleFrameAllocator allocator_{kAllocatorFrameCapacity};

  std::atomic<AppState> state_{AppState::kUninitialized};
  std::atomic<bool> shutdown_requested_{false};

  std::mutex shutdown_mutex_;
  std::condition_variable shutdown_cv_;

  std::vector<rclcpp::node_interfaces::NodeBaseInterface::SharedPtr> nodes_;
  std::vector<std::shared_ptr<rclcpp::Node>> owned_nodes_;
  std::optional<rclcpp::executors::MultiThreadedExecutor> ros_executor_;
  std::jthread ros_spin_thread_;
  std::optional<std::promise<void>> shutdown_promise_;
  std::shared_future<void> shutdown_future_;
};

inline App& App::Instance() noexcept {
  static App instance;
  return instance;
}

inline App::~App() {
  Shutdown();
}

inline void App::AddNode(
    rclcpp::node_interfaces::NodeBaseInterface::SharedPtr node) {
  ROSCRAFT_ASSERT(ros_executor_.has_value(),
                  "ROS executor is not initialized!");
  ROSCRAFT_ASSERT(node != nullptr, "Node base interface is null!");
  nodes_.push_back(node);
  ros_executor_->add_node(node);
}

inline void App::AddNode(std::shared_ptr<rclcpp::Node> node) {
  ROSCRAFT_ASSERT(ros_executor_.has_value(),
                  "ROS executor is not initialized!");
  ROSCRAFT_ASSERT(node != nullptr, "Node is null!");

  auto base_interface = node->get_node_base_interface();
  owned_nodes_.push_back(std::move(node));
  AddNode(base_interface);
}

inline void App::RequestShutdown() noexcept {
  shutdown_requested_.store(true, std::memory_order_release);
  shutdown_cv_.notify_all();
}

inline void App::WaitForShutdown() {
  std::unique_lock lock(shutdown_mutex_);
  shutdown_cv_.wait(lock, [this] {
    return shutdown_requested_.load(std::memory_order_acquire);
  });
}

inline Bridge& App::GetBridge() noexcept {
  ROSCRAFT_ASSERT(State() == AppState::kInitialized, "App is not initialized!");
  return *bridge_;
}

inline const Bridge& App::GetBridge() const noexcept {
  ROSCRAFT_ASSERT(State() == AppState::kInitialized, "App is not initialized!");
  return *bridge_;
}

template <BridgeTrait T>
inline T& App::GetBridge() noexcept {
  ROSCRAFT_ASSERT(State() == AppState::kInitialized, "App is not initialized!");
  return static_cast<T&>(*bridge_);
}

template <BridgeTrait T>
inline const T& App::GetBridge() const noexcept {
  ROSCRAFT_ASSERT(State() == AppState::kInitialized, "App is not initialized!");
  return static_cast<const T&>(*bridge_);
}

}  // namespace roscraft::bridge
