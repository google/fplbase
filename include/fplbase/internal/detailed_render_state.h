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

#ifndef FPLBASE_DETAILED_RENDER_STATE_H
#define FPLBASE_DETAILED_RENDER_STATE_H

#include "fplbase/viewport.h"

namespace fplbase {

enum RenderFunction {
  kRenderAlways,        // Corresponds to GL_ALWAYS.
  kRenderEqual,         // Corresponds to GL_EQUAL.
  kRenderGreater,       // Corresponds to GL_GREATER.
  kRenderGreaterEqual,  // Corresponds to GL_GEQUAL.
  kRenderLess,          // Corresponds to GL_LESS.
  kRenderLessEqual,     // Corresponds to GL_LEQUAL.
  kRenderNever,         // Corresponds to GL_NEVER.
  kRenderNotEqual,      // Corresponds to GL_NOTEQUAL.
  kRenderCount
};

struct AlphaTestState {
  bool enabled;
  RenderFunction function;
  float ref;

  AlphaTestState() : enabled(false), function(kRenderAlways), ref(0.0f) {}
};

struct BlendState {
  enum BlendFactor {
    kZero,                   // Corresponds to GL_ZERO.
    kOne,                    // Corresponds to GL_ONE.
    kSrcColor,               // Corresponds to GL_SRC_COLOR.
    kOneMinusSrcColor,       // Corresponds to GL_ONE_MINUS_SRC_COLOR.
    kDstColor,               // Corresponds to GL_DST_COLOR.
    kOneMinusDstColor,       // Corresponds to GL_ONE_MINUS_DST_COLOR.
    kSrcAlpha,               // Corresponds to GL_SRC_ALPHA.
    kOneMinusSrcAlpha,       // Corresponds to GL_ONE_MINUS_SRC_ALPHA.
    kDstAlpha,               // Corresponds to GL_DST_ALPHA.
    kOneMinusDstAlpha,       // Corresponds to GL_ONE_MINUS_DST_ALPHA.
    kConstantColor,          // Corresponds to GL_CONSTANT_COLOR.
    kOneMinusConstantColor,  // Corresponds to GL_ONE_MINUS_CONSTANT_COLOR.
    kConstantAlpha,          // Corresponds to GL_CONSTANT_ALPHA.
    kOneMinusConstantAlpha,  // Corresponds to GL_ONE_MINUS_CONSTANT_ALPHA.
    kSrcAlphaSaturate,       // Corresponds to GL_SRC_ALPHA_SATURATE.
    kCount
  };

  bool enabled;
  BlendFactor src_alpha;
  BlendFactor src_color;
  BlendFactor dst_alpha;
  BlendFactor dst_color;

  BlendState()
      : enabled(false),
        src_alpha(kOne),
        src_color(kOne),
        dst_alpha(kZero),
        dst_color(kZero) {}
};

struct CullState {
  enum CullFace { kFront, kBack, kFrontAndBack, kCullFaceCount };
  enum FrontFace { kClockWise, kCounterClockWise, kFrontFaceCount };
  CullFace face;
  FrontFace front;
  bool enabled;

  CullState() : face(kBack), front(kCounterClockWise), enabled(false) {}
};

struct DepthState {
  RenderFunction function;
  bool test_enabled;
  bool write_enabled;

  DepthState()
      : function(kRenderAlways), test_enabled(false), write_enabled(true) {}
};

struct PointState {
  /// If enabled, calculate texture coordinates for points based on texture
  /// environment and point parameter settings. Otherwise texture coordinates
  /// are constant across points.
  bool point_sprite_enabled;
  /// If enabled and a vertex or geometry shader is active, then the derived
  /// point size is taken from the (potentially clipped) shader builtin
  /// gl_PointSize and clamped to the implementation-dependent point size range.
  /// If disabled, then the point size will be derived from
  /// |program_point_size_enabled|.
  bool program_point_size_enabled;
  /// Point size to be used if |program_point_size_enabled| is disabled.
  float point_size;

