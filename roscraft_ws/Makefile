SHELL := /bin/bash

.DEFAULT_GOAL := help

ROOT_DIR := $(abspath $(CURDIR)/..)
SCRIPTS_DIR := $(ROOT_DIR)/scripts

PYTHON ?= python3

CMAKE_ARGS ?=
COLCON_ARGS ?=
BUILD_TYPE ?= relwithdebinfo

FILES ?=
CLANG_FORMAT_CFG ?=
CLANG_TIDY_CFG ?=
BUILD_DIRS ?=
EXCLUDE ?=

.PHONY: help format format-check lint build build-debug build-relwithdebinfo build-release test test-debug test-relwithdebinfo test-release

help:
	@printf "Roscraft workspace helpers\n"
	@printf "\n"
	@printf "Targets:\n"
	@printf "  make build                 Build with BUILD_TYPE (default: relwithdebinfo)\n"
	@printf "  make build-debug           Build with Debug configuration\n"
	@printf "  make build-relwithdebinfo  Build with RelWithDebInfo configuration\n"
	@printf "  make build-release         Build with Release configuration\n"
	@printf "  make test                  Run tests with BUILD_TYPE (default: relwithdebinfo)\n"
	@printf "  make test-debug            Run tests with Debug configuration\n"
	@printf "  make test-relwithdebinfo   Run tests with RelWithDebInfo configuration\n"
	@printf "  make test-release          Run tests with Release configuration\n"
	@printf "  make format                Format C/C++ files with clang-format\n"
	@printf "  make format-check          Check C/C++ formatting with clang-format\n"
	@printf "  make lint                  Lint C/C++ files with clang-tidy\n"
	@printf "  make help                  Show this help\n"
	@printf "\n"
	@printf "Variables:\n"
	@printf "  BUILD_TYPE        Build type: debug, relwithdebinfo, release (default: relwithdebinfo)\n"
	@printf "  BUILD_DIR         Override build/install base dirs (use with test)\n"
	@printf "  CMAKE_ARGS        Extra cmake arguments passed to colcon build\n"
	@printf "  COLCON_ARGS       Extra colcon arguments (e.g. --parallel-workers 6)\n"
	@printf "  FILES             Space-separated files/dirs to process\n"
	@printf "  EXCLUDE           Space-separated files/dirs to exclude\n"
	@printf "  CLANG_FORMAT_CFG  Optional path to .clang-format\n"
	@printf "  CLANG_TIDY_CFG    Optional path to .clang-tidy\n"
	@printf "  BUILD_DIRS        Space-separated build dirs for lint\n"
	@printf "  PYTHON            Python executable (default: python3)\n"
	@printf "\n"
	@printf "Examples:\n"
	@printf "  make build BUILD_TYPE=release\n"
	@printf "  make build-debug CMAKE_ARGS=\"-DROSCRAFT_BRIDGE_COMMON_BUILD_TESTS=ON\"\n"
	@printf "  make test BUILD_TYPE=release\n"
	@printf "  make test-debug\n"
	@printf "  make test-release\n"
	@printf "  make test BUILD_DIR=\"build/debug install/debug\"\n"
	@printf "  make format FILES=\"core/src/assert.cpp\"\n"
	@printf "  make format-check CLANG_FORMAT_CFG=.clang-format\n"
	@printf "  make lint BUILD_DIRS=\"core/build bridge/network/build\" FILES=\"core/src\"\n"

build:
	@if [[ -z "$(BUILD_TYPE)" ]]; then \
	  echo "Error: BUILD_TYPE is required"; \
	  exit 1; \
	fi
	@cmake_build_type=""; \
	case "$(BUILD_TYPE)" in \
	  debug) cmake_build_type="Debug" ;; \
	  relwithdebinfo) cmake_build_type="RelWithDebInfo" ;; \
	  release) cmake_build_type="Release" ;; \
	  *) \
	    echo "Error: BUILD_TYPE must be one of: debug, relwithdebinfo, release"; \
	    exit 1; \
	    ;; \
	esac; \
	colcon build \
	  --build-base build/$(BUILD_TYPE) \
	  --install-base install/$(BUILD_TYPE) \
	  $(COLCON_ARGS) \
	  --cmake-args -DCMAKE_BUILD_TYPE=$$cmake_build_type $(CMAKE_ARGS)

