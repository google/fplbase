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
#include "fplbase/version.h"
#include "mathfu/glsl_mappings.h"

#ifdef __ANDROID__
#include "fplbase/renderer_android.h"
#endif

namespace fplbase {

typedef void *Window;
typedef void *GLContext;

/// @file
/// @addtogroup fplbase_renderer
/// @{

class RenderTarget;

/// @class Renderer
/// @brief Manages the rendering system, handling the window and resources.
///
/// The core of the rendering system. Deals with setting up and shutting down
/// the window + OpenGL context, and creating/using resources
/// such as shaders, textures, and geometry.
class Renderer {
 public:
  Renderer();
  ~Renderer();

  enum CullingMode { kNoCulling, kCullFront, kCullBack, kCullFrontAndBack };

  // OpenGL ES feature level we are able to obtain.
  enum FeatureLevel {
    kFeatureLevel20,  // 2.0: Our fallback.
    kFeatureLevel30,  // 3.0: We request this by default.
  };

// clang-format off
#ifdef FPL_BASE_RENDERER_BACKEND_SDL
  /// @brief Creates the window + OpenGL context.
  ///
  /// window_size is used on desktop platforms, but typically is ignored on
  /// mobile platforms in favor of the native resolution.
  /// A descriptive error is in last_error() if it returns false.
  ///
  /// @param window_size The default size to initialize the window to.
  /// @param window_title The title of the created window.
  /// @return Returns true on success, false if there was an error.
#else
  /// @brief Initializes internal state of the renderer.
  ///
  /// This assumes a window and OpenGL context have already been created by
  /// the caller.
  ///
  /// @param window_size This is ignored and only present for compatibility
  /// with the interface that uses SDL as a backend.
  /// @param window_title This is ignored and only present for compatibility
  /// with the interface that uses SDL as a backend.
  /// @return Returns true on success, false if there was an error.
#endif  // FPL_BASE_RENDERER_BACKEND_SDL
  bool Initialize(const mathfu::vec2i &window_size, const char *window_title);
// clang-format on

#ifdef FPL_BASE_RENDERER_BACKEND_SDL
  /// @brief Swaps frames. Call this once per frame inside your main loop.
  ///
  /// The two arguments are typically the result of the InputManager's
  /// minimized() and Time() (seconds since the start of the program).
  ///
  /// @param minimized If the window is considered to be minimized.
  /// @param time The seconds since the start of the program.
  void AdvanceFrame(bool minimized, double time);

  /// @brief Cleans up the resources initialized by the renderer.
  void ShutDown();
#else
  /// @brief Sets the window size, for when window is not owned by the renderer.
  ///
  /// In the non-window-owning use case, call to update the window size whenever
  /// it changes.
  ///
  /// @param window_size The size to set the window to.
  void SetWindowSize(const mathfu::vec2i &window_size);
#endif  // FPL_BASE_RENDERER_BACKEND_SDL

  /// @brief Clears the framebuffer.
  ///
  /// Call this after AdvanceFrame if desired.
  ///
  /// @param color The color to clear the buffer to.
  void ClearFrameBuffer(const mathfu::vec4 &color);

  /// @brief Clears the depthbuffer. Leaves the colorbuffer untouched.
  void ClearDepthBuffer();

  /// @brief Create a shader object from two strings containing glsl code.
  ///
  /// Returns nullptr upon error, with a descriptive message in glsl_error().
  /// Attribute names in the vertex shader should be aPosition, aNormal,
  /// aTexCoord, aColor, aBoneIndices and aBoneWeights, to match whatever
  /// attributes your vertex data has.
  ///
  /// @param vs_source The source code of the vertex shader.
  /// @param ps_source The source code of the fragment shader.
  Shader *CompileAndLinkShader(const char *vs_source, const char *ps_source);

  /// @brief Sets the blend mode used by the renderer.
  ///
  /// Set alpha test (cull pixels with alpha below amount) vs alpha blend
  /// (blend with framebuffer pixel regardedless).
  ///
  /// @param blend_mode The type of blend mode, see materials.h for valid enum
  ///                   values.
  /// @param amount The value used with kBlendModeTest, defaults to 0.5f.
  void SetBlendMode(BlendMode blend_mode, float amount);
  void SetBlendMode(BlendMode blend_mode);

  /// @brief Sets the culling mode. By default, no culling happens.
  ///
  /// @param mode The type of culling mode to use.
  void SetCulling(CullingMode mode);

  /// @brief Set to compare fragment against Z-buffer before writing, or not.
  ///
  /// @param on Should depth testing be enabled.
  void DepthTest(bool on);

  /// @brief Turn on a scissor region. Arguments are in screen pixels.
  ///
  /// @param pos The lower left corner of the scissor box.
  /// @param size The width and height of the scissor box.s
  void ScissorOn(const mathfu::vec2i &pos, const mathfu::vec2i &size);
  /// @brief Turn off the scissor region.
  void ScissorOff();

  /// @brief Set bone transforms in vertex shader uniforms.
  ///
  /// Allows vertex shader to skin each vertex to the bone position.
  ///
  /// @param bone_transforms Array of transforms to pass to the shader.
  /// @param num_bones The length of the provided array.
  void SetAnimation(const mathfu::AffineTransform *bone_transforms,
                    int num_bones);

  /// @brief Shader uniform: model_view_projection
  /// @return Returns the current model view projection being used.
  const mathfu::mat4 &model_view_projection() const {
    return model_view_projection_;
  }
  /// @brief Sets the shader uniform model_view_projection
  /// @param mvp The model view projection to be passed to the shader.
  void set_model_view_projection(const mathfu::mat4 &mvp) {
    model_view_projection_ = mvp;
  }

