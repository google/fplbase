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

#include "fplbase/config.h" // Must come first.

#include "mathfu/glsl_mappings.h"

namespace fpl {

using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec3i;
using mathfu::vec4;
using mathfu::vec4i;
using mathfu::mat4;

class Renderer;

static const int kMaxTexturesPerShader = 8;

// These typedefs compatible with their OpenGL equivalents, but don't require
// this header to depend on OpenGL.
typedef unsigned int ShaderHandle;
typedef int UniformHandle;

// Represents a shader consisting of a vertex and pixel shader. Also stores
// ids of standard uniforms. Use the Renderer class below to create these.
class Shader {
 public:
  Shader(ShaderHandle program, ShaderHandle vs, ShaderHandle ps)
      : program_(program),
        vs_(vs),
        ps_(ps),
        uniform_model_view_projection_(-1),
        uniform_model_(-1),
        uniform_color_(-1),
        uniform_light_pos_(-1),
        uniform_camera_pos_(-1),
        uniform_time_(-1),
        uniform_bone_tranforms_(-1) {}

  ~Shader();

  // Will make this shader active for any subsequent draw calls, and sets
  // all standard uniforms (e.g. mvp matrix) based on current values in
  // Renderer, if this shader refers to them.
  void Set(const Renderer &renderer) const;

  // Find a non-standard uniform by name, -1 means not found.
  UniformHandle FindUniform(const char *uniform_name);

  // Raw call to set any uniform (with 1/2/3/4/16 components).
  // More convenient variants below.
  void SetUniform(UniformHandle uniform_loc, const float *value,
                  size_t num_components);

  // Set an non-standard uniform to a vec2/3/4 value.
  // Call this after Set() or FindUniform().
  template <int N>
  void SetUniform(UniformHandle uniform_loc,
                  const mathfu::Vector<float, N> &value) {
    SetUniform(uniform_loc, &value[0], N);
  }

  // Convenience call that does a Lookup and a Set if found.
  // Call this after Set().
  template <int N>
  bool SetUniform(const char *uniform_name,
                  const mathfu::Vector<float, N> &value) {
    auto loc = FindUniform(uniform_name);
    if (loc < 0) return false;
    SetUniform(loc, &value[0], N);
    return true;
  }

  bool SetUniform(const char *uniform_name, float value) {
    auto loc = FindUniform(uniform_name);
    if (loc < 0) return false;
    SetUniform(loc, &value, 1);
    return true;
  }

  bool SetUniform(const char *uniform_name, const mathfu::mat4 &value) {
    auto loc = FindUniform(uniform_name);
    if (loc < 0) return false;
    SetUniform(loc, &value[0], sizeof(value) / sizeof(float));
    return true;
  }

  void InitializeUniforms();

  ShaderHandle GetProgram() const { return program_; }

 private:
  ShaderHandle program_, vs_, ps_;

  UniformHandle uniform_model_view_projection_;
  UniformHandle uniform_model_;
  UniformHandle uniform_color_;
  UniformHandle uniform_light_pos_;
  UniformHandle uniform_camera_pos_;
  UniformHandle uniform_time_;
  UniformHandle uniform_bone_tranforms_;
};

}  // namespace fpl

#endif  // FPLBASE_SHADER_H
