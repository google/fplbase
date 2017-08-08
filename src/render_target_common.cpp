// Copyright 2014 Google Inc. All rights reserved.
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

#include "precompiled.h"

#include "fplbase/render_target.h"

namespace fplbase {

void RenderTarget::Initialize(const mathfu::vec2i& dimensions) {
  Initialize(dimensions, kRenderTargetTextureFormatRGBA8,
             kDepthStencilFormatNone);
}

// Generates a render target that represents the screen.
RenderTarget RenderTarget::ScreenRenderTarget(Renderer& renderer) {
  RenderTarget screen_render_target = RenderTarget();
  screen_render_target.framebuffer_id_ = InvalidBufferHandle();
  screen_render_target.rendered_texture_id_ = InvalidTextureHandle();
  screen_render_target.depth_buffer_id_ = InvalidBufferHandle();
  mathfu::vec2i window_size = renderer.environment().GetViewportSize();
  screen_render_target.dimensions_ = window_size;
  screen_render_target.initialized_ = true;
  return screen_render_target;
}

}  // namespace fplbase
