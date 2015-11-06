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

namespace fpl {

// Initializes the framebuffer needed for Head Mounted Display undistortion.
void InitializeUndistortFramebuffer(int width, int height);
// Called before rendering for HMD to set up the framebuffer.
void BeginUndistortFramebuffer();
// Called when finished with HMD, to undistort and render the result.
void FinishUndistortFramebuffer();

// Called to set whether the Cardboard settings button (gear icon) is enabled
// and rendering.
void SetCardboardButtonEnabled(bool enabled);

/// Dimensions and transforms for viewport when using stereoscopic rendering.
struct HeadMountedDisplayViewSettings {
  /// Extents of each viewport.
  mathfu::vec4i viewport_extents[2];
  /// Transformation matrix for each viewport.
  mathfu::mat4 viewport_transforms[2];
};

// Prepare to render to a Head Mounted Display (HMD).
void HeadMountedDisplayRenderStart(
    const HeadMountedDisplayInput& head_mounted_display_input,
    Renderer* renderer, const mathfu::vec4& clear_color, bool use_undistortion,
    HeadMountedDisplayViewSettings *view_settings);

// Reset viewport settings, finish applying undistortion effect (if enabled)
// and disable blending.
void HeadMountedDisplayRenderEnd(Renderer* renderer, bool use_undistortion);

// Call render_callback between HeadMountedDisplayRenderStart() and
// HeadMountedDisplayRenderEnd() passing
// HeadMountedDisplayViewSettings.viewport_extents and
// HeadMountedDisplayViewSettings.viewport_transforms as arguments to
// render_callback.
template <typename RenderCallback>
void HeadMountedDisplayRender(const InputSystem* input_system,
                              Renderer* renderer, const vec4& clear_color,
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

template <typename RenderCallback>
void HeadMountedDisplayRender(const InputSystem* input_system,
                              Renderer* renderer, const vec4& clear_color,
                              RenderCallback render_callback) {
  HeadMountedDisplayRender(input_system, renderer, clear_color, render_callback,
                           true);
}

}  // namespace fpl

#endif  // FPLBASE_RENDERER_CARDBOARD_H
