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
#include "fplbase/fpl_common.h"
#include "fplbase/internal/type_conversions_gl.h"

namespace fplbase {

void RenderTarget::Initialize(const mathfu::vec2i& dimensions,
                              RenderTargetTextureFormat texture_format,
                              DepthStencilFormat depth_stencil_format) {
  assert(!initialized());

  GLint original_frame_buffer = 0;
  GLint original_render_buffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &original_frame_buffer);
  glGetIntegerv(GL_RENDERBUFFER_BINDING, &original_render_buffer);

  dimensions_ = dimensions;

  // Set up the framebuffer itself:
  framebuffer_id_ = InvalidBufferHandle();
  depth_buffer_id_ = InvalidBufferHandle();

  // Our framebuffer object:
  GLuint framebuffer_id = 0;
  GL_CALL(glGenFramebuffers(1, &framebuffer_id));
  framebuffer_id_ = TextureHandleFromGl(framebuffer_id);
  assert(ValidBufferHandle(framebuffer_id_));

  // Bind the framebuffer:
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id));

  // Is the texture format a depth format?
  const bool is_depth_texture =
      (texture_format >= kRenderTargetTextureFormatDepth16 &&
       texture_format <= kRenderTargetTextureFormatDepth32F);

  // The color buffer:
  if (texture_format != kRenderTargetTextureFormatNone) {
    // The texture we're going to render to
    GLuint rendered_texture_id = 0;
    GL_CALL(glGenTextures(1, &rendered_texture_id));
    rendered_texture_id_ = TextureHandleFromGl(rendered_texture_id);

    // Set up the texture:
    GL_CALL(glBindTexture(GL_TEXTURE_2D, rendered_texture_id));

    // Give an empty image to OpenGL.  (It will allocate memory, but not bother
    // to populate it.  Which is fine, since we're going to render into it.)
    GL_CALL(glTexImage2D(
        GL_TEXTURE_2D, 0,
        RenderTargetTextureFormatToInternalFormatGl(texture_format),
        dimensions.x, dimensions.y, 0,
        RenderTargetTextureFormatToFormatGl(texture_format),
        RenderTargetTextureFormatToTypeGl(texture_format), nullptr));

    // Define texture properties:
    const GLenum filter = is_depth_texture ? GL_NEAREST : GL_LINEAR;
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter));

    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    // Attach the texture to the frame buffer.
    const GLenum texture_target =
        is_depth_texture ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0;
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, texture_target,
                                   GL_TEXTURE_2D, rendered_texture_id, 0));

    if (is_depth_texture) {
      const GLenum draw_buffers = GL_NONE;
      GL_CALL(glDrawBuffers(1, &draw_buffers));
      GL_CALL(glReadBuffer(GL_NONE));
    }
  }

  if (depth_stencil_format != kDepthStencilFormatNone && !is_depth_texture) {
    // A renderbuffer, that we'll use for depth:
    GLuint depth_buffer_id = 0;
    GL_CALL(glGenRenderbuffers(1, &depth_buffer_id));
    depth_buffer_id_ = TextureHandleFromGl(depth_buffer_id);
    assert(ValidBufferHandle(depth_buffer_id_));

    // Bind renderbuffer and set it as the depth buffer:
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_id));
    GL_CALL(glRenderbufferStorage(
        GL_RENDERBUFFER,
        DepthStencilFormatToInternalFormatGl(depth_stencil_format),
        dimensions_.x, dimensions_.y));

    // Attach renderbuffer as our depth attachment.
    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, depth_buffer_id));
  }

  // Make sure everything worked:
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

  // Be good citizens and clean up:
  // Bind the framebuffer:
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, original_frame_buffer));
  GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, original_render_buffer));

  initialized_ = true;
}

void RenderTarget::Delete() {
  if (initialized_) {
    GLuint framebuffer_id = GlBufferHandle(framebuffer_id_);
    GL_CALL(glDeleteFramebuffers(1, &framebuffer_id));
    framebuffer_id_ = BufferHandleFromGl(framebuffer_id);

    GLuint depth_buffer_id = GlBufferHandle(depth_buffer_id_);
    GL_CALL(glDeleteRenderbuffers(1, &depth_buffer_id));
    depth_buffer_id_ = BufferHandleFromGl(depth_buffer_id);

    GLuint rendered_texture_id = GlBufferHandle(rendered_texture_id_);
    GL_CALL(glDeleteTextures(1, &rendered_texture_id));
    rendered_texture_id_ = TextureHandleFromGl(rendered_texture_id);

    initialized_ = false;
  }
}

// Set up all the rendering state so that the output is the texture in
// this rendertarget.
void RenderTarget::SetAsRenderTarget() const {
  // Calling SetAsRenderTarget on uninitialized rendertargets is bad.
  assert(initialized_);
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, GlBufferHandle(framebuffer_id_)));
  GL_CALL(glViewport(0, 0, dimensions_.x, dimensions_.y));
}

void RenderTarget::BindAsTexture(int texture_number) const {
  assert(initialized_);
  GL_CALL(glActiveTexture(GL_TEXTURE0 + texture_number));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, GlTextureHandle(rendered_texture_id_)));
}


}  // namespace fplbase
