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

#ifndef FPLBASE_RENDERER_H
#define FPLBASE_RENDERER_H

#include "fplbase/config.h"  // Must come first.

#include "fplbase/material.h"
#include "fplbase/mesh.h"
#include "fplbase/shader.h"
#include "fplbase/texture.h"
#include "mathfu/glsl_mappings.h"

#ifdef __ANDROID__
#include "fplbase/renderer_android.h"
#endif

namespace fpl {

using mathfu::mat4;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;

typedef void *Window;
typedef void *GLContext;

class RenderTarget;

// The core of the rendering system. Deals with setting up and shutting down
// the window + OpenGL context (based on SDL), and creating/using resources
// such as shaders, textures, and geometry.
class Renderer {
 public:
  Renderer();
  ~Renderer();

  enum CullingMode {
    kNoCulling,
    kCullFront,
    kCullBack,
    kCullFrontAndBack
  };

  // OpenGL ES feature level we are able to obtain.
  enum FeatureLevel {
    kFeatureLevel20,  // 2.0: Our fallback.
    kFeatureLevel30,  // 3.0: We request this by default.
  };

#ifdef FPL_BASE_RENDERER_BACKEND_SDL
  // Creates the window + OpenGL context.
  // window_size is used on desktop platforms, but typically is ignored on
  // mobile platforms in favor of the native resolution.
  // A descriptive error is in last_error() if it returns false.
  bool Initialize(const vec2i &window_size, const char *window_title);

  // Swaps frames. Call this once per frame inside your main loop.
  // The two arguments are typically the result of the InputManager's
  // minimized() and Time() (seconds since the start of the program).
  void AdvanceFrame(bool minimized, double time);

  // Cleans up whatever Initialize creates.
  void ShutDown();
#else
  // In the non-window-owning use case, call to update the window size whenever
  // it changes.
  void SetWindowSize(const vec2i &window_size);
#endif

  // Clears the framebuffer. Call this after AdvanceFrame if desired.
  void ClearFrameBuffer(const vec4 &color);

  // Clears the depthbuffer.  Leaves the colorbuffer untouched.
  void ClearDepthBuffer();

  // Create a shader object from two strings containing glsl code.
  // Returns nullptr upon error, with a descriptive message in glsl_error().
  // Attribute names in the vertex shader should be aPosition, aNormal,
  // aTexCoord, aColor, aBoneIndices and aBoneWeights, to match whatever
  // attributes your vertex data has.
  Shader *CompileAndLinkShader(const char *vs_source, const char *ps_source);

  // Set alpha test (cull pixels with alpha below amount) vs alpha blend
  // (blend with framebuffer pixel regardedless).
  // Pass amount for use with kBlendModeTest, default 0.5f.
  // blend_mode: see materials.h for valid enum values.
  void SetBlendMode(BlendMode blend_mode, float amount);
  void SetBlendMode(BlendMode blend_mode);

  // Set culling mode.  By default, no culling happens.
  void SetCulling(CullingMode mode);

  // Set to compare fragment against Z-buffer before writing, or not.
  void DepthTest(bool on);

  // Set the current render target.
  void SetRenderTarget(const RenderTarget &render_target);

  // Turn on/off a scissor region. Arguments are in screen pixels.
  void ScissorOn(const vec2i &pos, const vec2i &size);
  void ScissorOff();

  // Set bone transforms in vertex shader uniforms.
  // Allows vertex shader to skin each vertex to the bone position.
  void SetAnimation(const mathfu::AffineTransform *bone_transforms,
                    int num_bones);

  // Shader uniform: model_view_projection
  const mat4 &model_view_projection() const { return model_view_projection_; }
  void set_model_view_projection(const mat4 &mvp) {
    model_view_projection_ = mvp;
  }

  // Shader uniform: model (object to world transform only)
  const mat4 &model() const { return model_; }
  void set_model(const mat4 &model) { model_ = model; }

  // Shader uniform: color
  const vec4 &color() const { return color_; }
  void set_color(const vec4 &color) { color_ = color; }

  // Shader uniform: light_pos
  const vec3 &light_pos() const { return light_pos_; }
  void set_light_pos(const vec3 &light_pos) { light_pos_ = light_pos; }

  // Shader uniform: camera_pos
  const vec3 &camera_pos() const { return camera_pos_; }
  void set_camera_pos(const vec3 &camera_pos) { camera_pos_ = camera_pos; }

  // Shader uniform: bone_transforms
  const mathfu::AffineTransform *bone_transforms() const {
    return bone_transforms_;
  }
  int num_bones() const { return num_bones_; }
  void SetBoneTransforms(const mathfu::AffineTransform *bone_transforms,
                         int num_bones) {
    bone_transforms_ = bone_transforms;
    num_bones_ = num_bones;
  }

  // If any of the more complex loading operations (shaders, textures etc.)
  // fail, this sting will contain a more informative error message.
  const std::string &last_error() const { return last_error_; }
  void set_last_error(const std::string &last_error) {
    last_error_ = last_error;
  }

  // The device's current framebuffer size. May change from frame to frame
  // due to window resizing or Android navigation buttons turning on/off.
  const vec2i &window_size() const { return window_size_; }
  vec2i &window_size() { return window_size_; }
  void set_window_size(const vec2i &ws) { window_size_ = ws; }

  // Get the size of the viewport.  This may be larger than the framebuffer /
  // window on Android if the hardware scalar is enabled.
  vec2i GetViewportSize();

  // Time in seconds since program start, as used by animated shaders,
  // updated once per frame only.
  double time() const { return time_; }

  // Get the supported OpenGL ES feature level;
  FeatureLevel feature_level() const { return feature_level_; }

  // Set this to override the blend used for all draw calls (after calling
  // SetBlendMode for it).
  BlendMode force_blend_mode() const {
    return force_blend_mode_;
  }
  void set_force_blend_mode(BlendMode bm) {
    force_blend_mode_ = bm;
  }

  // Set this force any shader that gets loaded to use this pixel shader
  // instead (for debugging purposes).
  void set_override_pixel_shader(const std::string &ps) {
    override_pixel_shader_ = ps;
  }

  // Get the max number of uniforms components (i.e. individual floats, so
  // a mat4 needs 16 of them). This variable is also available in the
  // shader as GL_MAX_VERTEX_UNIFORM_COMPONENTS.
  // From this, you can compute safe sizes of uniform arrays etc.
  int max_vertex_uniform_components() { return max_vertex_uniform_components_; }

 private:
  ShaderHandle CompileShader(bool is_vertex_shader, ShaderHandle program,
                             const char *source);

  // The mvp. Use the Ortho() and Perspective() methods in mathfu::Matrix
  // to conveniently change the camera.
  mat4 model_view_projection_;
  mat4 model_;
  vec4 color_;
  vec3 light_pos_;
  vec3 camera_pos_;
  const mathfu::AffineTransform *bone_transforms_;
  int num_bones_;
  double time_;
  vec2i window_size_;

  std::string last_error_;

#ifdef FPL_BASE_RENDERER_BACKEND_SDL
  Window window_;
  GLContext context_;
#endif

  BlendMode blend_mode_;

  FeatureLevel feature_level_;

  Shader *force_shader_;
  BlendMode force_blend_mode_;
  std::string override_pixel_shader_;

  int max_vertex_uniform_components_;
};

}  // namespace fpl

#endif  // FPLBASE_RENDERER_H
