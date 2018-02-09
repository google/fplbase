// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// clang-format off
#include "precompiled.h"
#include "fplbase/utilities.h"
#include "fplutil/mutex.h"
// clang-format on

#if !defined(_CRT_SECURE_NO_DEPRECATE)
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <cstdio>

namespace fplbase {

int32_t GetSystemRamSize() {
  return 0;
}

bool GetStoragePath(const char *app_name, std::string *path_string) {
  (void)app_name;
  *path_string = "/";
  return true;
}

#if defined(__ANDROID__)
static jobject g_activity = nullptr;

jobject AndroidGetActivity(bool optional) {
  // If you get this assert, you're calling a function that depends on the
  // activity. Please ensure you call AndroidSetActivity near the start of
  // your program.
  assert(optional || g_activity);
  return g_activity;
}

void AndroidSetActivity(jobject activity) {
  g_activity = activity;
}

static JavaVM* g_jvm = NULL;
static jint g_jni_version = 0;
static pthread_key_t g_pthread_key;

static JavaVM* GetJVM() {
  return g_jvm;
}

static JNIEnv* AttachCurrentThread() {
  JNIEnv* env = NULL;
  JavaVM* jvm = GetJVM();
  if (!jvm)
    return NULL;

  // Avoid attaching threads provided by the JVM.
  if (jvm->GetEnv(reinterpret_cast<void**>(&env), g_jni_version) == JNI_OK)
    return env;

  if (!(env = static_cast<JNIEnv*>(pthread_getspecific(g_pthread_key)))) {
    jint ret = jvm->AttachCurrentThread(&env, NULL);
    if (ret != JNI_OK)
      return NULL;

    pthread_setspecific(g_pthread_key, env);
  }

  return env;
}

static void DetachCurrentThread() {
  JavaVM* jvm = GetJVM();
  if (jvm) {
    jvm->DetachCurrentThread();
  }
}

static void DetachCurrentThreadWrapper(void* value) {
  JNIEnv* env = reinterpret_cast<JNIEnv*>(value);
  if (env) {
    DetachCurrentThread();
    pthread_setspecific(g_pthread_key, NULL);
  }
}

void AndroidSetJavaVM(JavaVM* vm, jint jni_version) {
  g_jvm = vm;
  g_jni_version = jni_version;

  pthread_key_create(&g_pthread_key, DetachCurrentThreadWrapper);
}

JNIEnv* AndroidGetJNIEnv() {
  return AttachCurrentThread();
}

// Initialize the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeInitVsync(JNIEnv *env,
                                                        jobject thiz,
                                                        jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
}

// Clean up the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeCleanupVsync(JNIEnv *env,
                                                           jobject thiz,
                                                           jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
}

// Blocks until the next time a VSync event occurs.
void WaitForVsync() {
  // TODO: Write STDLIB version
}

// Receive native vsync updates from the choreographer, and use them to
// signal starting a frame update and render.
// Note that this callback is signaled from another thread, and so
// needs to be thread-safe.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeOnVsync(JNIEnv *env, jobject thiz,
                                                      jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  CallVsyncCallback();
}

int GetVsyncFrameId() {
  // TODO: Write STDLIB version
  return 0;
}

#endif  // __ANDROID__

}  // namespace fplbase
