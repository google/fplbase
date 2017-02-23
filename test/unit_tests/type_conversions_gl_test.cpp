// Copyright 2017 Google Inc. All rights reserved.
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

#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/glplatform.h"
#include "gtest/gtest.h"

using namespace fplbase;

class TypeConversationsGlTests : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(TypeConversationsGlTests, RenderFunctionToGlFunction) {
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderAlways), GL_ALWAYS);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderEqual), GL_EQUAL);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderGreater), GL_GREATER);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderGreaterEqual), GL_GEQUAL);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderLess), GL_LESS);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderLessEqual), GL_LEQUAL);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderNever), GL_NEVER);
  EXPECT_EQ(RenderFunctionToGlFunction(kRenderNotEqual), GL_NOTEQUAL);
}

TEST_F(TypeConversationsGlTests, BlendStateFactorToGl) {
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kZero), GL_ZERO);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOne), GL_ONE);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kSrcColor), GL_SRC_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusSrcColor),
            GL_ONE_MINUS_SRC_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kDstColor), GL_DST_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusDstColor),
            GL_ONE_MINUS_DST_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kSrcAlpha), GL_SRC_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusSrcAlpha),
            GL_ONE_MINUS_SRC_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kDstAlpha), GL_DST_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusDstAlpha),
            GL_ONE_MINUS_DST_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kConstantColor),
            GL_CONSTANT_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusConstantColor),
            GL_ONE_MINUS_CONSTANT_COLOR);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kConstantAlpha),
            GL_CONSTANT_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kOneMinusConstantAlpha),
            GL_ONE_MINUS_CONSTANT_ALPHA);
  EXPECT_EQ(BlendStateFactorToGl(BlendState::kSrcAlphaSaturate),
            GL_SRC_ALPHA_SATURATE);
}

TEST_F(TypeConversationsGlTests, StencilOpToGlOp) {
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kKeep), GL_KEEP);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kZero), GL_ZERO);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kReplace), GL_REPLACE);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kIncrement), GL_INCR);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kIncrementAndWrap), GL_INCR_WRAP);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kDecrement), GL_DECR);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kDecrementAndWrap), GL_DECR_WRAP);
  EXPECT_EQ(StencilOpToGlOp(StencilOperation::kInvert), GL_INVERT);
}

TEST_F(TypeConversationsGlTests, CullFaceToGl) {
  EXPECT_EQ(CullFaceToGl(CullState::kFront), GL_FRONT);
  EXPECT_EQ(CullFaceToGl(CullState::kBack), GL_BACK);
  EXPECT_EQ(CullFaceToGl(CullState::kFrontAndBack), GL_FRONT_AND_BACK);
}

extern "C" int FPL_main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
