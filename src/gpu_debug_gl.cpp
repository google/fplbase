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

#include "fplbase/gpu_debug.h"

#include "fplbase/glplatform.h"
#include "fplbase/internal/type_conversions_gl.h"

namespace fplbase {

inline bool GlToBool(GLboolean bool_value) { return bool_value == GL_TRUE; }

bool ValidateGlBlendState(const BlendState& state) {
  GLboolean bool_value;
  GLint int_value;

  GL_CALL(glGetBooleanv(GL_BLEND, &bool_value));
  if (GlToBool(bool_value) != state.enabled) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_BLEND_SRC_RGB, &int_value));
  if (static_cast<GLint>(BlendStateFactorToGl(state.src_color)) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_BLEND_SRC_ALPHA, &int_value));
  if (static_cast<GLint>(BlendStateFactorToGl(state.src_alpha)) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_BLEND_DST_RGB, &int_value));
  if (static_cast<GLint>(BlendStateFactorToGl(state.dst_color)) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_BLEND_DST_ALPHA, &int_value));
  if (static_cast<GLint>(BlendStateFactorToGl(state.dst_alpha)) != int_value) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateGlCullState(const CullState& state) {
  GLboolean bool_value;
  GLint int_value;

  GL_CALL(glGetBooleanv(GL_CULL_FACE, &bool_value));
  if (GlToBool(bool_value) != state.enabled) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_CULL_FACE_MODE, &int_value));
  if (static_cast<GLint>(CullFaceToGl(state.face)) != int_value) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateGlDepthState(const DepthState& state) {
  GLboolean bool_value;
  GLint int_value;

  GL_CALL(glGetBooleanv(GL_DEPTH_TEST, &bool_value));
  if (GlToBool(bool_value) != state.test_enabled) {
    assert(false);
    return false;
  }

  GL_CALL(glGetBooleanv(GL_DEPTH_WRITEMASK, &bool_value));
  if (GlToBool(bool_value) != state.write_enabled) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_DEPTH_FUNC, &int_value));
  if (static_cast<GLint>(RenderFunctionToGlFunction(state.function)) !=
      int_value) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateGlPointState(const PointState& state) {
  GLboolean bool_value;

#ifndef FPLBASE_GLES
#ifdef GL_POINT_SPRITE
  GL_CALL(glGetBooleanv(GL_POINT_SPRITE, &bool_value));
  if (GlToBool(bool_value) != state.point_sprite_enabled) {
    assert(false);
    return false;
  }
#endif  // GL_POINT_SPRITE

#ifdef GL_PROGRAM_POINT_SIZE
  GL_CALL(glGetBooleanv(GL_PROGRAM_POINT_SIZE, &bool_value));
  if (GlToBool(bool_value) != state.point_sprite_enabled) {
    assert(false);
    return false;
  }
#elif defined(GL_VERTEX_PROGRAM_POINT_SIZE)
  GL_CALL(glGetBooleanv(GL_VERTEX_PROGRAM_POINT_SIZE, &bool_value));
  if (GlToBool(bool_value) != state.point_sprite_enabled) {
    assert(false);
    return false;
  }
#endif  // GL_PROGRAM_POINT_SIZE

  float float_value;
  GL_CALL(glGetFloatv(GL_POINT_SIZE, &float_value));
  if (float_value != state.point_size) {
    assert(false);
    return false;
  }
#endif  // FPLBASE_GLES

  // "Use" the bool value in case none of the conditions were hit, to avoid
  // compilation errors.
  (void)bool_value;

  return true;
}

bool ValidateGlStencilState(const StencilState& state) {
  GLboolean bool_value;
  GLint int_value;

  GL_CALL(glGetBooleanv(GL_STENCIL_TEST, &bool_value));
  if (GlToBool(bool_value) != state.enabled) {
    assert(false);
    return false;
  }

  // Back Stencil Function values.
  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_FUNC, &int_value));
  if (static_cast<GLint>(RenderFunctionToGlFunction(
          state.back_function.function)) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_REF, &int_value));
  if (static_cast<GLint>(state.back_function.ref) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &int_value));
  if (static_cast<GLint>(state.back_function.mask) != int_value) {
    assert(false);
    return false;
  }

  // Front Stencil Function values.
  GL_CALL(glGetIntegerv(GL_STENCIL_FUNC, &int_value));
  if (static_cast<GLint>(RenderFunctionToGlFunction(
          state.front_function.function)) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_REF, &int_value));
  if (static_cast<GLint>(state.front_function.ref) != int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_VALUE_MASK, &int_value));
  if (static_cast<GLint>(state.front_function.mask) != int_value) {
    assert(false);
    return false;
  }

  // Back Stencil Operations.
  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_FAIL, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.back_op.stencil_fail)) !=
      int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.back_op.depth_fail)) !=
      int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.back_op.pass)) != int_value) {
    assert(false);
    return false;
  }

  // Front Stencil Operations.
  GL_CALL(glGetIntegerv(GL_STENCIL_FAIL, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.front_op.stencil_fail)) !=
      int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.front_op.depth_fail)) !=
      int_value) {
    assert(false);
    return false;
  }

  GL_CALL(glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &int_value));
  if (static_cast<GLint>(StencilOpToGlOp(state.front_op.pass)) != int_value) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateGlScissorState(const ScissorState& state) {
  GLboolean bool_value;

  GL_CALL(glGetBooleanv(GL_SCISSOR_TEST, &bool_value));
  if (GlToBool(bool_value) != state.enabled) {
    assert(false);
    return false;
  }

  GLint int_values[4];

  GL_CALL(glGetIntegerv(GL_SCISSOR_BOX, int_values));
  if (int_values[0] != state.rect.pos.x || int_values[1] != state.rect.pos.y ||
      int_values[2] != state.rect.size.x ||
      int_values[3] != state.rect.size.y) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateGlViewport(const Viewport& viewport) {
  GLint int_values[4];

  GL_CALL(glGetIntegerv(GL_VIEWPORT, int_values));
  if (int_values[0] != viewport.pos.x || int_values[1] != viewport.pos.y ||
      int_values[2] != viewport.size.x || int_values[3] != viewport.size.y) {
    assert(false);
    return false;
  }

  return true;
}

bool ValidateRenderState(const RenderState& render_state) {
  if (!ValidateGlBlendState(render_state.blend_state)) {
    return false;
  }

  if (!ValidateGlCullState(render_state.cull_state)) {
    return false;
  }

  if (!ValidateGlScissorState(render_state.scissor_state)) {
    return false;
  }

  if (!ValidateGlDepthState(render_state.depth_state)) {
    return false;
  }

  if (!ValidateGlPointState(render_state.point_state)) {
    return false;
  }

  if (!ValidateGlStencilState(render_state.stencil_state)) {
    return false;
  }

  if (!ValidateGlViewport(render_state.viewport)) {
    return false;
  }

  return true;
}

}  // namespace fplbase
