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

#include "fplbase/preprocessor.h"
#include <stack>
#include <unordered_set>
#include "fplbase/fpl_common.h"
#include "precompiled.h"

namespace fplbase {

bool LoadFileWithDirectivesHelper(
    const char *filename, std::string *dest, std::string *error_message,
    std::set<std::string> *all_includes,
    const char *const *defines) {
  if (!LoadFile(filename, dest)) {
    *error_message = std::string("cannot load ") + filename;
    return false;
  }

  // Add the #defines.
  if (defines) {
    std::string to_insert;
    for (auto defs = defines; *defs; defs++) {
      if (**defs) {  // Skip empty strings.
        to_insert.append("#define ").append(*defs).append("\n");
      }
    }
    if (to_insert != "") {
      dest->insert(0, to_insert);
    }
  }

  all_includes->insert(filename);
  std::vector<std::string> includes;
  auto cursor = dest->c_str();
  static auto kIncludeStatement = "#include";
  auto include_len = strlen(kIncludeStatement);
  size_t insertion_point = 0;
  // Parse only lines that are include statements, skipping everything else.
  while (*cursor) {
    cursor += strspn(cursor, " \t");  // Skip whitespace.
    auto start = cursor;
    if (strncmp(cursor, kIncludeStatement, include_len) == 0) {
      cursor += include_len;
      cursor += strspn(cursor, " \t");  // Skip whitespace.
      if (*cursor == '\"') {            // Must find quote.
        cursor++;
        auto len = strcspn(cursor, "\"\n\r");  // Filename part.
        if (cursor[len] == '\"') {             // Ending quote.
          includes.push_back(std::string(cursor, len));
          cursor += len + 1;
          insertion_point = start - dest->c_str();
          dest->erase(insertion_point, cursor - start);
          cursor = start;
          cursor += strspn(cursor, "\n\r \t");  // Skip whitespace and newline.
          continue;
        }
      }
    }
    // Something else, skip it.
    cursor += strcspn(cursor, "\n\r");  // Skip all except newline;
    cursor += strspn(cursor, "\n\r");   // Skip newline;
  }

  // Now insert the includes.
  std::string include;
  for (auto it = includes.begin(); it != includes.end(); ++it) {
    if (all_includes->find(*it) == all_includes->end()) {
      if (!LoadFileWithDirectivesHelper(it->c_str(), &include, error_message,
                                        all_includes, nullptr)) {
        return false;
      }
      dest->insert(insertion_point, include);
      insertion_point += include.length();
      // Ensure there's a linefeed at eof.
      if (include.size() && include.back() != '\n') {
        dest->insert(insertion_point++, "\n");
      }
    }
  }
  return true;
}

bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            const char *const *defines,
                            std::string *error_message) {
  std::set<std::string> all_includes;
  return LoadFileWithDirectivesHelper(filename, dest, error_message,
                                      &all_includes, defines);
}

bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            std::string *error_message) {
  return LoadFileWithDirectives(filename, dest, nullptr, error_message);
}
}  // namespace fplbase
