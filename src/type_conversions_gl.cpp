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

#include "type_conversions_gl.h"

#include "fplbase/fpl_common.h"

namespace fplbase {

GLenum RenderFunctionToGlFunction(RenderFunction func) {
  static const GLenum kRenderFunctionToGlFunctionTable[] = {
      GL_ALWAYS, GL_EQUAL,  GL_GREATER, GL_GEQUAL,
      GL_LESS,   GL_LEQUAL, GL_NEVER,   GL_NOTEQUAL};

  assert(static_cast<size_t>(func) <
         FPL_ARRAYSIZE(kRenderFunctionToGlFunctionTable));
  static_assert(FPL_ARRAYSIZE(kRenderFunctionToGlFunctionTable) == kRenderCount,
                "Need to update the kRenderFunctionToGlFunctionTable array");
  return kRenderFunctionToGlFunctionTable[func];
}

GLenum BlendStateFactorToGl(BlendState::BlendFactor factor) {
  static const GLenum kBlendStateFactorToGlTable[] = {
      GL_ZERO,           GL_ONE,
      GL_SRC_COLOR,      GL_ONE_MINUS_SRC_COLOR,
      GL_DST_COLOR,      GL_ONE_MINUS_DST_COLOR,
      GL_SRC_ALPHA,      GL_ONE_MINUS_SRC_ALPHA,
      GL_DST_ALPHA,      GL_ONE_MINUS_DST_ALPHA,
      GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR,
      GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA};

  static_assert(FPL_ARRAYSIZE(kBlendStateFactorToGlTable) == BlendState::kCount,
                "Need to update the kBlendStateFactorToGlTable array");
  assert(static_cast<size_t>(factor) <
         FPL_ARRAYSIZE(kBlendStateFactorToGlTable));
  return kBlendStateFactorToGlTable[factor];
}

GLenum StencilOpToGlOp(StencilOperation::StencilOperations op) {
  static const GLenum kStencilOpToGlTable[] = {
      GL_KEEP,      GL_ZERO, GL_REPLACE,   GL_INCR,
      GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

  static_assert(FPL_ARRAYSIZE(kStencilOpToGlTable) == StencilOperation::kCount,
                "Need to update the kStencilOpGlTable array");
  assert(static_cast<size_t>(op) < FPL_ARRAYSIZE(kStencilOpToGlTable));
  return kStencilOpToGlTable[op];
}

GLenum CullFaceToGl(CullState::CullFace face) {
  static const GLenum kCullFaceToGlTable[] = {GL_FRONT, GL_BACK,
                                              GL_FRONT_AND_BACK};

  static_assert(FPL_ARRAYSIZE(kCullFaceToGlTable) == CullState::kCount,
                "Need to update the kCullFaceToGlTable array");
  assert(static_cast<size_t>(face) < FPL_ARRAYSIZE(kCullFaceToGlTable));
  return kCullFaceToGlTable[face];
}

}  // namespace fplbase
