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
#include <string>
#include "gtest/gtest.h"

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
bool LoadFile(const char *file, std::string *dest) {
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
  const char *file = "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// An empty list of defines should also be valid.
TEST_F(PreprocessorTests, EmptyDefineList) {
  const char *file = "#define foo";
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
  const char *file =
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
  const char *file = "#define foo 1";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

TEST_F(PreprocessorTests, SanitizeCheckPrefix) {
  const char* file = "";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);

#ifdef FPLBASE_GLES
  EXPECT_EQ(result, "#ifdef GL_ES\nprecision highp float;\n#endif\n");
#else
  EXPECT_EQ(result,
            "#version 120\n#define lowp\n#define mediump\n#define highp\n");
#endif
}

TEST_F(PreprocessorTests, SanitizeVersionIsFirstLine) {
  const char* file = "#define foo 1\n#version 100\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);
  EXPECT_TRUE(result.compare(0, 12, "#version 100"));
}

TEST_F(PreprocessorTests, SanitizeVersionConversion) {
  struct ConversionTest {
    const char* file;
    const char* gl_result;
    const char* gles_result;
  };
  const ConversionTest kTests[] = {
    // Known conversions.
    { "#version 110\n",    "#version 110\n", "#version 100 es\n" },
    { "#version 100 es\n", "#version 110\n", "#version 100 es\n" },
    { "#version 330\n",    "#version 330\n", "#version 300 es\n" },
    { "#version 300 es\n", "#version 330\n", "#version 300 es\n" },

    // Unknown versions: preserve across platforms.
    { "#version 500\n",    "#version 500\n", "#version 500 es\n" },
  };
  const size_t kNumTests = sizeof(kTests) / sizeof(kTests[0]);

  std::string result;
  for (size_t i = 0; i < kNumTests; ++i) {
    fplbase::PlatformSanitizeShaderSource(kTests[i].file, nullptr, &result);

#ifdef FPLBASE_GLES
    const std::string& expected = kTests[i].gles_result;
#else
    const std::string& expected = kTests[i].gl_result;
#endif

    EXPECT_EQ(result.compare(0, expected.length(), expected), 0);
  }
}

TEST_F(PreprocessorTests, SanitizeExtensionsMoved) {
  const char* file =
      "#define foo 1\n"
      "#extension GL_OES_standard_derivatives : enable\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);
  const size_t define_pos = result.find("#define");
  const size_t extension_pos = result.find("#extension");
  EXPECT_LT(extension_pos, define_pos);
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
  const char*file =
      "#define foo 1\n"
      "// #version 100\n"
      "#define baz 0\n"
      "// #extension GL_FOO_BAZ : enable\n";
  std::string result;
  fplbase::PlatformSanitizeShaderSource(file, nullptr, &result);
  const size_t pos = result.find("#define foo");
  EXPECT_NE(pos, std::string::npos);
  EXPECT_NE(pos, 0u);
  EXPECT_EQ(result.compare(pos, strlen(file), file), 0);
}

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
