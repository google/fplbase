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

namespace fplbase {

void LogInfo(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogInfo(category, fmt, args);
  va_end(args);
}

void LogError(LogCategory category, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogError(category, fmt, args);
  va_end(args);
}

void LogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogInfo(fmt, args);
  va_end(args);
}

void LogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogError(fmt, args);
  va_end(args);
}

}  // namespace fplbase
