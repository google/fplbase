// Copyright 2015 Google Inc. All rights reserved.
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

#ifndef FPLBASE_RENDERER_HMD_H
#define FPLBASE_RENDERER_HMD_H

#include "fplbase/config.h"  // Must come first.

#include "fplbase/input.h"
#include "fplbase/renderer.h"
#include "mathfu/glsl_mappings.h"

/// @file fplbase/renderer_hmd.h
/// @brief Contains helper functions for interacting with rendering aspects of
///        using a Head Mounted Display device, such as Cardboard.

namespace fplbase {

/// @addtogroup fplbase_renderer
/// @{

/// @brief Initializes the framebuffer needed for Head Mounted Display
///        undistortion.
/// @param width The width of the framebuffer.
/// @param height The height of the framebuffer.
void InitializeUndistortFramebuffer(int width, int height);
/// @brief Called before rendering for HMD to set up the framebuffer.
void BeginUndistortFramebuffer();
/// @brief Called when finished with rendering for HMD, to undistort and render
///        the result.
void FinishUndistortFramebuffer();

/// @brief Called to set whether the Cardboard settings button (gear icon) is
///        enabled and rendering.
/// @param enabled If the settings button should be enabled.
void SetCardboardButtonEnabled(bool enabled);

/// Dimensions and transforms for viewport when using stereoscopic rendering.
struct HeadMountedDisplayViewSettings {
  /// Extents of each viewport.
  mathfu::vec4i viewport_extents[2];
  /// Transformation matrix for each viewport.
  mathfu::mat4 viewport_transforms[2];
};

#if FPLBASE_ANDROID_VR

/// @brief Prepare to render to a Head Mounted Display (HMD).
/// @param head_mounted_display_input The input object managing the HMD state.
/// @param renderer The renderer that is being used to render the scene.
/// @param clear_color The color to clear the framebuffer to before rendering.
/// @param use_undistortion If undistortion should be applied after rendering.
/// @param view_settings The dimensions and transforms for the viewports.
void HeadMountedDisplayRenderStart(
    const HeadMountedDisplayInput& head_mounted_display_input,
    Renderer* renderer, const mathfu::vec4& clear_color, bool use_undistortion,
    HeadMountedDisplayViewSettings* view_settings);

/// @brief Reset viewport settings, finish applying undistortion effect (if
///        enabled) and disable blending.
/// @param renderer The renderer that is being used to render the scene.
/// @param use_undistortion If the undistortion effect should be used.
void HeadMountedDisplayRenderEnd(Renderer* renderer, bool use_undistortion);

/// @brief Helper function that wraps the HMD calls, rendering using the given
///        callback.
///
/// Call render_callback between HeadMountedDisplayRenderStart() and
/// HeadMountedDisplayRenderEnd() passing
/// HeadMountedDisplayViewSettings.viewport_extents and
/// HeadMountedDisplayViewSettings.viewport_transforms as arguments to
/// render_callback.
///
/// @param input_system The input system managing the game's input.
/// @param renderer The renderer that will being used to render the scene.
/// @param clear_color The color to clear the framebuffer to before rendering.
/// @param render_callback The function to call after setting up each viewport.
/// @param use_undistortion If the undistortion effect should be applied.
template <typename RenderCallback>
void HeadMountedDisplayRender(const InputSystem* input_system,
                              Renderer* renderer,
                              const mathfu::vec4& clear_color,
                              RenderCallback render_callback,
                              bool use_undistortion) {
  HeadMountedDisplayViewSettings view_settings;
  HeadMountedDisplayRenderStart(input_system->head_mounted_display_input(),
                                renderer, clear_color, use_undistortion,
                                &view_settings);
  render_callback(view_settings.viewport_extents,
                  view_settings.viewport_transforms);
  HeadMountedDisplayRenderEnd(renderer, use_undistortion);
}

/// @brief Helper function that wraps the HMD calls, rendering using the given
///        callback.
///
/// Call render_callback between HeadMountedDisplayRenderStart() and
/// HeadMountedDisplayRenderEnd() passing
/// HeadMountedDisplayViewSettings.viewport_extents and
/// HeadMountedDisplayViewSettings.viewport_transforms as arguments to
/// render_callback.
///
/// @param input_system The input system managing the game's input.
/// @param renderer The renderer that will being used to render the scene.
/// @param clear_color The color to clear the framebuffer to before rendering.
/// @param render_callback The function to call after setting up each viewport.
template <typename RenderCallback>
void HeadMountedDisplayRender(const InputSystem* input_system,
                              Renderer* renderer,
                              const mathfu::vec4& clear_color,
                              RenderCallback render_callback) {
  HeadMountedDisplayRender(input_system, renderer, clear_color, render_callback,
                           true);
}

#endif  // FPLBASE_ANDROID_VR

/// @}
}  // namespace fplbase

#endif  // FPLBASE_RENDERER_CARDBOARD_H
