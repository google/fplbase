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

#ifndef FPLBASE_LOGGING_H
#define FPLBASE_LOGGING_H

#include "fplbase/config.h"  // Must come first.

#include <stdarg.h>

namespace fplbase {

/// @brief Constants for use with LogInfo, LogError, etc.
#ifdef FPLBASE_BACKEND_SDL
enum LogCategory {
  kApplication = 0,  // SDL_LOG_CATEGORY_APPLICATION
  kError = 1,        // SDL_LOG_CATEGORY_ERROR
  kSystem = 3,       // SDL_LOG_CATEGORY_SYSTEM
  kAudio = 4,        // SDL_LOG_CATEGORY_AUDIO
  kVideo = 5,        // SDL_LOG_CATEGORY_VIDEO
  kRender = 6,       // SDL_LOG_CATEGORY_RENDER
  kInput = 7,        // SDL_LOG_CATEGORY_INPUT
  kCustom = 19,      // SDL_LOG_CATEGORY_CUSTOM
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

/// @brief Log a format string with `Info` priority to the console.
/// @param[in] fmt A C-string format string.
/// @param[in] args A variable length argument list for the format
/// string `fmt`.
void LogInfo(const char *fmt, va_list args);

/// @brief Log a format string with `Error` priority to the console.
/// @param[in] fmt A C-string format string.
/// @param[in] args A variable length argument list for the format
/// string `fmt`.
void LogError(const char *fmt, va_list args);

/// @brief Log a format string with `Info` priority to the console.
/// @param[in] category The LogCategory for the message.
/// @param[in] fmt A C-string format string.
/// @param[in] args A variable length argument list for the format
/// string `fmt`.
void LogInfo(LogCategory category, const char *fmt, va_list args);

/// @brief Log a format string with `Error` priority to the console.
/// @param[in] category The LogCategory for the message.
/// @param[in] fmt A C-string format string.
/// @param[in] args A variable length argument list for the format
/// string `fmt`.
void LogError(LogCategory category, const char *fmt, va_list args);

/// @brief Log a format string with `Info` priority to the console.
/// @param[in] fmt A C-string format string.
void LogInfo(const char *fmt, ...);

/// @brief Log a format string with `Error` priority to the console.
/// @param[in] fmt A C-string format string.
void LogError(const char *fmt, ...);

/// @brief Log a format string with `Info` priority to the console.
/// @param[in] category The LogCategory for the message.
/// @param[in] fmt A C-string format string.
void LogInfo(LogCategory category, const char *fmt, ...);

/// @brief Log a format string with `Error` priority to the console.
/// @param[in] category The LogCategory for the message.
/// @param[in] fmt A C-string format string.
void LogError(LogCategory category, const char *fmt, ...);

}  // namespace fplbase

#endif  // FPLBASE_LOGGING_H
