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

#include "fplbase/fpl_common.h"
#include "fplbase/glplatform.h"

namespace fplbase {

unsigned int RenderFunctionToGlFunction(RenderFunction func) {
  static const GLenum kRenderFunctionToGlFunctionTable[] = {
      GL_ALWAYS, GL_EQUAL,  GL_GREATER, GL_GEQUAL,
      GL_LESS,   GL_LEQUAL, GL_NEVER,   GL_NOTEQUAL};

  assert(static_cast<size_t>(func) <
         FPL_ARRAYSIZE(kRenderFunctionToGlFunctionTable));
  static_assert(FPL_ARRAYSIZE(kRenderFunctionToGlFunctionTable) == kRenderCount,
                "Need to update the kRenderFunctionToGlFunctionTable array");
  return kRenderFunctionToGlFunctionTable[func];
}

unsigned int BlendStateFactorToGl(BlendState::BlendFactor factor) {
  static const GLenum kBlendStateFactorToGlTable[] = {
      GL_ZERO,
      GL_ONE,
      GL_SRC_COLOR,
      GL_ONE_MINUS_SRC_COLOR,
      GL_DST_COLOR,
      GL_ONE_MINUS_DST_COLOR,
      GL_SRC_ALPHA,
      GL_ONE_MINUS_SRC_ALPHA,
      GL_DST_ALPHA,
      GL_ONE_MINUS_DST_ALPHA,
      GL_CONSTANT_COLOR,
      GL_ONE_MINUS_CONSTANT_COLOR,
      GL_CONSTANT_ALPHA,
      GL_ONE_MINUS_CONSTANT_ALPHA,
      GL_SRC_ALPHA_SATURATE};

  static_assert(FPL_ARRAYSIZE(kBlendStateFactorToGlTable) == BlendState::kCount,
                "Need to update the kBlendStateFactorToGlTable array");
  assert(static_cast<size_t>(factor) <
         FPL_ARRAYSIZE(kBlendStateFactorToGlTable));
  return kBlendStateFactorToGlTable[factor];
}

unsigned int StencilOpToGlOp(StencilOperation::StencilOperations op) {
  static const GLenum kStencilOpToGlTable[] = {
      GL_KEEP,      GL_ZERO, GL_REPLACE,   GL_INCR,
      GL_INCR_WRAP, GL_DECR, GL_DECR_WRAP, GL_INVERT};

  static_assert(FPL_ARRAYSIZE(kStencilOpToGlTable) == StencilOperation::kCount,
                "Need to update the kStencilOpGlTable array");
  assert(static_cast<size_t>(op) < FPL_ARRAYSIZE(kStencilOpToGlTable));
  return kStencilOpToGlTable[op];
}

unsigned int CullFaceToGl(CullState::CullFace face) {
  static const GLenum kCullFaceToGlTable[] = {GL_FRONT, GL_BACK,
                                              GL_FRONT_AND_BACK};

  static_assert(FPL_ARRAYSIZE(kCullFaceToGlTable) == CullState::kCullFaceCount,
                "Need to update the kCullFaceToGlTable array");
  assert(static_cast<size_t>(face) < FPL_ARRAYSIZE(kCullFaceToGlTable));
  return kCullFaceToGlTable[face];
}

unsigned int FrontFaceToGl(CullState::FrontFace front_face) {
  static const GLenum kFrontFaceToGlTable[] = {
      GL_CW,  // kClockWise,
      GL_CCW  // kCounterClockWise,
  };
  static_assert(
      FPL_ARRAYSIZE(kFrontFaceToGlTable) == CullState::kFrontFaceCount,
      "Need to update the kFrontFaceToGlTable array");
  assert(static_cast<size_t>(front_face) < FPL_ARRAYSIZE(kFrontFaceToGlTable));
  return kFrontFaceToGlTable[front_face];
}

unsigned int RenderTargetTextureFormatToInternalFormatGl(
    RenderTargetTextureFormat format) {
  static const GLenum kRenderTargetTextureFormatToTypeGlTable[] = {
      GL_ALPHA,  // kRenderTargetTextureFormatA8,
      GL_RGB,    // kRenderTargetTextureFormatR8,
      GL_RGB,    // kRenderTargetTextureFormatRGB8,
      GL_RGBA,   // kRenderTargetTextureFormatRGBA8,

      GL_DEPTH_COMPONENT16,  // kRenderTargetTextureFormatDepth16,
      GL_DEPTH_COMPONENT32F  // kRenderTargetTextureFormatDepth32F,
  };

  static_assert(FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable) ==
                    kRenderTargetTextureFormatCount,
                "Update kRenderTargetTextureFormatToTypeGlTable to match "
                "enum");
  assert(0 <= format &&
         format < FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable));
  return kRenderTargetTextureFormatToTypeGlTable[format];
}

