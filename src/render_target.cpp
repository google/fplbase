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
#include "fplbase/utilities.h"

#include "webp/decode.h"

using mathfu::vec2i;

namespace fplbase {

void RenderTarget::Initialize(mathfu::vec2i dimensions) {
  Initialize(dimensions, GL_UNSIGNED_BYTE, true);
}

void RenderTarget::Initialize(mathfu::vec2i dimensions, GLenum format,
                              bool create_depth_buffer) {
  assert(!initialized());
  dimensions_ = dimensions;
  // Set up the framebuffer itself:
  framebuffer_id_ = 0;
  depth_buffer_id_ = 0;

  // Our framebuffer object:
  GL_CALL(glGenFramebuffers(1, &framebuffer_id_));

  // The texture we're going to render to
  GL_CALL(glGenTextures(1, &rendered_texture_id_));

  // Bind the framebuffer:
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_));

  // Set up the texture:
  GL_CALL(glBindTexture(GL_TEXTURE_2D, rendered_texture_id_));

  // Give an empty image to OpenGL.  (It will allocate memory, but not bother
  // to populate it.  Which is fine, since we're going to render into it.)
  GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dimensions.x(),
                       dimensions.y(), 0, GL_RGBA, format, nullptr));

  // Define texture properties:
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));

  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

  // Attach our texture as the color attachment.
  GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, rendered_texture_id_, 0));

  if (create_depth_buffer) {
    // A renderbuffer, that we'll use for depth:
    GL_CALL(glGenRenderbuffers(1, &depth_buffer_id_));

    // Bind renderbuffer and set it as the depth buffer:
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_id_));
    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                  dimensions_.x(), dimensions_.y()));

    // Attach renderbuffer as our depth attachment.
    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, depth_buffer_id_));
  }

  // Make sure everything worked:
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    assert(false);
  }
  assert(framebuffer_id_ != 0);
  assert(depth_buffer_id_ != 0);

  // Be good citizens and clean up:
  // Bind the framebuffer:
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

  initialized_ = true;
}

void RenderTarget::Delete() {
  if (initialized_) {
    GL_CALL(glDeleteFramebuffers(1, &framebuffer_id_));
    GL_CALL(glDeleteRenderbuffers(1, &depth_buffer_id_));
    GL_CALL(glDeleteTextures(1, &rendered_texture_id_));
    initialized_ = false;
  }
}

// Set up all the rendering state so that the output is the texture in
// this rendertarget.
void RenderTarget::SetAsRenderTarget() const {
  // Calling SetAsRenderTarget on uninitialized rendertargets is bad.
  assert(initialized_);
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id_));
  GL_CALL(glViewport(0, 0, dimensions_.x(), dimensions_.y()));
}

void RenderTarget::BindAsTexture(int texture_number) const {
  assert(initialized_);
  GL_CALL(glActiveTexture(GL_TEXTURE0 + texture_number));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, rendered_texture_id_));
}

// Generates a render target that represents the screen.
RenderTarget RenderTarget::ScreenRenderTarget(Renderer& renderer) {
  RenderTarget screen_render_target = RenderTarget();
  screen_render_target.framebuffer_id_ = 0;
  screen_render_target.rendered_texture_id_ = 0;
  screen_render_target.depth_buffer_id_ = 0;
  mathfu::vec2i window_size = renderer.environment().GetViewportSize();
  screen_render_target.dimensions_ = window_size;
  screen_render_target.initialized_ = true;
  return screen_render_target;
}

}  // namespace fplbase
