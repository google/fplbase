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

#ifndef FPLBASE_TYPE_CONVERSIONS_GL_H
#define FPLBASE_TYPE_CONVERSIONS_GL_H

#include "fplbase/handles.h"
#include "fplbase/internal/detailed_render_state.h"
#include "fplbase/mesh.h"
#include "fplbase/render_target.h"

namespace fplbase {

/// @brief Converts FPL render function value to equivalent GL enum value.
///
/// @param func The render function value to convert.
unsigned int RenderFunctionToGlFunction(RenderFunction func);

/// @brief Converts FPL blend state factor to equivalent GL enum value.
///
/// @param factor The blend state factor to convert.
unsigned int BlendStateFactorToGl(BlendState::BlendFactor factor);

/// @brief Converts FPL stencil operation value to equivalent GL enum value.
///
/// @param op The stencil operation to convert.
unsigned int StencilOpToGlOp(StencilOperation::StencilOperations op);

/// @brief Converts FPL cull face value to equivalent GL enum value.
///
/// @param face The cull face value to convert.
unsigned int CullFaceToGl(CullState::CullFace face);

/// @brief Converts FPL front face value to equivalent GL enum value.
///
/// @param front_face The front face value to convert.
unsigned int FrontFaceToGl(CullState::FrontFace front_face);

/// @brief Converts FPL RenderTargetTextureFormat to equivalent GL internal
/// format enum value.
///
/// @param format The format to convert.
unsigned int RenderTargetTextureFormatToInternalFormatGl(
    RenderTargetTextureFormat format);

/// @brief Converts FPL RenderTargetTextureFormat to equivalent GL format enum
/// value.
///
/// @param format The format to convert.
unsigned int RenderTargetTextureFormatToFormatGl(
    RenderTargetTextureFormat format);

/// @brief Converts FPL RenderTargetTextureFormat to equivalent GL type enum
/// value.
///
/// @param format The format to convert.
unsigned int RenderTargetTextureFormatToTypeGl(
    RenderTargetTextureFormat format);

/// @brief Converts FPL DepthStencilFormat to equivalent GL depth
/// internal format enum value.
///
/// @param format The format to convert.
unsigned int DepthStencilFormatToInternalFormatGl(DepthStencilFormat format);

/// @brief Converts FPL Mesh Primitive to equivalent GL enum value.
///
/// @param primitive The primitive type to convert.
unsigned int GetPrimitiveTypeFlags(Mesh::Primitive primitive);

union HandleUnionGl {
  HandleUnionGl() { handle.handle = 0; }
  explicit HandleUnionGl(internal::OpaqueHandle handle) : handle(handle) {}
  explicit HandleUnionGl(unsigned int gl_param) {
    handle.handle = 0;  // Clear all the memory first.
    gl = gl_param;
  }

  // Opaque handle used in external API.
  internal::OpaqueHandle handle;

  // OpenGL handles, unsigned and signed.
  unsigned int gl;
  int gl_int;
};

// Call from OpenGL code to convert between GL and external API handles.
inline TextureHandle TextureHandleFromGl(unsigned int gl) {
  return HandleUnionGl(gl).handle;
}

inline TextureTarget TextureTargetFromGl(unsigned int gl) {
  return HandleUnionGl(gl).handle;
}

inline ShaderHandle ShaderHandleFromGl(unsigned int gl) {
  return HandleUnionGl(gl).handle;
}

inline UniformHandle UniformHandleFromGl(int gl_int) {
  HandleUnionGl u;
  u.gl_int = gl_int;
  return u.handle;
}

inline BufferHandle BufferHandleFromGl(unsigned int gl) {
  return HandleUnionGl(gl).handle;
}

inline unsigned int GlTextureHandle(TextureHandle handle) {
  return HandleUnionGl(handle).gl;
}

inline unsigned int GlTextureTarget(TextureTarget handle) {
  return HandleUnionGl(handle).gl;
}

inline unsigned int GlShaderHandle(ShaderHandle handle) {
  return HandleUnionGl(handle).gl;
}

inline int GlUniformHandle(UniformHandle handle) {
  HandleUnionGl u;
  u.handle = handle;
  return u.gl_int;
}

inline unsigned int GlBufferHandle(BufferHandle handle) {
  return HandleUnionGl(handle).gl;
}

}  // namespace fplbase

#endif  // FPLBASE_TYPE_CONVERSIONS_GL_H