unsigned int RenderTargetTextureFormatToFormatGl(
    RenderTargetTextureFormat format) {
  static const GLenum kRenderTargetTextureFormatToTypeGlTable[] = {
      GL_ALPHA,  // kRenderTargetTextureFormatA8,
#ifdef FPLBASE_GLES
      // For GLES2, the format must match internalformat.
      GL_RGB,  // kRenderTargetTextureFormatR8,
#else
      GL_RED,  // kRenderTargetTextureFormatR8,
#endif
      GL_RGB,   // kRenderTargetTextureFormatRGB8,
      GL_RGBA,  // kRenderTargetTextureFormatRGBA8,

      GL_DEPTH_COMPONENT,  // kRenderTargetTextureFormatDepth16,
      GL_DEPTH_COMPONENT   // kRenderTargetTextureFormatDepth32F,
  };

  static_assert(FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable) ==
                    kRenderTargetTextureFormatCount,
                "Update kRenderTargetTextureFormatToTypeGlTable to match "
                "enum");
  assert(0 <= format &&
         format < FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable));
  return kRenderTargetTextureFormatToTypeGlTable[format];
}

unsigned int RenderTargetTextureFormatToTypeGl(
    RenderTargetTextureFormat format) {
  static const GLenum kRenderTargetTextureFormatToTypeGlTable[] = {
      GL_UNSIGNED_BYTE,  // kRenderTargetTextureFormatA8,
      GL_UNSIGNED_BYTE,  // kRenderTargetTextureFormatR8,
      GL_UNSIGNED_BYTE,  // kRenderTargetTextureFormatRGB8,
      GL_UNSIGNED_BYTE,  // kRenderTargetTextureFormatRGBA8,

      GL_UNSIGNED_SHORT,  // kRenderTargetTextureFormatDepth16,
      GL_FLOAT            // kRenderTargetTextureFormatDepth32F,
  };

  static_assert(FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable) ==
                    kRenderTargetTextureFormatCount,
                "Update kRenderTargetTextureFormatToTypeGlTable to match "
                "enum");
  assert(0 <= format &&
         format < FPL_ARRAYSIZE(kRenderTargetTextureFormatToTypeGlTable));
  return kRenderTargetTextureFormatToTypeGlTable[format];
}

unsigned int DepthStencilFormatToInternalFormatGl(DepthStencilFormat format) {
  static const GLenum kDepthStencilFormatToInternalFormatGlTable[] = {
      GL_DEPTH_COMPONENT16,   // kDepthStencilFormatDepth16,
      GL_DEPTH_COMPONENT24,   // kDepthStencilFormatDepth24,
      GL_DEPTH_COMPONENT32F,  // kDepthStencilFormatDepth32F,
      GL_DEPTH24_STENCIL8,    // kDepthStencilFormatDepth24Stencil8,
      GL_DEPTH32F_STENCIL8,   // kDepthStencilFormatDepth32FStencil8,
      GL_STENCIL_INDEX8       // kDepthStencilFormatStencil8,
  };

  static_assert(FPL_ARRAYSIZE(kDepthStencilFormatToInternalFormatGlTable) ==
                    kDepthStencilFormatCount,
                "Update kDepthStencilFormatToInternalFormatGlTable to match "
                "enum");
  assert(0 <= format &&
         format < FPL_ARRAYSIZE(kDepthStencilFormatToInternalFormatGlTable));
  return kDepthStencilFormatToInternalFormatGlTable[format];
}

uint32_t GetPrimitiveTypeFlags(Mesh::Primitive primitive) {
  switch (primitive) {
    case Mesh::kLines:
      return GL_LINES;
    case Mesh::kPoints:
      return GL_POINTS;
    case Mesh::kTriangleStrip:
      return GL_TRIANGLE_STRIP;
    case Mesh::kTriangleFan:
      return GL_TRIANGLE_FAN;
    default:
      return GL_TRIANGLES;
  }
}

}  // namespace fplbase
