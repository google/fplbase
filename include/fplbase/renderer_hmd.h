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

// Call used for rendering to HMDs, handling the split and multiple matrices.
// Calls back on the provided render_callback twice, with the transforms from
// the device representing each eye, which are gotten from the input system.
// As an example:
// HeadMountedDisplayRender(&input_system, &renderer,
//                          vec4(0, 0, 0, 1),  // Clear screen to black
//                          [](const vec4i* viewport, const mat4* transform) {
//                            RenderSceneFromView(transform);
//                          });
template <typename RenderCallback>
void HeadMountedDisplayRender(InputSystem* input_system, Renderer* renderer,
                              const vec4& clear_color,
                              RenderCallback render_callback,
                              bool use_undistortion) {
  if (use_undistortion) {
    BeginUndistortFramebuffer();
  }
  renderer->ClearFrameBuffer(clear_color);
  renderer->set_color(mathfu::kOnes4f);
  renderer->DepthTest(true);

  vec2i size = AndroidGetScalerResolution();
  const vec2i viewport_size =
      size.x() && size.y() ? size : renderer->window_size();
  float window_width = viewport_size.x();
  float half_width = window_width / 2.0f;
  float window_height = viewport_size.y();

  // Render stereoscopic views.
  vec4i viewport[2] = {vec4i(0, 0, static_cast<int32_t>(half_width),
                             static_cast<int32_t>(window_height)),
                       vec4i(static_cast<int32_t>(half_width), 0,
                             static_cast<int32_t>(half_width),
                             static_cast<int32_t>(window_height))};
  mat4 transforms[2] = {
      input_system->cardboard_input().left_eye_transform(),
      input_system->cardboard_input().right_eye_transform(), };
  render_callback(viewport, transforms);

  // Reset the screen, and finish
  GL_CALL(glViewport(0, 0, window_width, window_height));
  if (use_undistortion) {
    FinishUndistortFramebuffer();
    renderer->SetBlendMode(kBlendModeOff);
  }
}

template <typename RenderCallback>
void HeadMountedDisplayRender(InputSystem* input_system, Renderer* renderer,
                              const vec4& clear_color,
                              RenderCallback render_callback) {
  HeadMountedDisplayRender(input_system, renderer, clear_color, render_callback,
                           true);
}

}  // namespace fpl

#endif  // FPLBASE_RENDERER_CARDBOARD_H
