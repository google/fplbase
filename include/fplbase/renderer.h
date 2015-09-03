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

#include "fplbase/config.h" // Must come first.

#include "mathfu/glsl_mappings.h"
#include "fplbase/material.h"
#include "fplbase/mesh.h"
#include "fplbase/shader.h"

#ifdef __ANDROID__
#  include "fplbase/renderer_android.h"
#endif

namespace fpl {

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
    kFeatureLevel20,    // 2.0: Our fallback.
    kFeatureLevel30,    // 3.0: We request this by default.
  };

# ifdef FPL_BASE_RENDERER_BACKEND_SDL
  // Creates the window + OpenGL context.
  // A descriptive error is in last_error() if it returns false.
  bool Initialize(const vec2i &window_size = vec2i(800, 600),
                  const char *window_title = "");

  // Swaps frames. Call this once per frame inside your main loop.
  // The two arguments are typically the result of the InputManager's
  // minimized() and Time() (seconds since the start of the program).
  void AdvanceFrame(bool minimized, double time);

  // Cleans up whatever Initialize creates.
  void ShutDown();
# else
  // In the non-window-owning use case, call to update the window size whenever
  // it changes.
  void SetWindowSize(const vec2i &window_size);
# endif

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

  // Create a texture from a memory buffer containing xsize * ysize RGBA pixels.
  // Return 0 if not a power of two in size.
  TextureHandle CreateTexture(const uint8_t *buffer, const vec2i &size,
                               bool has_alpha, bool mipmaps = true,
                               TextureFormat desired = kFormatAuto);

  // Update (part of) the current texture with new pixel data.
  // For now, must always update at least entire rows.
  void UpdateTexture(TextureFormat format, int xoffset, int yoffset, int width,
                     int height, const void *data);

  // Unpacks a memory buffer containing a TGA format file.
  // May only be uncompressed RGB or RGBA data, Y-flipped or not.
  // Returns RGBA array of returned dimensions or nullptr if the
  // format is not understood.
  // You must free() the returned pointer when done.
  uint8_t *UnpackTGA(const void *tga_buf, vec2i *dimensions, bool *has_alpha);

  // Unpacks a memory buffer containing a Webp format file.
  // Returns RGBA array of the returned dimensions or nullptr if the format
  // is not understood.
  // You must free() the returned pointer when done.
  uint8_t *UnpackWebP(const void *webp_buf, size_t size, vec2i *dimensions,
                      bool *has_alpha);

  // Loads the file in filename, and then unpacks the file format (supports
  // TGA and WebP).
  // last_error() contains more information if nullptr is returned.
  // You must free() the returned pointer when done.
  uint8_t *LoadAndUnpackTexture(const char *filename, vec2i *dimensions,
                                bool *has_alpha);

  // Utility functions to convert 32bit RGBA to 16bit.
  // You must delete[] the return value afterwards.
  uint16_t *Convert8888To5551(const uint8_t *buffer, const vec2i &size);
  uint16_t *Convert888To565(const uint8_t *buffer, const vec2i &size);

  // Set alpha test (cull pixels with alpha below amount) vs alpha blend
  // (blend with framebuffer pixel regardedless).
  // blend_mode: see materials.fbs for valid enum values.
  void SetBlendMode(BlendMode blend_mode, float amount = 0.5f);

  // Set culling mode.  By default, no culling happens.
  void SetCulling(CullingMode mode);

  // Set to compare fragment against Z-buffer before writing, or not.
  void DepthTest(bool on);

  // Set the current render target.
  void SetRenderTarget(const RenderTarget& render_target);

  // Turn on/off a scissor region. Arguments are in screen pixels.
  void ScissorOn(const vec2i &pos, const vec2i &size);
  void ScissorOff();

  // Set bone transforms in vertex shader uniforms.
  // Allows vertex shader to skin each vertex to the bone position.
  void SetAnimation(const mathfu::mat4 *bone_transforms, int num_bones);

  // Shader uniform: model_view_projection
  mat4 &model_view_projection() { return model_view_projection_; }
  const mat4 &model_view_projection() const { return model_view_projection_; }

  // Shader uniform: model (object to world transform only)
  mat4 &model() { return model_; }
  const mat4 &model() const { return model_; }

  // Shader uniform: color
  vec4 &color() { return color_; }
  const vec4 &color() const { return color_; }

  // Shader uniform: light_pos
  vec3 &light_pos() { return light_pos_; }
  const vec3 &light_pos() const { return light_pos_; }

  // Shader uniform: camera_pos
  vec3 &camera_pos() { return camera_pos_; }
  const vec3 &camera_pos() const { return camera_pos_; }

  // Shader uniform: bone_transforms
  const mat4 *bone_transforms() const { return bone_transforms_; }
  int num_bones() const { return num_bones_; }
  void SetBoneTransforms(const mat4 *bone_transforms, int num_bones) {
    bone_transforms_ = bone_transforms;
    num_bones_ = num_bones;
  }

  // If any of the more complex loading operations (shaders, textures etc.)
  // fail, this sting will contain a more informative error message.
  std::string &last_error() { return last_error_; }
  const std::string &last_error() const { return last_error_; }

  // The device's current framebuffer size. May change from frame to frame
  // due to window resizing or Android navigation buttons turning on/off.
  vec2i &window_size() { return window_size_; }
  const vec2i &window_size() const { return window_size_; }

  // Time in seconds since program start, as used by animated shaders,
  // updated once per frame only.
  double time() const { return time_; }

  // Get the supported OpenGL ES feature level;
  FeatureLevel feature_level() const { return feature_level_; }

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
  const mat4 *bone_transforms_;
  int num_bones_;
  double time_;
  vec2i window_size_;

  std::string last_error_;

# ifdef FPL_BASE_RENDERER_BACKEND_SDL
  Window window_;
  GLContext context_;
# endif

  BlendMode blend_mode_;

  FeatureLevel feature_level_;

  bool use_16bpp_;
};

}  // namespace fpl

#endif  // FPLBASE_RENDERER_H