build-debug:
	$(MAKE) build BUILD_TYPE=debug

build-relwithdebinfo:
	$(MAKE) build BUILD_TYPE=relwithdebinfo

build-release:
	$(MAKE) build BUILD_TYPE=release

test:
	@if [[ -n "$(BUILD_DIR)" ]]; then \
	  build_base=$$(echo "$(BUILD_DIR)" | cut -d' ' -f1); \
	  install_base=$$(echo "$(BUILD_DIR)" | cut -d' ' -f2); \
	  if [[ -z "$$install_base" ]]; then install_base="$${build_base%/build}/install"; fi; \
	  echo "Testing build dir: $$build_base"; \
	  . "$$install_base/setup.bash" && colcon test --build-base "$$build_base" $(COLCON_ARGS); \
	elif [[ -n "$(BUILD_TYPE)" ]]; then \
	  echo "Testing BUILD_TYPE=$(BUILD_TYPE)"; \
	  . "install/$(BUILD_TYPE)/setup.bash" && colcon test --build-base "build/$(BUILD_TYPE)"; \
	else \
	  echo "Error: Must set BUILD_TYPE or BUILD_DIR"; \
	  echo "  make test BUILD_TYPE=debug"; \
	  echo "  make test BUILD_DIR=\"build/debug install/debug\""; \
	  exit 1; \
	fi

test-debug:
	$(MAKE) test BUILD_TYPE=debug

test-relwithdebinfo:
	$(MAKE) test BUILD_TYPE=relwithdebinfo

test-release:
	$(MAKE) test BUILD_TYPE=release

format:
	@args=(); \
	if [[ -n "$(CLANG_FORMAT_CFG)" ]]; then args+=("--config" "$(CLANG_FORMAT_CFG)"); fi; \
	if [[ -n "$(EXCLUDE)" ]]; then for e in $(EXCLUDE); do args+=("--exclude" "$$e"); done; fi; \
	if [[ -n "$(FILES)" ]]; then for f in $(FILES); do args+=("$$f"); done; fi; \
	"$(PYTHON)" "$(SCRIPTS_DIR)/format.py" "$${args[@]}"

format-check:
	@args=("--check"); \
	if [[ -n "$(CLANG_FORMAT_CFG)" ]]; then args+=("--config" "$(CLANG_FORMAT_CFG)"); fi; \
	if [[ -n "$(EXCLUDE)" ]]; then for e in $(EXCLUDE); do args+=("--exclude" "$$e"); done; fi; \
	if [[ -n "$(FILES)" ]]; then for f in $(FILES); do args+=("$$f"); done; fi; \
	"$(PYTHON)" "$(SCRIPTS_DIR)/format.py" "$${args[@]}"

lint:
	@args=(); \
	if [[ -n "$(CLANG_TIDY_CFG)" ]]; then args+=("--config" "$(CLANG_TIDY_CFG)"); fi; \
	if [[ -n "$(BUILD_DIRS)" ]]; then for d in $(BUILD_DIRS); do args+=("--build-dir" "$$d"); done; fi; \
	if [[ -n "$(EXCLUDE)" ]]; then for e in $(EXCLUDE); do args+=("--exclude" "$$e"); done; fi; \
	if [[ -n "$(FILES)" ]]; then for f in $(FILES); do args+=("$$f"); done; fi; \
	"$(PYTHON)" "$(SCRIPTS_DIR)/lint.py" "$${args[@]}"
