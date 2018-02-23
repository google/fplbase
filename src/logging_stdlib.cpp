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

#if defined(__ANDROID__)
#include <android/log.h>
#endif  // defined(__ANDROID__)

#include <stdio.h>
#include "fplbase/logging.h"

namespace fplbase {

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

}  // namespace fplbase
