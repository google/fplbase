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

// Call used for rendering to HMDs, handling the split and multiple matrices.
// Calls back on the provided render_callback twice, with the transforms from
// the device representing each eye, which are gotten from the input system.
// As an example:
// HeadMountedDisplayRender(&input_system, &renderer,
//                          vec4(0, 0, 0, 1),  // Clear screen to black
//                          [](const mat4& transform) {
//                            RenderSceneFromView(transform);
//                          });
template <typename RenderCallback>
void HeadMountedDisplayRender(InputSystem* input_system, Renderer* renderer,
                              const vec4& clear_color,
                              RenderCallback render_callback) {
  BeginUndistortFramebuffer();
  renderer->ClearFrameBuffer(clear_color);
  renderer->color() = mathfu::kOnes4f;
  renderer->DepthTest(true);

  vec2i size = AndroidGetScalerResolution();
  const vec2i viewport_size =
      size.x() && size.y() ? size : renderer->window_size();
  float window_width = viewport_size.x();
  float half_width = window_width / 2.0f;
  float window_height = viewport_size.y();

  // Render for the left side
  GL_CALL(glViewport(0, 0, half_width, window_height));
  render_callback(input_system->cardboard_input().left_eye_transform());
  // Render for the right side
  GL_CALL(glViewport(half_width, 0, half_width, window_height));
  render_callback(input_system->cardboard_input().right_eye_transform());
  // Reset the screen, and finish
  GL_CALL(glViewport(0, 0, window_width, window_height));
  FinishUndistortFramebuffer();
}

}  // namespace fpl

#endif  // FPLBASE_RENDERER_CARDBOARD_H
