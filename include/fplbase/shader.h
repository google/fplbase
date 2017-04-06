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

#ifndef FPLBASE_SHADER_H
#define FPLBASE_SHADER_H

#include <set>

#include "fplbase/config.h"  // Must come first.

#include "fplbase/async_loader.h"
#include "fplbase/handles.h"
#include "mathfu/glsl_mappings.h"

namespace fplbase {
class Renderer;
struct ShaderImpl;

/// @file
/// @addtogroup fplbase_shader
/// @{

static const int kMaxTexturesPerShader = 8;
static const int kNumVec4sInAffineTransform = 3;

/// @class Shader
/// @brief Represents a shader consisting of a vertex and pixel shader.
///
/// Represents a shader consisting of a vertex and pixel shader. Also stores
/// ids of standard uniforms. Use the Renderer class below to create these.
class Shader : public AsyncAsset {
 public:
  Shader(const char *filename, const std::vector<std::string> &local_defines,
         Renderer *renderer)
      : AsyncAsset(filename ? filename : ""), impl_(CreateShaderImpl()) {
    const ShaderHandle invalid = InvalidShaderHandle();
    Init(invalid /* program */, invalid /* vs */, invalid /* ps */,
         local_defines, renderer);
  }

  Shader(ShaderHandle program, ShaderHandle vs, ShaderHandle ps)
      : impl_(CreateShaderImpl()) {
    static const std::vector<std::string> empty_defines;
    Init(program, vs, ps, empty_defines, nullptr /* renderer */);
  }

  ~Shader();

  /// @brief Recalculates the defines of this shader, and marks dirty if
  /// they differ from the last Reload().
  ///
  /// Defines change the shader source code when it is preprocessed.
  /// Typically, you have one uber-shader that contains all possible shader
  /// features, and enable the features using preprocessor defines.
  /// This function allows you to specify which features are enabled.
  ///
  /// @note This function does not reload the shader. It will simply mark
  /// the shader as dirty, if its defines have changed.
  ///
  /// @param global_defines_to_add the defines to be added into 'local_defines'
  /// above. These are generally global rendering settings, for example,
  /// enabling shadows or specular.
  /// @param global_defines_to_omit the defines to forcefully omit from
  /// 'local_defines'. Generally used to globally disable expensive features
  /// such as shadows, on low-powered hardware.
  void UpdateGlobalDefines(
      const std::vector<std::string> &global_defines_to_add,
      const std::vector<std::string> &global_defines_to_omit);

  /// @brief If the shader has been marked dirty, reload it and clear the
  /// dirty flag.
  ///
  /// Lazy loading ensures shaders are only loaded and compiled when they
  /// are used. Also allows the caller to ensure that the reload is happening
  /// on the correct thread.
  bool ReloadIfDirty();

  /// @brief Loads and unpacks the Shader from `filename_` into `data_`.
  virtual void Load();

  /// @brief Creates a Shader from `data_`.
  virtual bool Finalize();

  /// @brief Whether this object loaded and finalized correctly. Call after
  /// Finalize has been called (by AssetManager::TryFinalize).
  bool IsValid() { return ValidShaderHandle(program_); }

  /// @brief Find a non-standard uniform by name.
  ///
  /// @param uniform_name The name of the uniform to find.
  /// @return Returns a handle to the requested uniform, -1 if not found.
  UniformHandle FindUniform(const char *uniform_name);

  /// @brief Raw call to set any uniform (with 1/2/3/4/16 components).
  ///
  /// More convenient variants below.
  ///
  /// @param uniform_loc Handle to the uniform that will be set.
  /// @param value The value to set the uniform to.
  /// @param num_components The number of components used by the uniform.
  void SetUniform(UniformHandle uniform_loc, const float *value,
                  size_t num_components);

  /// @brief Set an non-standard uniform to a vec2/3/4 value.
  ///
  /// Call this after Set() or FindUniform().
  ///
  /// @param uniform_loc Handle to the uniform that will be set.
  /// @param value The vector to set the uniform to.
  template <int N>
  void SetUniform(UniformHandle uniform_loc,
                  const mathfu::Vector<float, N> &value) {
    SetUniform(uniform_loc, &value[0], N);
  }

