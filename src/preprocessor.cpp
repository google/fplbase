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

// Enum to identify #directives.
enum PreprocessorDirective {
  kInclude,
  kDefine,
  kIfDef,
  kIfNDef,
  kElse,
  kEndIf,
  kNumPreprocessorDirectives,
  kUnknownDirective  // Invalid directive
};

// Strings corresponding to each directive.
static const char *kDirectiveText[] = {
    "#include",  // kInclude
    "#define",   // kDefine
    "#ifdef",    // kIfDef
    "#ifndef",   // kIfNDef
    "#else",     // kElse
    "#endif"     // kEndIf
};

// Identifiers we want to pass thru.
static const char *kWhiteListed[] = {
    "__LINE__",
    "__FILE__",
    "__VERSION__",
    "GL_ES"
};

static_assert(
    FPL_ARRAYSIZE(kDirectiveText) == kNumPreprocessorDirectives,
    "all PreprocessorDirectives should have kDirectiveText equivalent.");

// Struct to keep track of nested #ifdef/#ifndef/#else statements.
struct IfStackItem {
  IfStackItem() : compiled(false), was_true(false), else_seen(false),
                  pass_thru(false) {}

  // Was this #if evaluated while the file was compiling?
  bool compiled;

  // Was this #if evaluated to be true?
  bool was_true;

  // Has an #else corresponding to this #if been seen?
  bool else_seen;

  // This #ifdef should be passed thru to the underlying compiler.
  bool pass_thru;
};

// Returns a constant identifying which directive has been called.
PreprocessorDirective FindDirective(const char *cursor,
                                    size_t directive_length) {
  for (size_t i = 0; i < kNumPreprocessorDirectives; ++i) {
    if (strncmp(kDirectiveText[i], cursor, directive_length) == 0) {
      return static_cast<PreprocessorDirective>(i);
    }
  }
  return kUnknownDirective;
}

bool FindWhiteListed(const std::string &def) {
  for (size_t i = 0; i < sizeof(kWhiteListed) / sizeof(const char *); ++i) {
    if (def == kWhiteListed[i]) return true;
  }
  return false;
}

// Helper function to determine if an identifier has been defined.
bool FindIdentifier(const std::string &term,
                    const std::unordered_set<std::string> &all_defines) {
  auto it = all_defines.find(term);
  return it != all_defines.end();
}

const char *SkipNewline(const char *cursor) {
  cursor += strcspn(cursor, "\n\r");  // Skip all except newline
  cursor += strspn(cursor, "\n\r");   // Skip newline
  return cursor;
}

const char *SkipWhitespace(const char *cursor) {
  cursor += strspn(cursor, " \t");  // Skip whitespace.
  return cursor;
}

