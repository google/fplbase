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

int32_t GetSystemRamSize() {
  return SDL_GetSystemRAM();
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
