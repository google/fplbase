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

namespace fplbase {

static_assert(kApplication ==
                  static_cast<LogCategory>(SDL_LOG_CATEGORY_APPLICATION),
              "update kApplication");
static_assert(kError == static_cast<LogCategory>(SDL_LOG_CATEGORY_ERROR),
              "update kError");
static_assert(kSystem == static_cast<LogCategory>(SDL_LOG_CATEGORY_SYSTEM),
              "update kSystem");
static_assert(kAudio == static_cast<LogCategory>(SDL_LOG_CATEGORY_AUDIO),
              "update kAudio");
static_assert(kVideo == static_cast<LogCategory>(SDL_LOG_CATEGORY_VIDEO),
              "update kVideo");
static_assert(kRender == static_cast<LogCategory>(SDL_LOG_CATEGORY_RENDER),
              "update kRender");
static_assert(kInput == static_cast<LogCategory>(SDL_LOG_CATEGORY_INPUT),
              "update kInput");
static_assert(kCustom == static_cast<LogCategory>(SDL_LOG_CATEGORY_CUSTOM),
              "update kCustom");

bool FileExistsRaw(const char *filename) {
  auto handle = SDL_RWFromFile(filename, "rb");
  if (!handle) {
    return false;
  }
  SDL_RWclose(handle);
  return true;
}

bool LoadFileRaw(const char *filename, std::string *dest) {
  auto handle = SDL_RWFromFile(filename, "rb");
  if (!handle) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  auto len = static_cast<size_t>(SDL_RWseek(handle, 0, RW_SEEK_END));
  SDL_RWseek(handle, 0, RW_SEEK_SET);
  dest->assign(len, 0);
  size_t rlen = static_cast<size_t>(SDL_RWread(handle, &(*dest)[0], 1, len));
  SDL_RWclose(handle);
  return len == rlen && len > 0;
}

bool SaveFile(const char *filename, const void *data, size_t size) {
  auto handle = SDL_RWFromFile(filename, "wb");
  if (!handle) {
    SDL_LogError(SDL_LOG_CATEGORY_ERROR, "SaveFile fail on %s", filename);
    return false;
  }
  size_t wlen = static_cast<size_t>(SDL_RWwrite(handle, data, 1, size));
  SDL_RWclose(handle);
  return (wlen == size);
}

// Search up the directory tree from binary_dir for target_dir, changing the
// working directory to the target_dir and returning true if it's found,
// false otherwise.
bool ChangeToUpstreamDir(const char *const binary_dir,
                         const char *const target_dir) {
  extern bool ChangeToUpstreamDirDesktop(const char *const binary_dir,
                                         const char *const target_dir);
  return ChangeToUpstreamDirDesktop(binary_dir, target_dir);
}

int32_t GetSystemRamSize() {
  return SDL_GetSystemRAM();
}

void LogInfo(LogCategory category, const char *fmt, va_list args) {
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, args);
}

void LogError(LogCategory category, const char *fmt, va_list args) {
  SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, args);
}

void LogInfo(const char *fmt, va_list args) {
  LogInfo(kApplication, fmt, args);
}

void LogError(const char *fmt, va_list args) {
  LogError(kApplication, fmt, args);
}

#if defined(__ANDROID__)
// This function always returns a pointer to a jobject, but we are returning a
// void* for the same reason SDL does - to avoid having to include the jni
// libraries in this library.  Anything calling this will probably want to
// static cast the return value into a jobject*.
jobject AndroidGetActivity(bool) {
  return reinterpret_cast<jobject>(SDL_AndroidGetActivity());
}

// This function always returns a pointer to a JNIEnv, but we are returning a
// void* for the same reason SDL does - to avoid having to include the jni
// libraries in this library.  Anything calling this will probably want to
// static cast the return value into a JNIEnv*.
JNIEnv *AndroidGetJNIEnv() {
  return reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
}
#endif

bool GetStoragePath(const char *app_name, std::string *path_string) {
#if defined(__ANDROID__)
  auto path = SDL_AndroidGetInternalStoragePath();
#else
  auto path = SDL_GetPrefPath("FPLBase", app_name);
#endif
  if (path == nullptr) {
    return false;
  }
  *path_string = path;
  return true;
}

#ifdef __ANDROID__
// Mutexes and ConditionVariables used for vsync synchonization:
SDL_mutex *frame_id_mutex;
SDL_mutex *vsync_cv_mutex;
SDL_cond *android_vsync_cv;
int vsync_frame_id = 0;

// Initialize the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeInitVsync(JNIEnv *env,
                                                        jobject thiz,
                                                        jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  frame_id_mutex = SDL_CreateMutex();
  vsync_cv_mutex = SDL_CreateMutex();
  android_vsync_cv = SDL_CreateCond();
  vsync_frame_id = 0;
}

// Clean up the Vsync mutexes.  Called by android lifecycle events.
extern "C" JNIEXPORT void JNICALL
Java_com_google_fpl_fplbase_FPLActivity_nativeCleanupVsync(JNIEnv *env,
                                                           jobject thiz,
                                                           jobject activity) {
  (void)env;
  (void)thiz;
  (void)activity;
  SDL_DestroyMutex(frame_id_mutex);
  SDL_DestroyMutex(vsync_cv_mutex);
  SDL_DestroyCond(android_vsync_cv);
}

// Blocks until the next time a VSync event occurs.
void WaitForVsync() {
  SDL_LockMutex(frame_id_mutex);
  int starting_id = vsync_frame_id;
  SDL_UnlockMutex(frame_id_mutex);

  SDL_LockMutex(vsync_cv_mutex);
  // CondWait will normally only wake up when we receive an actual vsync
  // event, but it's not *guaranteed* not to wake up other times as well.
  // It is generally good practice to verify that your wake condition has
  // been satisfied before actually moving on to do work.
  while (starting_id == vsync_frame_id) {
    SDL_CondWait(android_vsync_cv, vsync_cv_mutex);
  }
  SDL_UnlockMutex(vsync_cv_mutex);
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
  SDL_LockMutex(frame_id_mutex);
  vsync_frame_id++;
  SDL_UnlockMutex(frame_id_mutex);
  SDL_CondBroadcast(android_vsync_cv);
}

int GetVsyncFrameId() {
  SDL_LockMutex(frame_id_mutex);
  int return_value = vsync_frame_id;
  SDL_UnlockMutex(frame_id_mutex);
  return return_value;
}

#endif  // __ANDROID__

}  // namespace fplbase
