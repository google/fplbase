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

const int kDefaultDesktopVersion = 120;  // Use GLSL 1.20 by default on desktop.
const char kVersionTag[] = "version";
const char kExtensionTag[] = "extension";
const char kIfTag[] = "if";  // This will catch all flavors.
const char kElseTag[] = "else";
const char kElseIfTag[] = "elif";
const char kEndIfTag[] = "endif";
const size_t kVersionTagLength = sizeof(kVersionTag) - 1;
const size_t kExtensionTagLength = sizeof(kExtensionTag) - 1;
const size_t kIfTagLength = sizeof(kIfTag) - 1;
const size_t kElseTagLength = sizeof(kElseTag) - 1;
const size_t kElseIfTagLength = sizeof(kElseIfTag) - 1;
const size_t kEndIfTagLength = sizeof(kEndIfTag) - 1;

#ifdef PLATFORM_MOBILE
const char kDefaultShaderPrefix[] =
    "#ifdef GL_ES\n"
    "precision highp float;\n"
    "#endif\n";
#else
const char kDefaultShaderPrefix[] =
    "#define lowp\n"
    "#define mediump\n"
    "#define highp\n";
#endif

struct Substring {
  Substring() : start(nullptr), len(0) {}
  Substring(const char *start, size_t len) : start(start), len(len) {}

  const char *start;
  size_t len;
};

struct Condition {
  Condition() {}
  Condition(const char *start, size_t len) : if_expr(start, len) {}

  Substring if_expr;
  Substring else_expr;
};

typedef std::deque<Condition> Context;

struct Extension {
  Extension() {}
  Extension(const char *start, size_t len, const Context &context)
      : code(start, len), context(context) {}

  Substring code;
  Context context;
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
  // Newlines are \n, \r, \r\n or \n\r.
  ptr += strcspn(ptr, "\n\r");
  ptr += strspn(ptr, "\n\r");
  return ptr;
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

// If specified version isn't for current platform, try to convert it.
void ConvertVersion(int *version_number, bool *version_es) {
#ifdef PLATFORM_MOBILE
  if (!*version_es) {
    *version_number = MobileFromDesktopVersion(*version_number);
    *version_es = true;
  }
#else
  if (*version_es) {
    *version_number = DesktopFromMobileVersion(*version_number);
    *version_es = false;
  }
#endif
}

// Appends a version number and type to a string.
void AppendVersion(int version_number, bool version_es, std::string *result) {
  result->append("#")
      .append(kVersionTag)
      .append(" ")
      .append(flatbuffers::NumToString(version_number))
      .append(version_es ? " es\n" : "\n");
}

// Appends the default version for the current platform, if any.
void AppendDefaultVersion(std::string *result) {
#ifndef PLATFORM_MOBILE
  AppendVersion(kDefaultDesktopVersion, false, result);
#endif
}

// Appends a context's series of #if/else conditions to a string.
void AppendContextPrefix(const Context &context, std::string *result) {
  for (size_t i = 0; i < context.size(); ++i) {
    const auto& cond = context[i];
    result->append(cond.if_expr.start, cond.if_expr.len);

    // Immediately add the else expression.  We only care about the last one
    // before our contents.
    if (cond.else_expr.len != 0) {
      result->append(cond.else_expr.start, cond.else_expr.len);
    }
  }
}

// Appends a context's series of #endifs to a string.
void AppendContextSuffix(const Context &context, std::string *result) {
  for (size_t i = 0; i < context.size(); ++i) {
    result->append("#endif\n");
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
  // - Any #extension directives must be before any code, which includes the
  //   precision specifier that we automatically add on mobile platforms.
  //   Unfortunately this means we need to ensure that #extension directives are
  //   correctly placed, preserving their context as we do so.
  // - Only whitespace is allowed before #.
  std::vector<Extension> extensions;
  std::vector<Substring> code;
  Context context;
  int version_number = 0;
  bool version_es = false;
  const char *comment_start = nullptr;

  // Iterate through each line, stripping #version and #extension lines from the
  // rest of the code.
  for (const GLchar *line = csource, *next_line = nullptr; line && *line;
       line = next_line) {
    // If we start the line in a /* */ comment, skip across lines until it ends.
    if (comment_start) {
      const char *comment_end = strstr(line, "*/");
      if (comment_end) {
        next_line = comment_end + 2;
        AppendSubstring(comment_start, next_line - comment_start, &code);
      } else {
        next_line = nullptr;
      }
      comment_start = nullptr;
      continue;
    }

    const char *start = SkipWhitespaceInLine(line);
    next_line = FindNextLine(start);

    // Single-line comment; just skip this line.
    const bool is_single_line_comment = start[0] == '/' && start[1] == '/';

    size_t line_len = next_line - line;

    // Check if there's an unterminated /* */ comment on this line.  If so,
    // end the line there and remember that we're inside a comment.
    if (!is_single_line_comment) {
      comment_start = FindUnterminatedCommentInLine(start, next_line - start);
      if (comment_start) {
        line_len = comment_start - line;
      }
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

        const char *version_str = directive + kVersionTagLength;
        if (sscanf(version_str, " %d es", &version_number) == 1) {
          version_es = true;
        } else if (sscanf(version_str, " %d ", &version_number) != 1) {
          LogError("Invalid version identifier: %s", version_str);
        }
        continue;
      }

      if (strncmp(directive, kExtensionTag, kExtensionTagLength) == 0) {
        extensions.push_back(Extension(line, line_len, context));
        continue;
      }

      // Update the current context.
      if (strncmp(directive, kIfTag, kIfTagLength) == 0) {
        context.push_back(Condition(line, line_len));
      } else if (!context.empty() &&
                 (strncmp(directive, kElseTag, kElseTagLength) == 0 ||
                  strncmp(directive, kElseIfTag, kElseIfTagLength) == 0)) {
        // Replace any existing else expression, since we only care about the
        // latest one.
        context.back().else_expr = Substring(line, line_len);
      } else if (strncmp(directive, kEndIfTag, kEndIfTagLength) == 0) {
        context.pop_back();
      }
    } else if (context.empty() && !is_single_line_comment &&
               !IsEmptyLine(start)) {
      // We've found the first line of code.  Since there can be no #version or
      // #extension directives from this point on, we're done.
      AppendSubstring(line, strlen(line), &code);
      break;
    }

    AppendSubstring(line, line_len, &code);
  }

  // Time to write the file.
  result->clear();

  // Version number must come first.
  if (version_number != 0) {
    ConvertVersion(&version_number, &version_es);
    AppendVersion(version_number, version_es, result);
  } else {
    AppendDefaultVersion(result);
  }

  // Next add extension lines, preserving order and context.
  for (size_t i = 0; i < extensions.size(); ++i) {
    AppendContextPrefix(extensions[i].context, result);
    const auto& line = extensions[i].code;
    result->append(line.start, line.len);
    AppendContextSuffix(extensions[i].context, result);
  }

  // Add per-platform definitions.
  result->append(kDefaultShaderPrefix);

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
