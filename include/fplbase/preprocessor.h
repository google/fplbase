// Copyright 2016 Google Inc. All rights reserved.
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

#ifndef FPLBASE_PREPROCESSOR_H
#define FPLBASE_PREPROCESSOR_H

#include "fplbase/utilities.h"

namespace fplbase {
/// @brief Load a file like `LoadFile()`, but scan for directives.
///
/// Supported directives include #include, #define, #ifdef, #ifndef, #else,
/// and #endif.
/// @param[in] filename A UTF-8 C-string representing the file to load.
/// @param[out] dest A pointer to a `std::string` to capture the output of
/// the file.
/// @param[out] error_message A pointer to a `std::string` that captures an
/// error message (if the function returned `false`, indicating failure).
/// @return If this function returns false, `error_message` indicates which
/// directive caused the problem and why.
bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            std::string *error_message);
}

#endif // FPLBASE_PREPROCESSOR_H