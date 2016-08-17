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
#include <unordered_set>
#include "gtest/gtest.h"

class PreprocessorTests : public ::testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  std::string error_message_;
  std::set<std::string> all_includes_;
  std::unordered_set<std::string> all_define_;

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
  bool result =
      fplbase::LoadFileWithDirectives(file, &file_, nullptr, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// An empty list of defines should also be valid.
TEST_F(PreprocessorTests, EmptyDefineList) {
  const char *kEmptyDefines[] = {nullptr};
  const char *file = "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, kEmptyDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// #defines passed-in should be inserted into the file. Try with just one.
TEST_F(PreprocessorTests, OneDefinePassedIn) {
  const char *kOneDefine[] = {"foo", nullptr};
  bool result =
      fplbase::LoadFileWithDirectives("", &file_, kOneDefine, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo\n"));
}

// #defines passed-in should be inserted into the file. Try with multiple.
TEST_F(PreprocessorTests, MultipleDefinesPassedIn) {
  const char *kMultipleDefines[] = {"foo", "foo2", "foo3", nullptr};
  bool result = fplbase::LoadFileWithDirectives("", &file_, kMultipleDefines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo\n#define foo2\n#define foo3\n"));
}

// #define the same identifier twice should be ok
TEST_F(PreprocessorTests, DefineSameIdTwice) {
  const char *file =
      "#define foo\n"
      "#define foo";
  bool result =
      fplbase::LoadFileWithDirectives(file, &file_, nullptr, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

// #defines with a value should be passed through.
TEST_F(PreprocessorTests, ValuePassedIn) {
  const char *kOneDefine[] = {"foo 1", nullptr};
  bool result =
      fplbase::LoadFileWithDirectives("", &file_, kOneDefine, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("#define foo 1\n"));
}

// #defines with a value should be left alone.
TEST_F(PreprocessorTests, ValuePassthrough) {
  const char *file = "#define foo 1";
  bool result =
      fplbase::LoadFileWithDirectives(file, &file_, nullptr, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(file));
}

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
