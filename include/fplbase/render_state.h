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

#ifndef FPLBASE_RENDER_STATE_H
#define FPLBASE_RENDER_STATE_H

#include "fplbase/internal/detailed_render_state.h"
#include "fplbase/viewport.h"

namespace fplbase {

/// @brief Specifies the type for the Stencil Mask.
typedef uint32_t StencilMask;

/// @brief Specifies stencil modes for the stencil test.
enum StencilMode {
  kStencilDisabled,     /**< No stencil test. */
  kStencilCompareEqual, /**< Compare the that reference written reference and
                           given reference are equal, if they are, write to the
                           pixel buffer. */
  kStencilWrite,   /**< Always pass the test and write to the pixel and stencil
                      buffers. */
  kStencilUnknown, /**< An unknown mode, usually happens when accessing the
                      graphics API directly.  */
  kStencilModeCount
};

/// @brief Specifies the blending mode used by the blend function.
enum BlendMode {
  kBlendModeOff = 0, /**< No blending is done. The alpha channel is ignored. */
  kBlendModeTest,  /**< Used to provide a function via `glAlphaFunc` instead of
                        `glBlendFunc`.*/
  kBlendModeAlpha, /**< Normal alpha channel blending. */
  kBlendModeAdd, /**< Additive blending, where the resulting color is the sum of
                      the two colors. */
  kBlendModeAddAlpha, /**< Additive blending, where the resulting color is the
                           sum of the two colors, and the image is affected
                           by the alpha. (Note: The background is not affected
                           by the image's alpha.) */
  kBlendModeMultiply, /**< Multiplicative blending, where the resulting color is
                           the product of the image color and the background
                           color. */
  kBlendModePreMultipliedAlpha, /**< Like Alpha, except the source RGB values
                                     are assumed to have already been multiplied
                                     by the alpha, so the blend function doesn't
                                     touch them. */
  kBlendModeUnknown, /**< An unknown mode, usually happens when accessing the
                        graphics API directly.  */
  kBlendModeCount    /** Used at the end of the enum as sentinel value. */
};

/// @brief Specifies the depth function used for rendering.
enum DepthFunction {
  kDepthFunctionDisabled,
  kDepthFunctionNever,
  kDepthFunctionAlways,
  kDepthFunctionLess,
  kDepthFunctionLessEqual,
  kDepthFunctionGreater,
  kDepthFunctionGreaterEqual,
  kDepthFunctionEqual,
  kDepthFunctionNotEqual,
  kDepthFunctionUnknown, /**< An unknown mode, usually happens when accessing
                            the graphics API directly.  */
  kDepthFunctionCount
};

/// @brief Specifies the which face is culled when rendering.
enum CullingMode {
  kCullingModeNone,
  kCullingModeFront,
  kCullingModeBack,
  kCullingModeFrontAndBack,
  kCullingModeUnknown /**< An unknown mode, usually happens when accessing the
                       graphics API directly.  */
};

}  // namespace fplbase

#endif  // FPLBASE_RENDER_STATE_H
