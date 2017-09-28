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

#include <string>
#include "fplbase/preprocessor.h"
#include "gtest/gtest.h"

// The value of preprocessor.cpp's kDefaultDefines as a raw string so it can be
// concatenated in the .inc files.
#define DEFAULT_DEFINES_TEXT \
    "#ifndef GL_ES\n" \
    "#define lowp\n" \
    "#define mediump\n" \
    "#define highp\n" \
    "#endif\n"

// The value of preprocessor.cpp's kDefaultPrecisionSpecifier as a raw string
// so it can be concatenated in the .inc files.
#define DEFAULT_PRECISION_TEXT \
    "#ifdef GL_ES\n" \
    "precision highp float;\n" \
    "#endif\n"

static const std::set<std::string> kEmptyDefines;

class PreprocessorTests : public ::testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  std::string error_message_;
  std::set<std::string> all_includes_;
  std::set<std::string> all_define_;

  std::string file_;
};

// Load a golden file, e.g., take in an entire file and return it instead
// of trying to load an external file.
bool LoadFile(const char* file, std::string* dest) {
  *dest = file;
  return true;
}

void PreprocessorTests::SetUp() { fplbase::SetLoadFileFunction(LoadFile); }

void PreprocessorTests::TearDown() {
  error_message_ = "";
  all_includes_.clear();
  all_define_.clear();

  file_ = "";
}

