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

const char kDefaultDesktopVersion[] = "120";
const char kVersionTag[] = "version";
const char kExtensionTag[] = "extension";
const char kIfTag[] = "if";  // This will catch all flavors.
const char kEndIfTag[] = "endif";
const size_t kVersionTagLength = sizeof(kVersionTag) - 1;
const size_t kExtensionTagLength = sizeof(kExtensionTag) - 1;
const size_t kIfTagLength = sizeof(kIfTag) - 1;
const size_t kEndIfTagLength = sizeof(kEndIfTag) - 1;

// Default definitions which get placed at the top of a shader (but after any
// #version specifier).
const char kDefaultDefines[] =
    "#ifndef GL_ES\n"
    "#define lowp\n"
    "#define mediump\n"
    "#define highp\n"
    "#endif\n";

// This defines a default float precision for GL ES shaders. Without this, all
// float-based variables would require their own precision specifier. It's safe
// to have several of these in a file, so inserting one should be innocuous.
const char kDefaultPrecisionSpecifier[] =
    "#ifdef GL_ES\n"
    "precision highp float;\n"
    "#endif\n";

struct Substring {
  Substring() : start(nullptr), len(0) {}
  Substring(const char *start, size_t len) : start(start), len(len) {}

  const char *start;
  size_t len;
};

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
  // Newlines are \n, \r, \r\n or \n\r, except when immediately preceded by a
  // backslash.
  const char kNewlineCharacterSet[] = "\n\r";
  const char *next_line = ptr + strcspn(ptr, kNewlineCharacterSet);
  while (next_line > ptr && next_line[-1] == '\\') {
    next_line += strspn(next_line, kNewlineCharacterSet);
    next_line += strcspn(next_line, kNewlineCharacterSet);
  }
  next_line += strspn(next_line, kNewlineCharacterSet);
  return next_line;
}

bool IsEmptyLine(const char *ptr) {
  return strcspn(ptr, "\n\r") == 0;
}

// Returns the start of a multiline comment within a line.
const char *FindUnterminatedCommentInLine(const char *line, size_t line_len) {
  // Search backwards.  If we find /*, return its location unless we've already
  // seen */.
  for (const char *ptr = line + line_len - 1; ptr > line; --ptr) {
    if (ptr[-1] == '*' && ptr[0] == '/') {
      return nullptr;
    }
    if (ptr[-1] == '/' && ptr[0] == '*') {
      return ptr - 1;
    }
  }

  return nullptr;
}

// Appends a substring to a list of substrings, merging if possible.
void AppendSubstring(const char *start, size_t len,
                     std::vector<Substring> *list) {
  if (!list->empty() && list->back().start + list->back().len == start) {
    // Merge with previous line.
    list->back().len += len;
  } else {
    list->push_back(Substring(start, len));
  }
}

// If |version| doesn't match |profile|, try to convert it.
void ConvertVersion(ShaderProfile profile, int *version_number,
                    bool *version_es) {
  const bool profile_es = profile == kShaderProfileEs;
  if (profile_es == *version_es) {
    return;
  }
  if (profile_es) {
    *version_number = MobileFromDesktopVersion(*version_number);
  } else {
    *version_number = DesktopFromMobileVersion(*version_number);
  }
  *version_es = profile_es;
}

// Appends a version to a string.
void AppendVersion(const char *version_string, std::string *result) {
  result->append("#")
      .append(kVersionTag)
      .append(" ")
      .append(version_string)
      .append("\n");
}

// Appends a version number and type to a string.
void AppendVersion(int version_number, bool version_es, std::string *result) {
  std::string version_string = flatbuffers::NumToString(version_number);
  if (version_es) {
    version_string.append(" es");
  }
  AppendVersion(version_string.c_str(), result);
}

