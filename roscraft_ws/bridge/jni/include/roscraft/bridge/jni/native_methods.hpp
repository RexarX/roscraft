#pragma once

#include <jni.h>

extern "C" {

JNIEXPORT jboolean JNICALL
Java_net_roscraft_bridge_JniBridge_nativeCreate(JNIEnv* env, jclass cls);

JNIEXPORT void JNICALL
Java_net_roscraft_bridge_JniBridge_nativeDestroy(JNIEnv* env, jclass cls);

JNIEXPORT void JNICALL
Java_net_roscraft_bridge_JniBridge_nativeRegisterCallback(JNIEnv* env,
                                                          jclass cls,
                                                          jobject callback);

JNIEXPORT void JNICALL
Java_net_roscraft_bridge_JniBridge_nativeTick(JNIEnv* env, jclass cls);

JNIEXPORT void JNICALL Java_net_roscraft_bridge_JniBridge_nativeSendPacket(
    JNIEnv* env, jclass cls, jbyteArray packet);

}  // extern "C"
