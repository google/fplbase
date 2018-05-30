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

#include "fplbase/logging.h"
#include "SDL_log.h"

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

}  // namespace fplbase
