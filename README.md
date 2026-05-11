<a name="readme-top"></a>

# roscraft

<!-- PROJECT SHIELDS -->

[![ROS 2][ros-shield]][ros-url]
[![C++23][cpp-shield]][cpp-url]
[![Java 21][java-shield]][java-url]
[![MIT License][license-shield]][license-url]

## Table of Contents

- [About The Project](#about-the-project)
- [Features](#features)
- [Architecture](#architecture)
- [Prerequisites](#prerequisites)
- [Dependencies](#dependencies)
- [Building](#building)
  - [C++ Backend](#cpp-backend)
  - [Minecraft Mod](#minecraft-mod)
- [CMake Options](#cmake-options)
- [Makefile](#makefile)
- [Testing](#testing)
- [Usage](#usage)
- [Contact](#contact)
- [License](#license)

<a name="about-the-project"></a>

## About The Project

**roscraft** is a bidirectional ROS 2 <=> Minecraft bridge.
It lets Minecraft server operators interact with a live ROS 2 graph directly from inside the game — querying nodes, topics, services, actions, parameters, and interfaces — all through the in-game `/ros` chat command.

The system has two sides: a **Java Fabric mod** for Minecraft and a **C++23 ROS 2 workspace** for the bridge backend. They communicate via FlatBuffers over JNI (in-process) or UDP (network), auto-selecting the transport based on build capabilities and configuration.

<a name="features"></a>

## Features

- ROS 2 graph querying — list nodes, topics, services, and actions
- Real-time topic subscription — see ROS message payloads in Minecraft chat
- Topic publishing — send messages to ROS topics from inside Minecraft
- Service and action calls — invoke ROS services and send action goals
- Topic statistics — hz, bandwidth, and delay metrics
- Parameter manipulation — get, set, list, dump, load, and describe parameters
- Interface inspection — list and show message, service, and action definitions
- Player coordinate forwarding — connected Minecraft player positions sent to ROS
- Dual transport — JNI (in-process) and UDP (network) bridge backends
- Extensible command tree — `/ros` command with 7 subcommand branches

<a name="architecture"></a>

## Architecture

```
schemas/                        FlatBuffers IDL (source of truth for protocol)
    │
mc_mod/                         Minecraft Fabric Mod (Java 21)
  src/main/java/net/roscraft/
    mod/                        Fabric entrypoints: RoscraftMod, config, commands
    bridge/                     Transport: RoscraftBridge (abstract), JniBridge, NetworkBridge
                                Serialization: FlatBufferPacketBuilder/Dispatcher
    │
    │  JNI call  ┊  UDP datagrams (127.0.0.1:7401)
    ▼
roscraft_ws/
  bridge/jni/                   roscraft_bridge_jni — JNI shared library
  bridge/network/               roscraft_bridge_network — standalone UDP server
  bridge/common/                roscraft_bridge_common — ROS 2 nodes, commands, FlatBuffers codec
  core/                         roscraft_core — allocators, containers, utilities (ROS-agnostic)
```

| Module                      | Description                                                                    |
| --------------------------- | ------------------------------------------------------------------------------ |
| **schemas**                 | FlatBuffers IDL protocol definitions (32 packet types in a union)              |
| **mc_mod**                  | Minecraft Fabric mod — commands, chat rendering, bridge clients                |
| **roscraft_core**           | Foundation library — custom allocators, containers, utilities (C++23)          |
| **roscraft_bridge_common**  | ROS 2 integration — node implementations for all operations, FlatBuffers codec |
| **roscraft_bridge_jni**     | JNI shared library — in-process bridge backend                                 |
| **roscraft_bridge_network** | UDP server — network bridge backend                                            |

[↑ Back to Top](#readme-top)

<a name="prerequisites"></a>

## Prerequisites

| Tool  | Minimum Version                   |
| ----- | --------------------------------- |
| CMake | 3.25                              |
| GCC   | 13                                |
| Clang | 17                                |
| Java  | 21                                |
| Ninja | (recommended)                     |
| ROS 2 | Humble or later (tested on Jazzy) |
| flatc | 25.0                              |

<a name="dependencies"></a>

## Dependencies

### C++ (via CPM / system)

| Dependency      | Notes                            |
| --------------- | -------------------------------- |
| Boost           | Container (flat_map), stacktrace |
| FlatBuffers     | Binary serialization protocol    |
| Asio            | UDP async I/O (non-Boost)        |
| glaze           | Modern C++ JSON/serialization    |
| argparse        | CLI argument parsing             |
| concurrentqueue | Lock-free MPMC queue             |
| Taskflow        | Parallel task execution          |
| doctest         | Unit test framework              |

### Java (via Gradle)

| Dependency       | Notes             |
| ---------------- | ----------------- |
| Fabric Loader    | 0.16.5            |
| Fabric API       | 0.102.0+1.21.1    |
| Minecraft        | 1.21.1            |
| flatbuffers-java | 25.+              |
| Gson             | 2.10.1            |
| Mod Menu         | 11.0.4 (optional) |

### System

| Dependency | Notes                                                                   |
| ---------- | ----------------------------------------------------------------------- |
| ROS 2      | Must be sourced before building (e.g. `source /opt/ros/jazzy/setup.sh`) |

The C++ project uses [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) to automatically download missing dependencies.

[↑ Back to Top](#readme-top)

<a name="building"></a>

## Building

<a name="cpp-backend"></a>

### C++ Backend

```bash
# Source ROS 2 first
source /opt/ros/jazzy/setup.sh

# Quick start with Makefile
cd roscraft_ws
make build BUILD_TYPE=relwithdebinfo

# Or use colcon directly
colcon build --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

CMake presets are available in `roscraft_ws/core/CMakePresets.json` (18 presets: Linux/macOS/Windows x GCC/Clang/MSVC x Debug/RelWithDebInfo/Release).

<a name="minecraft-mod"></a>

### Minecraft Mod

```bash
cd mc_mod
./gradlew build
```

The Gradle build auto-generates FlatBuffers Java sources, runs Spotless format checks, and bundles the native JNI library if found in the CMake build tree.

[↑ Back to Top](#readme-top)

<a name="cmake-options"></a>

## CMake Options

| Option                               | Default | Description                         |
| ------------------------------------ | ------- | ----------------------------------- |
| `BUILD_TESTING`                      | ON\*    | Build all test suites               |
| `ROSCRAFT_BUILD_TESTS`               | ON      | Build tests for each package        |
| `ROSCRAFT_ENABLE_WARNINGS_AS_ERRORS` | ON\*    | Treat warnings as errors            |
| `ROSCRAFT_ENABLE_LTO`                | OFF     | Enable link-time optimization       |
| `ROSCRAFT_ENABLE_PCH`                | ON      | Enable precompiled headers          |
| `ROSCRAFT_ENABLE_SIMD`               | ON      | Enable SIMD detection               |
| `ROSCRAFT_ENABLE_SANITIZERS`         | OFF     | Enable sanitizers for debug builds  |
| `ROSCRAFT_SANITIZER_ADDRESS`         | OFF     | Enable AddressSanitizer             |
| `ROSCRAFT_SANITIZER_UNDEFINED`       | OFF     | Enable UndefinedBehaviorSanitizer   |
| `ROSCRAFT_SANITIZER_THREAD`          | OFF     | Enable ThreadSanitizer              |
| `ROSCRAFT_SANITIZER_MEMORY`          | OFF     | Enable MemorySanitizer (Clang only) |

\* Defaults to `ON` when built as top-level project.

[↑ Back to Top](#readme-top)

<a name="makefile"></a>

## Makefile

A convenience `Makefile` is provided for formatting, linting, and building:

```bash
make help              # Show available targets

make build             # Build the workspace
make test              # Run all tests
make format            # Format C/C++ files with clang-format
make lint              # Lint C/C++ files with clang-tidy
```

Variables:

| Variable      | Description                      |
| ------------- | -------------------------------- |
| `BUILD_TYPE`  | debug, relwithdebinfo, release   |
| `CMAKE_ARGS`  | Extra arguments passed to CMake  |
| `COLCON_ARGS` | Extra arguments passed to colcon |

<a name="testing"></a>

## Testing

Tests use the [doctest](https://github.com/doctest/doctest) framework for C++. Run via the Makefile or CTest directly:

```bash
# Using Makefile
cd roscraft_ws
make test BUILD_TYPE=debug

# Using CTest directly
cd roscraft_ws/build/debug
ctest
```

[↑ Back to Top](#readme-top)

<a name="usage"></a>

## Usage

1. Source your ROS 2 environment and start the C++ backend:

```bash
source /opt/ros/jazzy/setup.sh
cd roscraft_ws && source install/setup.sh
ros2 run roscraft_bridge_network roscraft_bridge_network # optional --host 127.0.0.1 --port 7401
```

2. Install the mod in your Minecraft Fabric server and configure `config/roscraft.json` or you can do it via the in-game GUI (if Mod Menu mod installed) or via commands in the game:

```json
{
  "bridgeType": "NETWORK",
  "networkHost": "127.0.0.1",
  "networkPort": 7401
}
```

3. In-game, use the `/ros` command:

```
/ros topic list                              # List all topics
/ros topic subscribe /chatter                # Subscribe to a topic
/ros topic publish /chatter "hello"          # Publish a message
/ros service list                            # List all services
/ros param get /my_node/param_name          # Get a parameter
/ros node info /my_node                     # Inspect a node
/ros action send_goal /action_name {...}    # Send an action goal
```

[↑ Back to Top](#readme-top)

<a name="contact"></a>

## Contact

**RexarX** — who727cares@gmail.com

**Project Link:** [https://github.com/RexarX/roscraft](https://github.com/RexarX/roscraft)

<a name="license"></a>

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for more information.

[↑ Back to Top](#readme-top)

<!-- MARKDOWN LINKS & IMAGES -->

[license-shield]: https://img.shields.io/github/license/RexarX/roscraft.svg?style=for-the-badge
[license-url]: https://github.com/RexarX/roscraft/blob/main/LICENSE
[ros-shield]: https://img.shields.io/badge/ROS-2-blue.svg?style=for-the-badge&logo=ros
[ros-url]: https://www.ros.org/
[cpp-shield]: https://img.shields.io/badge/C%2B%2B-23-blue.svg?style=for-the-badge&logo=c%2B%2B
[cpp-url]: https://en.cppreference.com/w/cpp/23
[java-shield]: https://img.shields.io/badge/Java-21-orange.svg?style=for-the-badge&logo=openjdk
[java-url]: https://openjdk.org/projects/jdk/21/
