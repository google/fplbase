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
#include "fplbase/fpl_common.h"
#include "precompiled.h"

namespace fplbase {
namespace {

static const std::set<std::string> kEmptySet;

// Maps desktop GLSL [1] shader to mobile GLSL-ES [2] shader #versions.
// [1] https://www.opengl.org/wiki/Core_Language_(GLSL)
// [2] https://github.com/mattdesl/lwjgl-basics/wiki/GLSL-Versions
struct VersionMapPair {
  int desktop;
  int mobile;
};

const VersionMapPair kVersionMap[] = {
    // desktop, mobile
    {110, 100},
    {330, 300},
};
const size_t kNumMappedVersions = FPL_ARRAYSIZE(kVersionMap);

int DesktopFromMobileVersion(int version) {
  for (size_t i = 0; i < kNumMappedVersions; ++i) {
    if (kVersionMap[i].mobile == version) {
      return kVersionMap[i].desktop;
    }
  }
  LogError("Unknown mobile version %d", version);
  return version;
}

int MobileFromDesktopVersion(int version) {
  for (size_t i = 0; i < kNumMappedVersions; ++i) {
    if (kVersionMap[i].desktop == version) {
      return kVersionMap[i].mobile;
    }
  }
  LogError("Unknown desktop version %d", version);
  return version;
}

const char *SkipWhitespaceInLine(const char *ptr) {
  // Whitespace which doesn't end a line: space, horizontal & vertical tabs.
  // GLSL ES spec: https://www.khronos.org/files/opengles_shading_language.pdf
  return (ptr + strspn(ptr, " \t\v"));
}

const char *FindNextLine(const char *ptr) {
  // Newlines are \n, \r, \r\n or \n\r.
  ptr += strcspn(ptr, "\n\r");
  ptr += strspn(ptr, "\n\r");
  return ptr;
}

}  // namespace

bool LoadFileWithDirectivesHelper(
    const char *filename, std::string *dest, std::string *error_message,
    std::set<std::string> *all_includes,
    const std::set<std::string> &defines) {
  if (!LoadFile(filename, dest)) {
    *error_message = std::string("cannot load ") + filename;
    return false;
  }

  // Add the #defines.
  std::string to_insert;
  for (auto iter = defines.begin(); iter != defines.end(); ++iter) {
    const std::string &define = *iter;
    if (!define.empty()) {  // Skip empty strings.
      to_insert.append("#define ").append(define).append("\n");
    }
  }
  if (to_insert != "") {
    dest->insert(0, to_insert);
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
                                        all_includes, kEmptySet)) {
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
                            const std::set<std::string> &defines,
                            std::string *error_message) {
  std::set<std::string> all_includes;
  return LoadFileWithDirectivesHelper(filename, dest, error_message,
                                      &all_includes, defines);
}

bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            std::string *error_message) {
  return LoadFileWithDirectives(filename, dest, kEmptySet, error_message);
}

bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            const char * const *defines,
                            std::string *error_message) {
  // Convert defines into an set.
  std::set<std::string> defines_set;
  if (defines) {
    for (auto defs = defines; *defs; defs++) {
      if (**defs) {  // Skip empty strings.
        defines_set.insert(std::string(*defs));
      }
    }
  }
  return LoadFileWithDirectives(filename, dest, defines_set, error_message);
}

void PlatformSanitizeShaderSource(const char *csource,
                                  const char *const *defines,
                                  std::string *result) {
  assert(csource);
  assert(result);

  // - If it exists, #version must be first non-empty line.
  // - Any #extension directives must be before any code.
  // - Only whitespace is allowed before #.
  const char *kVersionTag = "#version";
  const char *kExtensionTag = "#extension";
  const size_t kVersionTagLength = strlen(kVersionTag);
  const size_t kExtensionTagLength = strlen(kExtensionTag);

  struct Substring {
    Substring() : start(nullptr), len(0) {}
    Substring(const char *start, size_t len) : start(start), len(len) {}

    const char *start;
    size_t len;
  };
  std::vector<Substring> extensions;
  std::vector<Substring> code;
  int version_number = 0;
  bool version_es = false;

  // Iterate through each line, stripping #version and #extension lines from the
  // rest of the code.
  // TODO(b/32952464) Early-out when we hit the first line of code.
  for (const GLchar *line = csource, *next_line = nullptr; line && *line;
       line = next_line) {
    const GLchar *ptr = SkipWhitespaceInLine(line);
    next_line = FindNextLine(line);
    const size_t len = next_line - line;

    if (strncmp(ptr, kVersionTag, kVersionTagLength) == 0) {
      if (version_number != 0) {
        LogError("More than one #version found in shader: %s", ptr);
        continue;
      }

      ptr += kVersionTagLength;
      if (sscanf(ptr, " %d es", &version_number) == 1) {
        version_es = true;
      } else if (sscanf(ptr, " %d ", &version_number) != 1) {
        LogError("Invalid version identifier: %s", ptr);
      }
    } else if (strncmp(ptr, kExtensionTag, kExtensionTagLength) == 0) {
      extensions.push_back(Substring(line, len));
    } else if (!code.empty() && code.back().start + code.back().len == line) {
      // Merge with previous line.
      code.back().len += len;
    } else {
      code.push_back(Substring(line, len));
    }
  }

  result->clear();

  // If we have a version, put it at the top of the file.
  if (version_number != 0) {
    // If specified version wasn't for current platform, try to convert it.
#ifdef PLATFORM_MOBILE
    if (!version_es) {
      version_number = MobileFromDesktopVersion(version_number);
      version_es = true;
    }
#else
    if (version_es) {
      version_number = DesktopFromMobileVersion(version_number);
      version_es = false;
    }
#endif

    result->append(kVersionTag)
        .append(" ")
        .append(flatbuffers::NumToString(version_number))
        .append(version_es ? " es\n" : "\n");
  } else {
#ifndef PLATFORM_MOBILE
    // Use GLSL 1.20 by default on desktop.
    result->append("#version 120\n");
#endif
  }

  // Next add extension lines, preserving order.
  for (size_t i = 0; i < extensions.size(); ++i) {
    const auto& line = extensions[i];
    result->append(line.start, line.len);
  }

  // Add per-platform definitions.
#ifdef PLATFORM_MOBILE
  result->append("#ifdef GL_ES\nprecision highp float;\n#endif\n");
#else
  result->append("#define lowp\n#define mediump\n#define highp\n");
#endif

  // Add custom definitions, if any.
  if (defines) {
    for (auto defs = defines; *defs; defs++) {
      if (**defs) {  // Skip empty strings.
        result->append("#define ").append(*defs).append("\n");
      }
    }
  }

  // Finally append the rest of the code.
  for (size_t i = 0; i < code.size(); ++i) {
    const auto& block = code[i];
    result->append(block.start, block.len);
  }
}

}  // namespace fplbase
