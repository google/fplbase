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

#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/preprocessor.h"
#include "fplbase/renderer.h"
#include "fplbase/shader.h"
#include "shader_generated.h"

#ifdef _WIN32
#define snprintf(buffer, count, format, ...) \
  _snprintf_s(buffer, count, count, format, __VA_ARGS__)
#endif  // _WIN32

namespace fplbase {

//static
ShaderImpl *Shader::CreateShaderImpl() { return nullptr; }

// static
void Shader::DestroyShaderImpl(ShaderImpl *impl) { (void)impl; }

void Shader::Clear() {
  if (ValidShaderHandle(vs_)) {
    GL_CALL(glDeleteShader(GlShaderHandle(vs_)));
    vs_ = InvalidShaderHandle();
  }
  if (ValidShaderHandle(ps_)) {
    GL_CALL(glDeleteShader(GlShaderHandle(ps_)));
    ps_ = InvalidShaderHandle();
  }
  if (ValidShaderHandle(program_)) {
    GL_CALL(glDeleteProgram(GlShaderHandle(program_)));
    program_ = InvalidShaderHandle();
  }
  if (data_ != nullptr) {
    delete reinterpret_cast<const ShaderSourcePair *>(data_);
    data_ = nullptr;
  }
}

UniformHandle Shader::FindUniform(const char *uniform_name) {
  auto program = GlShaderHandle(program_);
  GL_CALL(glUseProgram(program));
  return UniformHandleFromGl(glGetUniformLocation(program, uniform_name));
}

void Shader::SetUniform(UniformHandle uniform_loc, const float *value,
                        size_t num_components) {
  // clang-format off
  auto uniform_loc_gl = GlUniformHandle(uniform_loc);
  switch (num_components) {
    case 1: GL_CALL(glUniform1f(uniform_loc_gl, *value)); break;
    case 2: GL_CALL(glUniform2fv(uniform_loc_gl, 1, value)); break;
    case 3: GL_CALL(glUniform3fv(uniform_loc_gl, 1, value)); break;
    case 4: GL_CALL(glUniform4fv(uniform_loc_gl, 1, value)); break;
    case 16: GL_CALL(glUniformMatrix4fv(uniform_loc_gl, 1, false, value)); break;
    default: assert(0); break;
  }
  // clang-format on
}

void Shader::InitializeUniforms() {
  auto program = GlShaderHandle(program_);

  // Look up variables that are standard, but still optionally present in a
  // shader.
  uniform_model_view_projection_ = UniformHandleFromGl(
      glGetUniformLocation(program, "model_view_projection"));
  uniform_model_ = UniformHandleFromGl(glGetUniformLocation(program, "model"));

  uniform_color_ = UniformHandleFromGl(glGetUniformLocation(program, "color"));

  uniform_light_pos_ =
      UniformHandleFromGl(glGetUniformLocation(program, "light_pos"));
  uniform_camera_pos_ =
      UniformHandleFromGl(glGetUniformLocation(program, "camera_pos"));

  uniform_time_ = UniformHandleFromGl(glGetUniformLocation(program, "time"));

  // An array of vec4's. Three vec4's compose one affine transform.
  // The i'th affine transform is the translation, rotation, and
  // orientation of the i'th bone.
  uniform_bone_transforms_ =
      UniformHandleFromGl(glGetUniformLocation(program, "bone_transforms"));

  // Set up the uniforms the shader uses for texture access.
  char texture_unit_name[] = "texture_unit_#####";
  for (int i = 0; i < kMaxTexturesPerShader; i++) {
    snprintf(texture_unit_name, sizeof(texture_unit_name), "texture_unit_%d",
             i);
    auto loc = glGetUniformLocation(program, texture_unit_name);
    if (loc >= 0) GL_CALL(glUniform1i(loc, i));
  }
}

}  // namespace fplbase
