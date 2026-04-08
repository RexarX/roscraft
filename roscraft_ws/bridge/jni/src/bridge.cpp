#include <pch.hpp>

#include <roscraft/bridge/app/app.hpp>
#include <roscraft/bridge/jni/bridge.hpp>
#include <roscraft/bridge/jni/config.hpp>

#include <argparse/argparse.hpp>

#include <jni.h>

namespace roscraft::bridge::jni {

void JNIBridge::ParseArgs(int argc, char* argv[]) {
  if (argc < 2) [[unlikely]] {
    return;
  }
}

void JNIBridge::Init(App& app) {}

void JNIBridge::Destroy(App& app) {}

void JNIBridge::Tick(App& app) {}

}  // namespace roscraft::bridge::jni