  PointState()
      : point_sprite_enabled(false),
        program_point_size_enabled(false),
        point_size(1.0f) {}
};

struct StencilFunction {
  RenderFunction function;
  int ref;
  unsigned int mask;

  StencilFunction() : function(kRenderAlways), ref(0), mask(1) {}
};

struct StencilOperation {
  enum StencilOperations {
    kKeep,              // Corresponds to GL_KEEP.
    kZero,              // Corresponds to GL_ZERO.
    kReplace,           // Corresponds to GL_REPLACE.
    kIncrement,         // Corresponds to GL_INCR.
    kIncrementAndWrap,  // Corresponds to GL_INCR_WRAP.
    kDecrement,         // Corresponds to GL_DECR.
    kDecrementAndWrap,  // Corresponds to GL_DECR_WRAP.
    kInvert,            // Corresponds to GL_INVERT.
    kCount
  };

  // Specifies the action to take when the stencil test fails.
  StencilOperations stencil_fail;
  // Specifies the stencil action when the stencil test passes, but the depth
  // test fails.
  StencilOperations depth_fail;
  // Specifies the stencil action when both the stencil test and the depth test
  // pass, or when the stencil test passes and either there is no depth buffer
  // or depth testing is not enabled.
  StencilOperations pass;

  StencilOperation() : stencil_fail(kKeep), depth_fail(kKeep), pass(kKeep) {}
};

struct StencilState {
  bool enabled;

  StencilFunction back_function;
  StencilOperation back_op;
  StencilFunction front_function;
  StencilOperation front_op;

  StencilState() : enabled(false) {}
};

struct ScissorState {
  bool enabled;
  mathfu::recti rect;

  ScissorState() : enabled(false), rect(0, 0, 0, 0) {}
};

struct RenderState {
  AlphaTestState alpha_test_state;
  BlendState blend_state;
  CullState cull_state;
  DepthState depth_state;
  PointState point_state;
  ScissorState scissor_state;
  StencilState stencil_state;

  Viewport viewport;

  RenderState() {}
};

inline bool operator==(const ScissorState &lhs, const ScissorState &rhs) {
  return (lhs.enabled == rhs.enabled) && (lhs.rect == rhs.rect);
}

inline bool operator!=(const ScissorState &lhs, const ScissorState &rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const StencilOperation &lhs,
                       const StencilOperation &rhs) {
  return (lhs.stencil_fail == rhs.stencil_fail &&
          lhs.depth_fail == rhs.depth_fail && lhs.pass == rhs.pass);
}

inline bool operator!=(const StencilOperation &lhs,
                       const StencilOperation &rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const StencilFunction &lhs, const StencilFunction &rhs) {
  return (lhs.function == rhs.function && lhs.ref == rhs.ref &&
          lhs.mask == rhs.mask);
}

inline bool operator!=(const StencilFunction &lhs, const StencilFunction &rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const StencilState &lhs, const StencilState &rhs) {
  return lhs.enabled == rhs.enabled && lhs.back_function == rhs.back_function &&
         lhs.back_op == rhs.back_op &&
         lhs.front_function == rhs.front_function &&
         lhs.front_op == rhs.front_op;
}

inline bool operator!=(const StencilState &lhs, const StencilState &rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const DepthState &lhs, const DepthState &rhs) {
  return lhs.test_enabled == rhs.test_enabled &&
         lhs.write_enabled == rhs.write_enabled && lhs.function == rhs.function;
}

inline bool operator!=(const DepthState &lhs, const DepthState &rhs) {
  return !(lhs == rhs);
}

inline bool operator==(const CullState &lhs, const CullState &rhs) {
  return lhs.enabled == rhs.enabled && lhs.face == rhs.face;
}

inline bool operator!=(const CullState &lhs, const CullState &rhs) {
  return !(lhs == rhs);
}

}  // namespace fplbase

#endif  // FPLBASE_INTERNAL_RENDER_STATE_H
