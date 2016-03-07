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

#include "gtest/gtest.h"
#include "fplbase/preprocessor.h"
// #include "fplbase/utilities.h"
#include <string>
#include <unordered_set>

using fplbase::LoadFileWithDirectives;

class PreprocessorTests : public ::testing::Test {
protected:
  virtual void SetUp();
  virtual void TearDown();

  std::string error_message_;
  std::set<std::string> all_includes_;
  std::unordered_set<std::string> all_define_;

  std::string file_;
};

static const std::string kDefineMissingIdError =
    "#define must be followed by an identifier.";
static const std::string kDefineExtraArgsError =
    "#define can only support a single identifier.";
static const std::string kMissingEndIfError = "All #if (#ifdef, #ifndef) "
                                              "statements must have a "
                                              "corresponding #endif statement.";
static const std::string kIfStackEmptyRegex =
    "[Assertion failed: (!if_stack.empty())].*";
static const std::string kUnkownDirectiveError = "Unknown directive: #unknown";

static const char *empty_defines[] = {nullptr};

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

// #define without a definition should be fine as long as there is an
// identifier
TEST_F(PreprocessorTests, SimpleDefineTest) {
  const char *file = "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string(""));
}

// #define as a standalone directive should fail
TEST_F(PreprocessorTests, DefineWithoutIdentifier) {
  const char *file = "#define";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_FALSE(result);
  EXPECT_EQ(error_message_, kDefineMissingIdError);
}

// #define should not be able to handle more than one argument
TEST_F(PreprocessorTests, DefineWithIdAndOneDefinition) {
  const char *file = "#define foo bar";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_FALSE(result);
  EXPECT_EQ(error_message_, kDefineExtraArgsError);
}

// #define the same identifier twice should be ok
TEST_F(PreprocessorTests, DefineSameIdTwice) {
  const char *file = "#define foo\n"
                     "#define foo";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(error_message_, std::string(""));
  EXPECT_EQ(file_, std::string(""));
}

// #ifdef should allow compilation if the identifier is defined.
TEST_F(PreprocessorTests, SimpleIfDefTest) {
  const char *file = "#define foo\n"
                     "#ifdef foo\n"
                     "foo is defined.\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, std::string("foo is defined.\n"));
}

// #ifdef should skip compilation when the identifier is not defined.
TEST_F(PreprocessorTests, IfDefNotDefined) {
  const char *file = "#ifdef bar\n"
                     "bar is defined.\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file, &file_, empty_defines,
                                                &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "");
}

// #ifdef should skip nested statements that evaluate to be false.
TEST_F(PreprocessorTests, IfDefNestedTrueFalse) {
  std::string file = "#define foo\n"
                     "#ifdef foo\n"
                     "foo is defined.\n"
                     "#ifdef bar\n"
                     "bar is defined.\n"
                     "#endif\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is defined.\n");
}

// #ifdef should handle nested statmeents that are both true.
TEST_F(PreprocessorTests, IfDefNestedBothTrue) {
  std::string file = "#define foo\n"
                     "#define bar\n"
                     "#ifdef foo\n"
                     "foo is defined.\n"
                     "#ifdef bar\n"
                     "bar is defined.\n"
                     "#endif\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is defined.\nbar is defined.\n");
}

// #ifdef should skip everything (including nested statements) if the top-level
// statement is false.
TEST_F(PreprocessorTests, IfDefNestedFalseTrue) {
  std::string file = "#define bar\n"
                     "#ifdef foo\n"
                     "foo is defined.\n"
                     "#ifdef bar\n"
                     "bar is defined.\n"
                     "#endif\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "");
}

// #ifndef should compile if the symbol is not defined.
TEST_F(PreprocessorTests, SimpleIfNDefTest) {
  std::string file = "#ifndef foo\nfoo is not defined.\n#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is not defined.\n");
}

// #ifndef should not compile if the symbol is defined.
TEST_F(PreprocessorTests, IfNDefIsDefined) {
  std::string file = "#define foo\n#ifndef foo\nfoo is not defined.\n#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "");
}

// #else should compile if the #ifdef evaluates to false.
TEST_F(PreprocessorTests, SimpleElseTest) {
  std::string file =
      "#ifdef foo\nfoo is defined.\n#else\nfoo is not defined.\n#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is not defined.\n");
}

// #else should not compile if the #ifdef evaluates to true.
TEST_F(PreprocessorTests, ElseIgnored) {
  std::string file = "#define foo\n"
                     "#ifdef foo\n"
                     "foo is defined.\n"
                     "#else\n"
                     "foo is not defined.\n"
                     "#endif";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is defined.\n");
}

// Nested #else should not compile if the top-level #if is false.
TEST_F(PreprocessorTests, NestedElse) {
  std::string file = "#ifdef foo\n"
                     "foo is defined.\n"
                     "#ifdef bar\n"
                     "bar is defined.\n"
                     "#else\n"
                     "bar is not defined.\n"
                     "#endif\n"
                     "#endif\n";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "");
}

// Should fail if there aren't enough #endif
TEST_F(PreprocessorTests, TooFewEndIf) {
  std::string file = "#ifdef foo\n"
                     "foo is defined.\n";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_FALSE(result);
  EXPECT_EQ(error_message_, kMissingEndIfError);
}

// Should fail if there are too many #endif
TEST_F(PreprocessorTests, TooManyEndif) {
  std::string file = "#ifdef foo\n"
                     "foo is defined.\n"
                     "#endif\n"
                     "#endif";
  EXPECT_DEATH(fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                               empty_defines, &error_message_),
               kIfStackEmptyRegex);
}

// Unknown directives should fail.
TEST_F(PreprocessorTests, UnknownDirectiveTest) {
  std::string file = "#unknown";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                empty_defines, &error_message_);
  EXPECT_FALSE(result);
  EXPECT_EQ(error_message_, kUnkownDirectiveError);
}

// Passing in #define variables manually should make #ifdef true.
TEST_F(PreprocessorTests, ManualDefineTest) {
  static const char *my_defines[] = {"foo", nullptr};
  std::string file = "#ifdef foo\n"
                     "foo is defined.\n"
                     "#endif\n";
  bool result = fplbase::LoadFileWithDirectives(file.c_str(), &file_,
                                                my_defines, &error_message_);
  EXPECT_TRUE(result);
  EXPECT_EQ(file_, "foo is defined.\n");
}

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
