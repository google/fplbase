#ifndef FPL_RENDER_STATE_H
#define FPL_RENDER_STATE_H

#include "mathfu/glsl_mappings.h"

namespace fplbase {

/// @brief Specifies the blending mode used by the blend function.
enum BlendMode {
  kBlendModeOff = 0, /**< No blending is done. The alpha channel is ignored. */
  kBlendModeTest,  /**< Used to provide a function via `glAlphaFunc` instead of
                        `glBlendFunc`. This is not supported directly, meaning
                        the `glAlphaFunc` must be set manually by the user. This
                        will simply configure the renderer to use that
                        function.*/
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
  kBlendModeCount /** Used at the end of the enum as sentinel value. */
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
  kDepthFunctionCount
};

/// @brief Specifies the which face is culled when rendering.
enum CullingMode {
  kCullingModeNone,
  kCullingModeFront,
  kCullingModeBack,
  kCullingModeFrontAndBack
};

/// @brief Specifies the region of the surface to be used for rendering.
typedef mathfu::recti Viewport;

struct RenderState {
  BlendMode blend_mode;
  CullingMode cull_mode;
  DepthFunction depth_function;
  Viewport viewport;

  RenderState() {
    blend_mode = kBlendModeOff;
    cull_mode = kCullingModeBack;
    depth_function = kDepthFunctionDisabled;
  }
};

}  // namespace fplbase

#endif  // FPL_RENDER_STATE_H
