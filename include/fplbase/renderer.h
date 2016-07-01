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
class RenderTarget;

enum CullingMode {
  kCullingModeNone,
  kCullingModeFront,
  kCullingModeBack,
  kCullingModeFrontAndBack
};

/// @file
/// @addtogroup fplbase_renderer
/// @{

/// @class RenderContext
/// @brief Render context keeps graphics context specific data.
///
/// For graphics APIs that support multi-threading, like Vulkan,
/// the RenderContext class is a place for keeping data specific
/// to a render thread.
class RenderContext {
 public:
  // Render Context Class
  RenderContext()
      : blend_mode_(kBlendModeOff),
        model_view_projection_(mathfu::mat4::Identity()),
        model_(mathfu::mat4::Identity()),
        color_(mathfu::kOnes4f),
        light_pos_(mathfu::kZeros3f),
        camera_pos_(mathfu::kZeros3f),
        bone_transforms_(nullptr),
        num_bones_(0) {}

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

  // other render state
  Shader *shader_;
  BlendMode blend_mode_;
  float blend_amount_;
  CullingMode cull_mode_;
  bool depth_test;

 private:
  // The mvp. Use the Ortho() and Perspective() methods in mathfu::Matrix
  // to conveniently change the camera.
  mathfu::mat4 model_view_projection_;
  mathfu::mat4 model_;
  mathfu::vec4 color_;
  mathfu::vec3 light_pos_;
  mathfu::vec3 camera_pos_;
  const mathfu::AffineTransform *bone_transforms_;
  int num_bones_;
};

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

  // Get current singleton instance.
  static Renderer *Get() { return the_renderer_; }

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
  /// @param render_context Pointer to the render context
  void ClearFrameBuffer(const mathfu::vec4 &color,
                        RenderContext *render_context);
  /// @overload void ClearFrameBuffer(const mathfu::vec4 &color)
  void ClearFrameBuffer(const mathfu::vec4 &color) {
    ClearFrameBuffer(color, default_render_context_);
  }

  /// @brief Clears the depthbuffer. Leaves the colorbuffer untouched.
  /// @param render_context Pointer to the render context
  void ClearDepthBuffer(RenderContext *render_context);
  /// @overload void ClearDepthBuffer()
  void ClearDepthBuffer() { ClearDepthBuffer(default_render_context_); }

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

  /// @brief Like CompileAndLinkShader, but pass in an old shader to replace.
  ///
  /// Use `placement new` to use the same memory for the new shader.
  /// @note Only call this at the start of the frame.
  ///
  /// @param shader The old shader to replace with the recompiled shader.
  /// @param vs_source The source code of the vertex shader.
  /// @param ps_source The source code of the fragment shader.
  void RecompileShader(const char *vs_source, const char *ps_source,
                       Shader *shader);

  /// @brief Begin rendering commands.
  /// This must be called before any rendering commands are done
  /// @param render_context Pointer to a render context object
  void BeginRendering(RenderContext *render_context);
  /// @overload void BeginRendering()
  void BeginRendering() { BeginRendering(default_render_context_); }

  /// @brief End rendering commands.
  /// This is called after all of the rendering commands are done
  /// @param render_context Pointer to a render context object
  void EndRendering(RenderContext *render_context);
  /// @overload void EndRendering()
  void EndRendering() { EndRendering(default_render_context_); }

  /// @brief Checks for multithreading API.
  /// Returns true if the graphics API allows multi-threading
  bool AllowMultiThreading();

  /// @brief Sets the blend mode used by the renderer.
  ///
  /// Set alpha test (cull pixels with alpha below amount) vs alpha blend
  /// (blend with framebuffer pixel regardedless).
  ///
  /// @param blend_mode The type of blend mode, see materials.h for valid enum
  ///                   values.
  /// @param amount The value used with kBlendModeTest, defaults to 0.5f.
  /// @param render_context Pointer to the render context
  void SetBlendMode(BlendMode blend_mode, float amount,
                    RenderContext *render_context);
  /// @overload void SetBlendMode(BlendMode blend_mode, float amount)
  void SetBlendMode(BlendMode blend_mode, float amount) {
    SetBlendMode(blend_mode, amount, default_render_context_);
  }
  /// @overload void SetBlendMode(BlendMode blend_mode, RenderContext
  /// *render_context)
  void SetBlendMode(BlendMode blend_mode, RenderContext *render_context);
  /// @overload void SetBlendMode(BlendMode blend_mode)
  void SetBlendMode(BlendMode blend_mode) {
    SetBlendMode(blend_mode, default_render_context_);
  }

  /// @brief Sets the culling mode. By default, no culling happens.
  ///
  /// @param mode The type of culling mode to use.
  /// @param render_context Pointer to the render context
  void SetCulling(CullingMode mode, RenderContext *render_context);
  /// @overload SetCulling(CullingMode mode)
  void SetCulling(CullingMode mode) {
    SetCulling(mode, default_render_context_);
  }

  /// @brief Set to compare fragment against Z-buffer before writing, or not.
  ///
  /// @param on Should depth testing be enabled.
  /// @param render_context Pointer to the render context
  void DepthTest(bool on, RenderContext *render_context);
  /// @overload DepthTest(bool on)
  void DepthTest(bool on) { DepthTest(on, default_render_context_); }

  /// @brief Turn on a scissor region. Arguments are in screen pixels.
  ///
  /// @param pos The lower left corner of the scissor box.
  /// @param size The width and height of the scissor box.s
  /// @param render_context Pointer to the render context
  void ScissorOn(const mathfu::vec2i &pos, const mathfu::vec2i &size,
                 RenderContext *render_context);
  /// @overload void ScissorOn(const mathfu::vec2i &ops, const mathfu::vec2i
  /// &size)
  void ScissorOn(const mathfu::vec2i &ops, const mathfu::vec2i &size) {
    ScissorOn(ops, size, default_render_context_);
  }
  /// @brief Turn off the scissor region.
  /// @param render_context Pointer to the render context
  void ScissorOff(RenderContext *render_context);
  /// @overload void ScissorOff()
  void ScissorOff() { ScissorOff(default_render_context_); }

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
  /// @param render_context Pointer to the render context
  const mathfu::mat4 &model_view_projection(
      RenderContext *render_context) const {
    return render_context->model_view_projection();
  }
  /// @overload const mathfu::mat4 &model_view_projection() const
  const mathfu::mat4 &model_view_projection() const {
    return model_view_projection(default_render_context_);
  }
  /// @brief Sets the shader uniform model_view_projection
  /// @param mvp The model view projection to be passed to the shader.
  /// @param render_context Pointer to the render context
  void set_model_view_projection(const mathfu::mat4 &mvp,
                                 RenderContext *render_context) {
    render_context->set_model_view_projection(mvp);
  }
  /// @overload void set_model_view_projection(const mathfu::mat4 &mvp)
  void set_model_view_projection(const mathfu::mat4 &mvp) {
    set_model_view_projection(mvp, default_render_context_);
  }

  /// @brief Shader uniform: model (object to world transform only)
  /// @return Returns the current model transform being used.
  /// @param render_context Pointer to the render context
  const mathfu::mat4 &model(RenderContext *render_context) const {
    return render_context->model();
  }
  /// @overload const mathfu::mat4 &model()
  const mathfu::mat4 &model() const { return model(default_render_context_); }
  /// @brief Sets the shader uniform model transform.
  /// @param model The model transform to be passed to the shader.
  /// @param render_context Pointer to the render context
  void set_model(const mathfu::mat4 &model, RenderContext *render_context) {
    render_context->set_model(model);
  }
  /// @overload void set_model(const mathfu::mat4 &model)
  void set_model(const mathfu::mat4 &model) {
    set_model(model, default_render_context_);
  }

  /// @brief Shader uniform: color
  /// @return Returns the current color being used.
  /// @param render_context Pointer to the render context
  const mathfu::vec4 &color(RenderContext *render_context) const {
    return render_context->color();
  }
  /// @overload const mathfu::vec4 &color()
  const mathfu::vec4 &color() const { return color(default_render_context_); }

  /// @brief Sets the shader uniform color.
  /// @param color The color to be passed to the shader.
  /// @param render_context Pointer to the render context
  void set_color(const mathfu::vec4 &color, RenderContext *render_context) {
    render_context->set_color(color);
  }
  /// @overload set_color(const mathfu::vec4 &color)
  void set_color(const mathfu::vec4 &color) {
    set_color(color, default_render_context_);
  }

  /// @brief Shader uniform: light_pos
  /// @return Returns the current light position being used.
  /// @param render_context Pointer to the render context
  const mathfu::vec3 &light_pos(RenderContext *render_context) const {
    return render_context->light_pos();
  }
  /// @overload const mathfu::vec3 &light_pos()
  const mathfu::vec3 &light_pos() const {
    return light_pos(default_render_context_);
  }
  /// @brief Sets the shader uniform light position.
  /// @param light_pos The light position to be passed to the shader.
  /// @param render_context Pointer to the render context
  void set_light_pos(const mathfu::vec3 &light_pos,
                     RenderContext *render_context) {
    render_context->set_light_pos(light_pos);
  }
  /// @overload void set_light_pos(const mathfu::vec3 &light_pos)
  void set_light_pos(const mathfu::vec3 &light_pos) {
    set_light_pos(light_pos, default_render_context_);
  }

  /// @brief Shader uniform: camera_pos
  /// @return Returns the current camera position being used.
  /// @param render_context Pointer to the render context
  const mathfu::vec3 &camera_pos(RenderContext *render_context) const {
    return render_context->camera_pos();
  }
  /// @overload const mathfu::vec3 &camera_pos()
  const mathfu::vec3 &camera_pos() const {
    return camera_pos(default_render_context_);
  }
  /// @brief Sets the shader uniform camera position.
  /// @param camera_pos The camera position to be passed to the shader.
  /// @param render_context Pointer to the render context
  void set_camera_pos(const mathfu::vec3 &camera_pos,
                      RenderContext *render_context) {
    render_context->set_camera_pos(camera_pos);
  }
  /// @overload void set_camera_pos(const mathfu::vec3 &camera_pos)
  void set_camera_pos(const mathfu::vec3 &camera_pos) {
    set_camera_pos(camera_pos, default_render_context_);
  }

  /// @brief Shader uniform: bone_transforms
  /// @return Returns the current array of bone transforms being used.
  /// @param render_context Pointer to the render context
  const mathfu::AffineTransform *bone_transforms(
      RenderContext *render_context) const {
    return render_context->bone_transforms();
  }
  /// @overload const mathfu::AffineTransform *bone_transforms() const
  const mathfu::AffineTransform *bone_transforms() const {
    return bone_transforms(default_render_context_);
  }
  /// @brief The number of bones in the bone_transforms() array.
  /// @return Returns the length of the bone_transforms() array.
  /// @param render_context Pointer to the render context
  int num_bones(RenderContext *render_context) const {
    return render_context->num_bones();
  }
  /// @overload int num_bones() const
  int num_bones() const { return num_bones(default_render_context_); }
  /// @brief Sets the shader uniform bone transforms.
  /// @param bone_transforms The bone transforms to be passed to the shader.
  /// @param num_bones The length of the bone_transforms array provided.
  /// @param render_context Pointer to the render context
  void SetBoneTransforms(const mathfu::AffineTransform *bone_transforms,
                         int num_bones, RenderContext *render_context) {
    render_context->SetBoneTransforms(bone_transforms, num_bones);
  }
  /// @overload void SetBoneTransforms(const mathfu::AffineTransform
  /// *bone_transforms,int num_bones)
  void SetBoneTransforms(const mathfu::AffineTransform *bone_transforms,
                         int num_bones) {
    SetBoneTransforms(bone_transforms, num_bones, default_render_context_);
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
  /// @overload mathfu::vec2i &window_size()
  mathfu::vec2i &window_size() { return window_size_; }

  /// @brief Sets the window size
  ///
  /// Sets the window_size member variable to the given parameter
  /// @param ws The 2D vector for the window size
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

  /// @brief Sets the texture to be used for the next draw call
  ///
  /// @param unit The texture unit to set the texture
  /// @param texture Pointer to the texture object
  /// @param render_context Pointer to the texture object
  void SetTexture(unsigned unit, Texture *texture,
                  RenderContext *render_context) {
    texture->Set(unit, render_context);
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

  /// @brief Returns if a texture format is supported by the hardware.
  bool SupportsTextureFormat(TextureFormat texture_format) const;

  /// @brief Returns if a NPOT textures are supported by the hardware.
  /// see: https://www.opengl.org/wiki/NPOT_Texture
  bool SupportsTextureNpot() const;

 private:
  ShaderHandle CompileShader(bool is_vertex_shader, ShaderHandle program,
                             const char *source);
  Shader *CompileAndLinkShaderHelper(const char *vs_source,
                                     const char *ps_source, Shader *shader);

  RenderContext *default_render_context() { return default_render_context_; }

  // Initialize OpenGL parameters like uniform limits, supported texture formats
  // etc.
  bool InitializeRenderingState();

  double time_;
  mathfu::vec2i window_size_;

  std::string last_error_;

  RenderContext *default_render_context_;

#ifdef FPL_BASE_RENDERER_BACKEND_SDL
  Window window_;
  GLContext context_;
#endif

  FeatureLevel feature_level_;
  int64_t supports_texture_format_;  // 1 bit for each enum in TextureFormat.

  bool supports_texture_npot_;

  Shader *force_shader_;
  BlendMode force_blend_mode_;
  std::string override_pixel_shader_;

  int max_vertex_uniform_components_;

  // Current version of the Corgi Entity Library.
  const FplBaseVersion *version_;

  // Singleton instance.
  static Renderer *the_renderer_;
};

/// @}

}  // namespace fplbase

#endif  // FPLBASE_RENDERER_H
