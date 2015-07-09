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

#ifndef FPLBASE_RENDER_TARGET_H
#define FPLBASE_RENDER_TARGET_H

#include "fplbase/config.h" // Must come first.

#include "mathfu/glsl_mappings.h"
#include "fplbase/material.h"
#include "fplbase/mesh.h"
#include "fplbase/renderer.h"
#include "fplbase/shader.h"

#ifdef __ANDROID__
#include "fplbase/renderer_android.h"
#endif

namespace fpl {

// Abstracts a surface that can be rendered to.
// Calling SetAsRenderTarget() will cause all subsequent draw calls to be
// drawn onto the RenderTarget instead of to the screen buffer.
class RenderTarget {
 public:
  RenderTarget() : initialized_(false) {}
  // Initialize a render target of the provided dimensions.  If format and
  // useDepthBuffer are not provided, it defaults to GL_UNSIGNED_SHORT_5_6_5,
  // with a depth buffer attached.
  void Initialize(mathfu::vec2i dimensions);
  void Initialize(mathfu::vec2i dimensions, GLenum format, bool useDepthBuffer);

  // Deletes the associated opengl resources associtaed with the RenderTarget.
  void Delete();

  // Sets the RenderTarget as the active render target.  I. e. all subsequent
  // openGL draw calls will render to this RenderTarget instead of wherever
  // they were going before.
  void SetAsRenderTarget() const;

  // Binds the texture associated with this rendertarget as the active texture.
  // Primarily useful when rendering the RenderTarget's texture as part of a
  // mesh.  Throws an assert if the RenderTarget does not have a texture.
  void BindAsTexture(int texture_number) const;

  // Returns true if this rendertarget refers to an off-screen texture, and
  // false if it refers to the screen itself.  (This is important because
  // rendertargets that aren't texture-based will assert if you try to
  // bind them as texture or access their textureId.)
  inline bool IsTexture() const { return framebuffer_id_ == 0; }

  // Gets the TextureId associated with the RenderTarget, assuming that it is
  // texture-based.  (Throws an assert if you try to call GetTextureId on a
  // RenderTarget that doesn't have a texture backing it, such as the screen's
  // display buffer.
  inline unsigned int GetTextureId() const {
    assert(IsTexture());
    return rendered_texture_id_;
  }

  // Member accessor for initialized variable.  Returns true
  // if this RenderTarget has been initialized and is ready to use.
  // Returns false if has not yet been initialized, failed initalization, or
  // has been deleted.  Trying to use an uninitialized RenderTarget will
  // generally cause errors or asserts.
  bool initialized() const { return initialized_; }

  static RenderTarget ScreenRenderTarget(Renderer& renderer);

 private:
  mathfu::vec2i dimensions_;
  GLuint framebuffer_id_;
  GLuint rendered_texture_id_;
  GLuint depth_buffer_id_;
  bool initialized_;
};

}  // namespace fpl

#endif  // FPLBASE_RENDER_TARGET_H
