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

#include "mathfu/glsl_mappings.h"

namespace fplbase {
class Renderer;

/// @file
/// @addtogroup fplbase_shader
/// @{

static const int kMaxTexturesPerShader = 8;
static const int kNumVec4sInAffineTransform = 3;

// These typedefs compatible with their OpenGL equivalents, but don't require
// this header to depend on OpenGL.
typedef unsigned int ShaderHandle;
typedef int UniformHandle;

/// @class Shader
/// @brief Represents a shader consisting of a vertex and pixel shader.
///
/// Represents a shader consisting of a vertex and pixel shader. Also stores
/// ids of standard uniforms. Use the Renderer class below to create these.
class Shader : public AsyncAsset {
 public:
  Shader(const char *filename, const std::vector<std::string> &defines,
         Renderer *renderer)
      : AsyncAsset(filename ? filename : "") {
    Init(0 /* program */, 0 /* vs */, 0 /* ps */, defines, renderer);
  }

  Shader(ShaderHandle program, ShaderHandle vs, ShaderHandle ps) {
    static const std::vector<std::string> empty_defines;
    Init(program, vs, ps, empty_defines, nullptr /* renderer */);
  }

  ~Shader();

  /// @brief Reloads this shader with given filename and defines.
  bool Reload(const char *filename, const std::vector<std::string> &defines);

  /// @brief Reloads the defines of this shader.
  ///
  /// @param defines_to_add the defines to be added into 'original_defines'.
  /// @param defines_to_omit the defines to be omit if it's existed in
  /// 'original_defines'.
  bool ReloadDefines(const std::vector<std::string> &defines_to_add,
                     const std::vector<std::string> &defines_to_omit);

  /// @brief Loads and unpacks the Mesh from `filename_` into `data_`.
  virtual void Load();

  /// @brief Creates a Texture from `data_`.
  virtual bool Finalize();

  /// @brief Whether this object loaded and finalized correctly. Call after
  /// Finalize has been called (by AssetManager::TryFinalize).
  bool IsValid() { return program_ != 0; }

  /// @brief Activate this shader for subsequent draw calls.
  ///
  /// Will make this shader active for any subsequent draw calls, and sets
  /// all standard uniforms (e.g. mvp matrix) based on current values in
  /// Renderer, if this shader refers to them.
  ///
  /// @param renderer The renderer that has the standard uniforms set.
  void Set(const Renderer &renderer) const;

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
    if (loc < 0) return false;
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
    if (loc < 0) return false;
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
    if (loc < 0) return false;
    SetUniform(loc, &value[0], sizeof(value) / sizeof(float));
    return true;
  }

  void InitializeUniforms();

  ShaderHandle program() const { return program_; }

  bool HasDefine(const char *define) const {
    return enabled_defines_.find(define) != enabled_defines_.end();
  }

  bool IsDirty() const { return dirty_; }
  void SetDirty() { dirty_ = true; }

  /// @brief Loads a .fplshader file from disk.
  /// Used by the more convenient AssetManager interface, but can also be
  /// used without it.
  static Shader *LoadFromShaderDef(const char *filename);

 private:
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

  ShaderHandle program_, vs_, ps_;

  UniformHandle uniform_model_view_projection_;
  UniformHandle uniform_model_;
  UniformHandle uniform_color_;
  UniformHandle uniform_light_pos_;
  UniformHandle uniform_camera_pos_;
  UniformHandle uniform_time_;
  UniformHandle uniform_bone_transforms_;

  Renderer *renderer_;
  // Original defines set by Init() or Reload().
  std::vector<std::string> original_defines_;
  // Defines that are actually enabled for this shader.
  std::set<std::string> enabled_defines_;
  // If true, means this shader needs to be reloaded.
  bool dirty_;
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_SHADER_H