// #defines should just be passed through.
TEST_F(PreprocessorTests, DefinePassthrough) {
  const char* file = "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// An empty list of defines should also be valid.
TEST_F(PreprocessorTests, EmptyDefineList) {
  const char* file = "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// #defines passed-in should be inserted into the file. Try with just one.
TEST_F(PreprocessorTests, OneDefinePassedIn) {
  std::set<std::string> defines;
  defines.insert("foo");
  bool result =
      fplbase::LoadFileWithDirectives("", &file_, defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo\n"));
}

// #defines passed-in should be inserted into the file. Try with multiple.
TEST_F(PreprocessorTests, MultipleDefinesPassedIn) {
  std::set<std::string> defines;
  defines.insert("foo");
  defines.insert("foo2");
  defines.insert("foo3");
  bool result =
      fplbase::LoadFileWithDirectives("", &file_, defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo\n#define foo2\n#define foo3\n"));
}

// #define the same identifier twice should be ok
TEST_F(PreprocessorTests, DefineSameIdTwice) {
  const char* file =
      "#define foo\n"
      "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// #defines with a value should be passed through.
TEST_F(PreprocessorTests, ValuePassedIn) {
  std::set<std::string> defines;
  defines.insert("foo 1");
  bool result =
      fplbase::LoadFileWithDirectives("", &file_, defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo 1\n"));
}

// #defines with a value should be left alone.
TEST_F(PreprocessorTests, ValuePassthrough) {
  const char* file = "#define foo 1";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

TEST_F(PreprocessorTests, SanitizeCheckPrecisionSpecifiers) {
  const char* simple_file = "void main() { gl_FragColor = something; }";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(simple_file, nullptr, &result);

  const size_t defines_pos = result.find(DEFAULT_DEFINES_TEXT);
  EXPECT_NE(defines_pos, std::string::npos);

  size_t default_pos = result.find(DEFAULT_PRECISION_TEXT);
  EXPECT_NE(default_pos, std::string::npos);

  size_t main_pos = result.find("void main()");
  EXPECT_NE(main_pos, std::string::npos);

  EXPECT_LT(defines_pos, default_pos);
  EXPECT_LT(default_pos, main_pos);

  // Check that the default precision specifier is inserted at the top level.
  const char* if_file =
      "#define TEST_A 1\n"
      "#define TEST_B 0\n"
      "#if TEST_B\n"
      "vec4 do_stuff() { return something; }\n"
      "#else\n"
      "vec4 do_stuff() { return something_else; }\n"
      "#endif\n"
      "void main() { gl_FragColor = do_stuff(); }\n";

  fplbase::PlatformSanitizeShaderSource(if_file, nullptr, &result);

  default_pos = result.find(DEFAULT_PRECISION_TEXT);
  EXPECT_NE(default_pos, std::string::npos);

  const size_t foo_pos = result.find("#if TEST_B");
  EXPECT_NE(foo_pos, std::string::npos);

  EXPECT_LT(default_pos, foo_pos);

  // Test that the specifier is placed correctly if first code is in an #if.
  const char* if_code_test =
      "#if FOO\n"
      "// comment\n"
      "#extension GL_OES_standard_derivatives : enable\n"
      "#endif\n"
      "\n"
      "// comment\n"
      "\n"
      "#if defined(TEXTURE)\n"
      "// comment\n"
      "varying highp vec2 vTexCoord;\n"
      "#endif\n";

  fplbase::PlatformSanitizeShaderSource(if_code_test, nullptr, &result);
  default_pos = result.find(DEFAULT_PRECISION_TEXT);
  EXPECT_GT(default_pos, result.find("#extension"));
  EXPECT_LT(default_pos, result.find("#if defined(TEXTURE)\n"));

  // Test that we don't replace any existing precision default specifier.
  const char* precision_test =
      "#if FOO\n"
      "#extension BAZ\n"
      "precision mediump float;\n"
      "#endif\n"
      "void baz() { gl_FragColor = vec4(1, 1, 1, 1);\n";

  fplbase::PlatformSanitizeShaderSource(precision_test, nullptr, &result);
  EXPECT_EQ(result.find(DEFAULT_PRECISION_TEXT), std::string::npos);
}

TEST_F(PreprocessorTests, SanitizeVersionIsFirstLine) {
  const char* file = "#version 100\n#define foo 1\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);
  EXPECT_EQ(result.find("#version"), 0U);

  EXPECT_EQ(result.find("#version"), result.rfind("#version"));
}

TEST_F(PreprocessorTests, SanitizeVersionConversion) {
  struct ConversionTest {
    const char* file;
    const char* gl_result;
    const char* gles_result;
  };
  const ConversionTest kTests[] = {
      // Known conversions.
      {"#version 110\n", "#version 110\n", "#version 100 es\n"},
      {"#version 100 es\n", "#version 110\n", "#version 100 es\n"},
      {"#version 330\n", "#version 330\n", "#version 300 es\n"},
      {"#version 300 es\n", "#version 330\n", "#version 300 es\n"},

      // Unknown versions: preserve across platforms.
      {"#version 500\n", "#version 500\n", "#version 500 es\n"},
  };
  const size_t kNumTests = sizeof(kTests) / sizeof(kTests[0]);

  std::string result;
  for (size_t i = 0; i < kNumTests; ++i) {
    // Test against both core and es profiles.
    std::string expected = kTests[i].gl_result;
    fplbase::PlatformSanitizeShaderSource(kTests[i].file, nullptr,
                                          fplbase::kShaderProfileCore, &result);
    EXPECT_EQ(result.compare(0, expected.length(), expected), 0);
    EXPECT_EQ(result.find("#version"), result.rfind("#version"));

    expected = kTests[i].gles_result;
    fplbase::PlatformSanitizeShaderSource(kTests[i].file, nullptr,
                                          fplbase::kShaderProfileEs, &result);
    EXPECT_EQ(result.compare(0, expected.length(), expected), 0);
    EXPECT_EQ(result.find("#version"), result.rfind("#version"));
  }
}

TEST_F(PreprocessorTests, SanitizeMultiPartLinesPreserved) {
  const char* file = "#define foo(arg) \\\n    arg\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);
  const size_t pos = result.find("#define foo");
  EXPECT_NE(pos, std::string::npos);
  EXPECT_EQ(result.compare(pos, strlen(file), file), 0);
}

TEST_F(PreprocessorTests, SanitizeCommentsIgnored) {
  const char* single_line_test =
      "#define foo 1\n"
      "// #version 100\n"
      "#define baz 0\n"
      "// #extension GL_FOO_BAZ : enable\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(single_line_test, nullptr, &result);
  EXPECT_LT(result.find("#define foo 1"), result.find("// #version"));

  const char* multi_part_line_test =
      "#define foo 1\n"
      "// multi-part line comment\\\n#version 100\n"
      "#define baz 0\n"
      "// #extension GL_FOO_BAZ : enable\n";
  fplbase::PlatformSanitizeShaderSource(multi_part_line_test, nullptr, &result);
  EXPECT_NE(result.find("\\\n#version"), std::string::npos);
  EXPECT_LT(result.find("#define foo 1"), result.find("\\\n#version"));

  const char* multi_line_test =
      "/* start multi line comment\n"
      "#version 100\n"
      "#extension GL_FOO_BAZ : enable\n"
      "end multi line comment */";
  fplbase::PlatformSanitizeShaderSource(multi_line_test, nullptr, &result);
  EXPECT_NE(result.find("comment\n#version"), std::string::npos);
  EXPECT_LT(result.find("start multi line"), result.find("comment\n#version"));

  const char* combined_test =
      "// this will not start a multi line comment /*\n"
      "#extension GL_FOO_BAZ : enable\n"
      "but /* will, but let's */ end it, just to restart /* now in a comment\n"
      "#version 100\n"
      "end */";
  fplbase::PlatformSanitizeShaderSource(combined_test, nullptr, &result);
  const size_t ext_pos = result.find("#extension GL_FOO_BAZ : enable");
  EXPECT_NE(ext_pos, std::string::npos);

  // If the #version wasn't ignored, it will have been moved before #extension.
  const size_t version_pos = result.find("#version 100");
  EXPECT_NE(version_pos, std::string::npos);
  EXPECT_LT(ext_pos, version_pos);
}

TEST_F(PreprocessorTests, SetShaderVersion) {
  const char* source = "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  const char* expected =
      "#version 200\n"
      "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  std::string result;
  fplbase::SetShaderVersion(source, "200", &result);
  EXPECT_EQ(result, expected);

  source =
      "#version 100\n"
      "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  expected =
      "#version 300 es\n"
      "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  fplbase::SetShaderVersion(source, "300 es", &result);
  EXPECT_EQ(result, expected);

  source =
      "// #version 100\n"
      "/*\n"
      "#version 200\n"
      "*/\n"
      "// Multi-part line\\\n"
      "#version 300\n"
      "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  expected =
      "#version 330\n"
      "// #version 100\n"
      "/*\n"
      "#version 200\n"
      "*/\n"
      "// Multi-part line\\\n"
      "#version 300\n"
      "void main() { gl_FragColor = vec4(1, 1, 1, 1); }\n";
  fplbase::SetShaderVersion(source, "330", &result);
  EXPECT_EQ(result, expected);
}

// Skips the version string if it exists.
std::string SkipVersion(const std::string& str) {
  // Version is expected to be at the beginning of the string.
  if (str.compare(0, 8, "#version") == 0) {
    return str.substr(str.find('\n') + 1);
  }
  return str;
}

TEST_F(PreprocessorTests, SanitizeSimple) {
  #include "simple.inc"
  std::string result;
  fplbase::PlatformSanitizeShaderSource(kSourceText, nullptr, &result);
  EXPECT_EQ(SkipVersion(result), kExpectedText);
}

TEST_F(PreprocessorTests, SanitizeIfExtension) {
  #include "if_extension.inc"
  std::string result;
  fplbase::PlatformSanitizeShaderSource(kSourceText, nullptr, &result);
  EXPECT_EQ(SkipVersion(result), kExpectedText);
}

TEST_F(PreprocessorTests, SanitizeIfExtensionPrecision) {
  #include "if_extension_precision.inc"
  std::string result;
  fplbase::PlatformSanitizeShaderSource(kSourceText, nullptr, &result);
  EXPECT_EQ(SkipVersion(result), kExpectedText);
}

TEST_F(PreprocessorTests, SanitizeExtensionInclude) {
  #include "extension_include.inc"
  std::string result;
  fplbase::PlatformSanitizeShaderSource(kSourceText, nullptr, &result);
  EXPECT_EQ(SkipVersion(result), kExpectedText);
}

extern "C" int FPL_main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