// Appends the default version for |profile|, if any.
void AppendDefaultVersion(ShaderProfile profile, std::string *result) {
  if (profile == kShaderProfileCore) {
    AppendVersion(kDefaultDesktopVersion, result);
  }
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
  static const char kIncludeStatement[] = "#include";
  auto include_len = sizeof(kIncludeStatement) - 1;
  size_t insertion_point = 0;
  // Parse only lines that are include statements, skipping everything else.
  while (*cursor) {
    cursor = SkipWhitespaceInLine(cursor);
    auto start = cursor;
    if (strncmp(cursor, kIncludeStatement, include_len) == 0) {
      cursor += include_len;
      cursor = SkipWhitespaceInLine(cursor);
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
    cursor = FindNextLine(cursor);
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
                                  ShaderProfile profile, std::string *result) {
  assert(csource);
  assert(result);

  // This function does several things to try to meet both GLSL and GLSL-ES
  // specs using the same source:
  // 1. Keep #version directives first, and translate version numbers between
  //    the two standards since they use different numbering schemes.
  // 2. Add preprocessor definitions next so they can be used by the following
  //    code.
  // 3. Identify the first non-empty, non-comment, non-preprocessor line, and
  //    insert a default precision float specifier before it.

  std::vector<Substring> preamble;
  const char* precision_insertion_ptr = csource;
  int if_depth = 0;
  int version_number = 0;
  bool version_es = false;
  const char *comment_start = nullptr;
  bool found_default_precision = false;

  // Iterate through each line until we find the first non-empty, non-comment,
  // non-preprocessor line. Strip out the #version line (we'll manually add it),
  // and remember lines we've seen so they can be added before
  // kDefaultPrecisionSpecifier.
  // TODO(ahynek) pull out line iteration into a separate function
  for (const GLchar *line = csource, *next_line = nullptr; line && *line;
       line = next_line) {
    // If we start the line in a /* */ comment, skip across lines until it ends.
    if (comment_start) {
      const char *comment_end = strstr(line, "*/");
      if (comment_end) {
        next_line = comment_end + 2;
        AppendSubstring(comment_start, next_line - comment_start, &preamble);
      } else {
        next_line = nullptr;
      }
      comment_start = nullptr;
      continue;
    }

    const char *start = SkipWhitespaceInLine(line);
    next_line = FindNextLine(start);

    if (if_depth == 0) {
      precision_insertion_ptr = line;
    }

    // Single-line comment; just skip this line.
    const bool is_single_line_comment = start[0] == '/' && start[1] == '/';
    if (is_single_line_comment) {
      AppendSubstring(line, next_line - line, &preamble);
      continue;
    }

    size_t line_len = next_line - line;

    // Check if there's an unterminated /* */ comment on this line.  If so,
    // end the line there and remember that we're inside a comment.
    comment_start = FindUnterminatedCommentInLine(start, next_line - start);
    if (comment_start) {
      if (comment_start == start) {
        // Entire line is a comment; reprocess it as such.
        next_line = line;
        continue;
      }
      line_len = comment_start - line;
    }

    // Check for a preprocessor directive.  It can be separated from # by spaces
    // and horizontal tabs.
    if (start[0] == '#') {
      const char *directive = SkipWhitespaceInLine(start + 1);

      if (strncmp(directive, kVersionTag, kVersionTagLength) == 0) {
        if (version_number != 0) {
          LogError("More than one #version found in shader: %s", start);
          continue;
        }
        if (if_depth != 0) {
          LogError("Found #version directive within an #if");
        }

        const char *version_str = directive + kVersionTagLength;
        char spec[3];
        const int num_scanned =
            sscanf(version_str, " %d %2[es]", &version_number, spec);
        if (num_scanned == 2) {
          version_es = true;
        } else if (num_scanned != 1) {
          LogError("Invalid version identifier: %s", version_str);
        }

        // Be sure to skip this line when composing the result.
        precision_insertion_ptr = next_line;
        continue;
      }

      // Check for extension directives: we can't insert precision specifiers
      // before them, even if it means putting them within #if blocks.
      if (strncmp(directive, kExtensionTag, kExtensionTagLength) == 0) {
        precision_insertion_ptr = next_line;
      }

      // Update the #if context.
      if (strncmp(directive, kIfTag, kIfTagLength) == 0) {
        ++if_depth;
      } else if (if_depth > 0 &&
          strncmp(directive, kEndIfTag, kEndIfTagLength) == 0) {
        --if_depth;
      }
    } else {
      if (strncmp(start, "precision", 9) == 0) {
        found_default_precision = true;
      }
      if (!IsEmptyLine(start)) {
        // We've found the first line of code.  Since there can be no #version
        // or #extension directives from this point on, we're done.
        break;
      }
    }

    AppendSubstring(line, line_len, &preamble);
  }

  // Time to write the file.
  result->clear();

  // Version number must come first.
  if (version_number != 0) {
    ConvertVersion(profile, &version_number, &version_es);
    AppendVersion(version_number, version_es, result);
  } else {
    AppendDefaultVersion(profile, result);
  }

  // Add per-platform definitions.
  result->append(kDefaultDefines);

  // Add custom definitions, if any.
  if (defines) {
    for (auto defs = defines; *defs; defs++) {
      if (**defs) {  // Skip empty strings.
        result->append("#define ").append(*defs).append("\n");
      }
    }
  }

  // Add the preamble (lines before any code). Make sure we don't go past
  // precision_insertion_ptr, otherwise we end up with a partial dupe.
  for (size_t i = 0; i < preamble.size(); ++i) {
    if (preamble[i].start >= precision_insertion_ptr) {
      break;
    }
    const size_t max_len = precision_insertion_ptr - preamble[i].start;
    result->append(preamble[i].start, std::min(max_len, preamble[i].len));
  }

  // Add the default precision specifier.
  if (!found_default_precision) {
    result->append(kDefaultPrecisionSpecifier);
  }
  // Add the rest of the code.
  result->append(precision_insertion_ptr);
}

void PlatformSanitizeShaderSource(const char *source,
                                  const char *const *defines,
                                  std::string *result) {
#ifdef FPLBASE_GLES
  const ShaderProfile kProfile = kShaderProfileEs;
#else
  const ShaderProfile kProfile = kShaderProfileCore;
#endif
  PlatformSanitizeShaderSource(source, defines, kProfile, result);
}

void SetShaderVersion(const char *source, const char *version_string,
                      std::string *result) {
  assert(source);
  assert(version_string);
  assert(result);

  const char *comment_start = nullptr;
  Substring existing_version;

  // TODO(ahynek) pull out line iteration into a separate function
  for (const GLchar *line = source, *next_line = nullptr; line && *line;
       line = next_line) {
    // If we start the line in a /* */ comment, skip across lines until it ends.
    if (comment_start) {
      const char *comment_end = strstr(line, "*/");
      next_line = comment_end ? comment_end + 2 : nullptr;
      comment_start = nullptr;
      continue;
    }

    const char *start = SkipWhitespaceInLine(line);
    next_line = FindNextLine(start);

    // Single-line comment; just skip this line.
    const bool is_single_line_comment = start[0] == '/' && start[1] == '/';
    if (is_single_line_comment) {
      continue;
    }

    // Check if there's an unterminated /* */ comment on this line.  If so,
    // end the line there and remember that we're inside a comment.
    comment_start = FindUnterminatedCommentInLine(start, next_line - start);

    // Check for #version directive.  It can be separated from # by spaces
    // and horizontal tabs.
    if (start[0] == '#') {
      const char *directive = SkipWhitespaceInLine(start + 1);
      if (strncmp(directive, kVersionTag, kVersionTagLength) == 0) {
        existing_version.start = line;
        existing_version.len =
            (comment_start ? comment_start : next_line) - line;
        break;
      }
    }
  }

  result->clear();
  AppendVersion(version_string, result);

  if (existing_version.start != nullptr) {
    // Remove the existing version string.
    result->append(source, existing_version.start - source);
    result->append(existing_version.start + existing_version.len);
  } else {
    result->append(source);
  }
}

}  // namespace fplbase
