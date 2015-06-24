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

#ifndef FPLBASE_UTILITIES_H
#define FPLBASE_UTILITIES_H

#include "fplbase/config.h" // Must come first.

#include <string>
#include "mathfu/utilities.h"

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_STDLIB)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#endif

namespace fpl {

// General utility functions, used by FPLBase, and that might be of use to
// people using the library:
// Constants for use with LogInfo, LogError, etc.
#ifdef FPL_BASE_BACKEND_SDL
enum LogCategory {
  kApplication = 0,  // SDL_LOG_CATEGORY_APPLICATION
  kError       = 1,  // SDL_LOG_CATEGORY_ERROR
  kSystem      = 3,  // SDL_LOG_CATEGORY_SYSTEM
  kAudio       = 4,  // SDL_LOG_CATEGORY_AUDIO
  kVideo       = 5,  // SDL_LOG_CATEGORY_VIDEO
  kRender      = 6,  // SDL_LOG_CATEGORY_RENDER
  kInput       = 7,  // SDL_LOG_CATEGORY_INPUT
  kCustom      = 19, // SDL_LOG_CATEGORY_CUSTOM
};
#else
enum LogCategory {
  kApplication = 0,
  kError,
  kSystem,
  kAudio,
  kVideo,
  kRender,
  kInput,
  kCustom
};
#endif

typedef int32_t WorldTime;

const int kMillisecondsPerSecond = 1000;

// Loads a file and returns its contents via string pointer.
bool LoadFile(const char* filename, std::string* dest);

// Save a string to a file, overwriting the existing contents.
bool SaveFile(const char* filename, const std::string& data);

// Save binary data to a file, overwriting the existing contents.
bool SaveFile(const char* filename, const void* data, size_t size);

// Search up the directory tree from binary_dir for target_dir, changing the
// working directory to the target_dir and returning true if it's found,
// false otherwise.
bool ChangeToUpstreamDir(const char* const binary_dir,
                         const char* const target_dir);

// Returns true if 16bpp MipMap generation is supported.
// (Basically always true, except on certain android devices.)
bool MipmapGeneration16bppSupported();

// Basic logging functions.  They will output to the console.
void LogInfo(const char* fmt, ...);
void LogError(const char* fmt, ...);
void LogInfo(LogCategory category, const char* fmt, ...);
void LogError(LogCategory category, const char* fmt, ...);

// Returns the current time, in milliseconds.
WorldTime GetTicks();

// Delays (sleeps) for the specified number of milliseconds.
void Delay(WorldTime time);

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_SDL)
// Returns a pointer to the Java instance of the activity class
// in an Android application.
void* AndroidGetActivity();

// Returns a pointer to the Java native interface object (JNIEnv) of the
// current thread on an Android application.
void* AndroidGetJNIEnv();
#endif  // __ANDROID__ && FPL_BASE_BACKEND_SDL

#if defined(__ANDROID__) && defined(FPL_BASE_BACKEND_STDLIB)
// Provide a pointer to an already-created instance of AAssetManager. Must call
// this function once before loading any assets.
void SetAAssetManager(AAssetManager* manager);
#endif

}  // namespace fpl

#endif  // FPLBASE_UTILITIES_H