  /// @brief Shader uniform: model (object to world transform only)
  /// @return Returns the current model transform being used.
  const mathfu::mat4 &model() const { return model_; }
  /// @brief Sets the shader uniform model transform.
  /// @param model The model transform to be passed to the shader.
  void set_model(const mathfu::mat4 &model) { model_ = model; }

  /// @brief Shader uniform: color
  /// @return Returns the current color being used.
  const mathfu::vec4 &color() const { return color_; }
  /// @brief Sets the shader uniform color.
  /// @param color The color to be passed to the shader.
  void set_color(const mathfu::vec4 &color) { color_ = color; }

  /// @brief Shader uniform: light_pos
  /// @return Returns the current light position being used.
  const mathfu::vec3 &light_pos() const { return light_pos_; }
  /// @brief Sets the shader uniform light position.
  /// @param light_pos The light position to be passed to the shader.
  void set_light_pos(const mathfu::vec3 &light_pos) { light_pos_ = light_pos; }

  /// @brief Shader uniform: camera_pos
  /// @return Returns the current camera position being used.
  const mathfu::vec3 &camera_pos() const { return camera_pos_; }
  /// @brief Sets the shader uniform camera position.
  /// @param camera_pos The camera position to be passed to the shader.
  void set_camera_pos(const mathfu::vec3 &camera_pos) {
    camera_pos_ = camera_pos;
  }

  /// @brief Shader uniform: bone_transforms
  /// @return Returns the current array of bone transforms being used.
  const mathfu::AffineTransform *bone_transforms() const {
    return bone_transforms_;
  }
  /// @brief The number of bones in the bone_transforms() array.
  /// @return Returns the length of the bone_transforms() array.
  int num_bones() const { return num_bones_; }
  /// @brief Sets the shader uniform bone transforms.
  /// @param bone_transforms The bone transforms to be passed to the shader.
  /// @param num_bones The length of the bone_transforms array provided.
  void SetBoneTransforms(const mathfu::AffineTransform *bone_transforms,
                         int num_bones) {
    bone_transforms_ = bone_transforms;
    num_bones_ = num_bones;
  }

  /// @brief Contains the last error that occurred, if there is one.
  ///
  /// If any of the more complex loading operations (shaders, textures etc.)
  /// fail, this sting will contain a more informative error message.
  ///
  /// @brief Returns the last error that occurred.
  const std::string &last_error() const { return last_error_; }
  void set_last_error(const std::string &last_error) {
    last_error_ = last_error;
  }

  /// @brief The device's current framebuffer size.
  ///
  /// May change from frame to frame due to window resizing or Android
  /// navigation buttons turning on/off.
  ///
  /// @return Returns the current framebuffer size.
  const mathfu::vec2i &window_size() const { return window_size_; }
  mathfu::vec2i &window_size() { return window_size_; }
  void set_window_size(const mathfu::vec2i &ws) { window_size_ = ws; }

  /// @brief Get the size of the viewport.
  ///
  /// This may be larger than the framebuffer/window on Android if the hardware
  /// scalar is enabled.
  ///
  /// @return Returns the current viewport size.
  mathfu::vec2i GetViewportSize();

  /// @brief Time in seconds since program start.
  ///
  /// Used by animated shaders, updated once per frame only.
  ///
  /// @return Returns the time in seconds since program start.
  double time() const { return time_; }

  /// @brief The supported OpenGL ES feature level.
  /// @return Returns the supported OpenGL ES feature level.
  FeatureLevel feature_level() const { return feature_level_; }

  /// @brief The blend that will be used for all draw calls.
  /// @return Returns the blend mode that will be used.
  BlendMode force_blend_mode() const { return force_blend_mode_; }
  /// @brief Set to override the blend mode used for all draw calls.
  ///
  /// Overrides the blend that was set by calling SetBlendMode.
  /// Set to kBlendModeCount to disable.
  ///
  /// @param bm The blend mode to be used.
  void set_force_blend_mode(BlendMode bm) { force_blend_mode_ = bm; }

  /// @brief Set this force any shader that gets loaded to use this pixel shader
  ///        instead (for debugging purposes).
  /// @param ps The pixel shader that will be used.
  void set_override_pixel_shader(const std::string &ps) {
    override_pixel_shader_ = ps;
  }

  /// @brief Get the max number of uniforms components (i.e. individual floats,
  ///        so a mat4 needs 16 of them).
  ///
  /// The value is in individual floats, so a mat4 needs 16 of them. This
  /// variable is also available in the shader as
  /// GL_MAX_VERTEX_UNIFORM_COMPONENTS. From this, you can compute safe sizes
  /// of uniform arrays etc.
  ///
  /// @return Returns the max number of uniform components.
  int max_vertex_uniform_components() { return max_vertex_uniform_components_; }

  /// @brief Returns the version of the FPL Base Library.
  const FplBaseVersion *GetFplBaseVersion() const { return version_; }

 private:
  ShaderHandle CompileShader(bool is_vertex_shader, ShaderHandle program,
                             const char *source);
  // Retrieve uniform limits from the driver and cache for shader compilation.
  void InitializeUniformLimits();

  // The mvp. Use the Ortho() and Perspective() methods in mathfu::Matrix
  // to conveniently change the camera.
  mathfu::mat4 model_view_projection_;
  mathfu::mat4 model_;
  mathfu::vec4 color_;
  mathfu::vec3 light_pos_;
  mathfu::vec3 camera_pos_;
  const mathfu::AffineTransform *bone_transforms_;
  int num_bones_;
  double time_;
  mathfu::vec2i window_size_;

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

  // Current version of the Corgi Entity Library.
  const FplBaseVersion *version_;
};

/// @}

}  // namespace fplbase

#endif  // FPLBASE_RENDERER_H
