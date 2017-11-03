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

#if defined(__ANDROID__)

#include <android/log.h>

namespace {
static AAssetManager *g_asset_manager = nullptr;
}

#endif  // defined(__ANDROID__)

// clang-format off
#if defined(__APPLE__)
#include "TargetConditionals.h"
#include <CoreFoundation/CoreFoundation.h>
#endif  // defined(__APPLE__)
// clang-format on

namespace fplbase {

bool LoadFileRaw(const char *filename, std::string *dest) {
#if defined(__ANDROID__)
  // Don't try to load absolute file paths through the asset manager.
  if (filename[0] != '/') {
    if (!g_asset_manager) {
      LogError(kError,
               "Need to call SetAssetManager() once before calling LoadFile()");
      assert(false);
    }
    AAsset *asset =
        AAssetManager_open(g_asset_manager, filename, AASSET_MODE_STREAMING);
    if (!asset) {
      LogError(kError, "LoadFile fail on %s", filename);
      return false;
    }
    off_t len = AAsset_getLength(asset);
    dest->assign(len, 0);
    int rlen = AAsset_read(asset, &(*dest)[0], len);
    AAsset_close(asset);
    return len == rlen && len > 0;
  }
#endif  // defined(__ANDROID__)

  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  if (fseek(fd, 0, SEEK_END)) {
    fclose(fd);
    return false;
  }
  size_t len = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  dest->assign(len, 0);
  size_t rlen = fread(&(*dest)[0], 1, len, fd);
  fclose(fd);
  return len == rlen && len > 0;
}

bool SaveFile(const char *filename, const void *data, size_t size) {
#if defined(__ANDROID__)
  (void)filename;
  (void)data;
  (void)size;
  LogError(kError, "SaveFile unimplemented on STDLIB on ANDROID.");
  return false;
#else
  FILE *fd = fopen(filename, "wb");
  if (fd == NULL) {
    LogError(kError, "SaveFile fail on %s", filename);
    return false;
  }
  size_t wlen = fwrite(data, 1, size, fd);
  fclose(fd);
  return size == wlen && size > 0;
#endif
}

bool ChangeToUpstreamDir(const char *const binary_dir,
                         const char *const target_dir) {
#if defined(__APPLE__)
  (void)binary_dir;
  // Get the root of the target directory from the Bundle instead of using the directory
  // specified by the client.
  {
    CFBundleRef main_bundle = CFBundleGetMainBundle();
    CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
    char path[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(
            resources_url, true, reinterpret_cast<UInt8*>(path), PATH_MAX)) {
      LogError(kError, "Could not set the bundle directory");
      return false;
    }
    CFRelease(resources_url);
    int retval = chdir(path);
    if (retval == 0) {
      retval = chdir(target_dir);
    }
    return (retval == 0);
  }
#else
  extern bool ChangeToUpstreamDirDesktop(const char *const binary_dir,
                                         const char *const target_dir);
  return ChangeToUpstreamDirDesktop(binary_dir, target_dir);
#endif
}

int32_t GetSystemRamSize() {
  return 0;
}

#if defined(__ANDROID__)
void LogInfo(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogInfo(fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogError(fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  __android_log_vprint(ANDROID_LOG_VERBOSE, "fplbase", fmt, args);
}

void LogError(const char *fmt, va_list args) {
  __android_log_vprint(ANDROID_LOG_ERROR, "fplbase", fmt, args);
}

#else
void LogInfo(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogInfo(fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  (void)category;
  LogError(fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  vprintf(fmt, args);
  printf("\n");
}

void LogError(const char *fmt, va_list args) {
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}
#endif

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

void SetAAssetManager(AAssetManager *manager) { g_asset_manager = manager; }

AAssetManager *GetAAssetManager() { return g_asset_manager; }

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