bool LoadFileWithDirectivesHelper(
    const char *filename, std::string *dest, std::string *error_message,
    std::set<std::string> *all_includes,
    std::unordered_set<std::string> *all_defines) {
  if (!LoadFile(filename, dest)) {
    *error_message = "Error loading file: ";
    *error_message += filename;
    return false;
  }
  auto cursor = dest->c_str();

  // Whether or not the line being parsed should be considered when compiling
  // the final file. If the line is within an #if block that evaluates to
  // false, then compiling=false.
  bool compiling = true;

  std::stack<IfStackItem> if_stack;
  all_includes->insert(filename);
  std::vector<std::string> includes;

  for (;;) {
    if (!cursor[0] || strcspn(cursor, "\n\r") == 0) {
      break;
    }
    auto start = cursor;
    cursor = SkipWhitespace(cursor);

    bool remove_line = false;  // Should line be removed from output?

    if (cursor[0] == '#') {
      remove_line = true;

      bool skip_line = false;  // Should like be evaluated?

      size_t directive_length = strcspn(cursor, " \t\n\r");
      PreprocessorDirective directive = FindDirective(cursor, directive_length);

      if (directive == kUnknownDirective) {
        *error_message =
            "Unknown directive: " + std::string(start, directive_length);
        return false;
      }

      if (!compiling) {  // Within an #if statement that evaluated to false.
        IfStackItem item;
        switch (directive) {
          case kElse:
          case kEndIf:
            // Could activate compilation, so evaluate these statements.
            break;
          case kIfDef:
          case kIfNDef:
            // Can't activate compilation, but should be added to stack.
            if_stack.push(item);
            skip_line = true;
            break;
          default:
            // Not compiling, so skip the line.
            skip_line = true;
            break;
        }
      }

      if (!skip_line) {
        IfStackItem item;

        // Used to determine if an identifier has been defined
        bool found = false;
        // Whether or not the directive wants the identifier to be found
        bool enable_if_found = directive == kIfDef;

        cursor += directive_length;

        switch (directive) {
          case kIfDef:
          case kIfNDef: {
            item.compiled = true;
            cursor = SkipWhitespace(cursor);
            auto arg1_length = strcspn(cursor, " \n\r\t");
            auto arg1 = std::string(cursor, arg1_length);
            cursor += arg1_length;

            if (FindWhiteListed(arg1)) {
              item.pass_thru = true;
              remove_line = false;
            } else {
              found = FindIdentifier(arg1, *all_defines);
              if (found == enable_if_found) {
                compiling = true;
                item.was_true = true;
              } else {
                compiling = false;
              }
            }
            if_stack.push(item);
            break;
          }
          case kElse:
            // There should be a corresponding #if for this #else.
            assert(!if_stack.empty());
            // There shouldn't already be an #else.
            assert(!if_stack.top().else_seen);

            if_stack.top().else_seen = true;

            if (if_stack.top().pass_thru) {
              remove_line = false;
            } else {
              // Else should be the opposite of the #if
              compiling = if_stack.top().compiled && !if_stack.top().was_true;
            }
            break;
          case kEndIf:
            assert(!if_stack.empty());

            if (if_stack.top().pass_thru) {
              remove_line = false;
            } else {
              if (!compiling && if_stack.top().compiled) {
                // The #if statement evaluated to false and is at bottom of stack
                compiling = true;
              }
            }
            if_stack.pop();
            break;
          case kDefine: {
            cursor = SkipWhitespace(cursor);
            auto arg1_length = strcspn(cursor, " \t\n\r");

            if (arg1_length == 0) {
              *error_message = "#define must be followed by an identifier.";
              return false;
            }

            // Term to define
            auto arg1 = std::string(cursor, arg1_length);
            cursor += arg1_length;
            cursor += strspn(cursor, " \t");

            if (strcspn(cursor, "\n\r") > 0) {
              // A #define with arguments, pass thru to the underlying compiler.
              remove_line = false;
            } else {
              all_defines->insert(arg1);
            }
            break;
          }
          case kInclude:
            cursor = SkipWhitespace(cursor);
            if (*cursor == '\"') {  // Must find quote.
              cursor++;
              auto len = strcspn(cursor, "\"\n\r");  // Filename part.
              if (cursor[len] == '\"') {             // Ending quote.
                includes.push_back(std::string(cursor, len));
                cursor += len + 1;
              }
            }
            break;
          default:
            // kNumPreprocessorDirectives, which FindIdentifier should never
            // return.
            assert(false);
        }
      }
    } else if (!compiling) {
      remove_line = true;
    } else {
      // Non-directive line that ends up in final file.
    }
    cursor = SkipNewline(cursor);
    if (remove_line) {
      // Remove #directive line from final file.
      dest->erase(start - dest->c_str(), cursor - start);
      cursor = start;
    }
  }

  if (!if_stack.empty()) {
    *error_message =
        "All #if (#ifdef, #ifndef) statements must have a "
        "corresponding #endif statement.";
    return false;
  }

  // Early out for files with no includes.
  if (!includes.size()) return true;

  // Now insert the includes.
  std::string include;
  size_t insertion_point = 0;
  for (auto it = includes.begin(); it != includes.end(); ++it) {
    if (all_includes->find(*it) == all_includes->end()) {
      if (!LoadFileWithDirectivesHelper(it->c_str(), &include, error_message,
                                        all_includes, all_defines)) {
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
                            const char **defines, std::string *error_message) {
  std::set<std::string> all_includes;
  std::unordered_set<std::string> all_defines;

  if (defines != nullptr) {
    for (const char **d = defines; *d != nullptr; ++d) {
      all_defines.insert(std::string(*d));
    }
  }
  return LoadFileWithDirectivesHelper(filename, dest, error_message,
                                      &all_includes, &all_defines);
}

bool LoadFileWithDirectives(const char *filename, std::string *dest,
                            std::string *error_message) {
  return LoadFileWithDirectives(filename, dest, nullptr, error_message);
}
}  // namespace fplbase