  /// @brief Convenience call that does a Lookup and a Set if found.
  ///
  /// Call this after Set().
  ///
  /// @param uniform_name The name of the uniform that will be set.
  /// @param value The vector to set the uniform to.
  /// @return Returns true if the uniform was found and set, false otherwise.
  template <int N>
  bool SetUniform(const char *uniform_name,
                  const mathfu::Vector<float, N> &value) {
    auto loc = FindUniform(uniform_name);
    if (!ValidUniformHandle(loc)) return false;
    SetUniform(loc, &value[0], N);
    return true;
  }

  /// @brief Set a non-standard uniform to a float value.
  ///
  /// Call this after Set().
  ///
  /// @param uniform_name The name of the uniform that will be set.
  /// @param value The float to set the uniform to.
  /// @return Returns true if the uniform was found and set, false otherwise.
  bool SetUniform(const char *uniform_name, float value) {
    auto loc = FindUniform(uniform_name);
    if (!ValidUniformHandle(loc)) return false;
    SetUniform(loc, &value, 1);
    return true;
  }

  /// @brief Set a non-standard uniform to a mat4 value.
  ///
  /// Call this after Set().
  ///
  /// @param uniform_name The name of the uniform that will be set.
  /// @param value The mat4 to set the uniform to.
  /// @return Returns true if the uniform was found and set, false otherwise.
  bool SetUniform(const char *uniform_name, const mathfu::mat4 &value) {
    auto loc = FindUniform(uniform_name);
    if (!ValidUniformHandle(loc)) return false;
    SetUniform(loc, &value[0], sizeof(value) / sizeof(float));
    return true;
  }

  void InitializeUniforms();

  ShaderHandle program() const { return program_; }

  bool HasDefine(const char *define) const {
    return enabled_defines_.find(define) != enabled_defines_.end();
  }

  bool IsDirty() const { return dirty_; }

  /// @brief Call to mark the shader as needing to be reloaded.
  ///
  /// Useful when you've changed the shader source and want to dynamically
  /// re-compile the shader.
  ///
  /// @note Be sure to call ReloadIfDirty() on your render thread before
  /// the shader is used. Otherwise an assert will be hit in Shader::Set().
  void MarkDirty() { dirty_ = true; }

  // For internal use.
  ShaderImpl *impl() { return impl_; }

  /// @brief Loads a .fplshader file from disk.
  /// Used by the more convenient AssetManager interface, but can also be
  /// used without it.
  static Shader *LoadFromShaderDef(const char *filename);

 private:
  friend class Renderer;
  friend class RendererBase;

  // Holds the source code of vertex shader and fragment shader.
  struct ShaderSourcePair {
    std::string vertex_shader;
    std::string fragment_shader;
  };

  // Used by constructor to init inner variables.
  void Init(ShaderHandle program, ShaderHandle vs, ShaderHandle ps,
            const std::vector<std::string> &defines, Renderer *renderer);

  /// @brief Clear the Shader and reset everything to null.
  void Clear();

  void Reset(ShaderHandle program, ShaderHandle vs, ShaderHandle ps);

  bool ReloadInternal();

  ShaderSourcePair *LoadSourceFile();

  // Backend-specific create and destroy calls. These just call new and delete
  // on the platform-specific impl structs.
  static ShaderImpl *CreateShaderImpl();
  static void DestroyShaderImpl(ShaderImpl *impl);

  ShaderImpl *impl_;

  ShaderHandle program_;
  ShaderHandle vs_;
  ShaderHandle ps_;

  UniformHandle uniform_model_view_projection_;
  UniformHandle uniform_model_;
  UniformHandle uniform_color_;
  UniformHandle uniform_light_pos_;
  UniformHandle uniform_camera_pos_;
  UniformHandle uniform_time_;
  UniformHandle uniform_bone_transforms_;

  Renderer *renderer_;

  // Defines that are set by default. In UpdateDefines(), these are modified
  // by the global defines to create `enabled_defines` below.
  std::vector<std::string> local_defines_;

  // Defines that are actually enabled for this shader.
  // The shader files are preprocessed with this list of defines.
  std::set<std::string> enabled_defines_;

  // If true, means this shader needs to be reloaded.
  bool dirty_;
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_SHADER_H
