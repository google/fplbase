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

#include <iterator>
#include <vector>

#include "fplbase/preprocessor.h"
#include "fplbase/renderer.h"
#include "fplbase/shader.h"
#include "shader_generated.h"

#ifdef _WIN32
#define snprintf(buffer, count, format, ...) \
  _snprintf_s(buffer, count, count, format, __VA_ARGS__)
#endif  // _WIN32

namespace fplbase {

Shader::~Shader() { Clear(); }

void Shader::Init(ShaderHandle program, ShaderHandle vs, ShaderHandle ps,
                  const std::vector<std::string> &defines, Renderer *renderer) {
  program_ = program;
  vs_ = vs;
  ps_ = ps;
  uniform_model_view_projection_ = -1;
  uniform_model_ = -1;
  uniform_color_ = -1;
  uniform_light_pos_ = -1;
  uniform_camera_pos_ = -1;
  uniform_time_ = -1;
  uniform_bone_transforms_ = -1;
  renderer_ = renderer;

  original_defines_ = defines;
  // All defines are enabled by default.
  std::copy(defines.begin(), defines.end(),
            std::inserter(enabled_defines_, enabled_defines_.begin()));
  dirty_ = false;
}

bool Shader::Reload(const char *basename,
                    const std::vector<std::string> &defines) {
  filename_ = basename;
  original_defines_ = defines;
  // All defines are enabled by default.
  enabled_defines_.clear();
  std::copy(defines.begin(), defines.end(),
            std::inserter(enabled_defines_, enabled_defines_.begin()));

  return ReloadInternal();
}

bool Shader::ReloadDefines(const std::vector<std::string> &defines_to_add,
                           const std::vector<std::string> &defines_to_omit) {
  enabled_defines_.clear();
  std::copy(original_defines_.begin(), original_defines_.end(),
            std::inserter(enabled_defines_, enabled_defines_.begin()));
  std::copy(defines_to_add.begin(), defines_to_add.end(),
            std::inserter(enabled_defines_, enabled_defines_.begin()));
  for (auto iter = defines_to_omit.begin(); iter != defines_to_omit.end();
       ++iter) {
    enabled_defines_.erase(*iter);
  }
  dirty_ = false;

  return ReloadInternal();
}

bool Shader::ReloadInternal() {
  ShaderSourcePair *source_pair = LoadSourceFile();
  if (source_pair != nullptr) {
    auto sh = renderer_->RecompileShader(source_pair->vertex_shader.c_str(),
                                         source_pair->fragment_shader.c_str(),
                                         this);
    delete source_pair;
    return sh != nullptr;
  } else {
    delete source_pair;
    return false;
  }
}

void Shader::Reset(ShaderHandle program, ShaderHandle vs, ShaderHandle ps) {
  Clear();
  program_ = program;
  vs_ = vs;
  ps_ = ps;
}

void Shader::Load() {
  ShaderSourcePair *source_pair = LoadSourceFile();
  if (source_pair != nullptr) {
    data_ = reinterpret_cast<uint8_t *>(source_pair);
  }
}

bool Shader::Finalize() {
  if (data_ == nullptr) {
    return false;
  }
  const ShaderSourcePair *source_pair =
      reinterpret_cast<const ShaderSourcePair *>(data_);
  // This funciton will call Shader::Reset() -> Shader::Clear() to clear
  // 'data_'.
  auto sh = renderer_->RecompileShader(source_pair->vertex_shader.c_str(),
                                       source_pair->fragment_shader.c_str(),
                                       this);
  CallFinalizeCallback();
  return sh != nullptr;
}

void Shader::Clear() {
  if (vs_) {
    GL_CALL(glDeleteShader(vs_));
    vs_ = 0;
  }
  if (ps_) {
    GL_CALL(glDeleteShader(ps_));
    ps_ = 0;
  }
  if (program_) {
    GL_CALL(glDeleteProgram(program_));
    program_ = 0;
  }
  if (data_ != nullptr) {
    delete reinterpret_cast<const ShaderSourcePair *>(data_);
    data_ = nullptr;
  }
}

Shader::ShaderSourcePair *Shader::LoadSourceFile() {
  std::string filename = std::string(filename_) + ".glslv";
  std::string error_message;

  ShaderSourcePair *source_pair = new ShaderSourcePair();
  if (LoadFileWithDirectives(filename.c_str(), &source_pair->vertex_shader,
                             enabled_defines_, &error_message)) {
    filename = std::string(filename_) + ".glslf";
    if (LoadFileWithDirectives(filename.c_str(), &source_pair->fragment_shader,
                               enabled_defines_, &error_message)) {
      return source_pair;
    }
  }
  LogError(kError, "%s", error_message.c_str());
  renderer_->set_last_error(error_message.c_str());
  delete source_pair;
  return nullptr;
}

Shader *Shader::LoadFromShaderDef(const char *filename) {
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(shaderdef::VerifyShaderBuffer(verifier));
    auto shaderdef = shaderdef::GetShader(flatbuf.c_str());
    auto shader =
        Renderer::Get()->CompileAndLinkShader(
          shaderdef->vertex_shader()->c_str(),
          shaderdef->fragment_shader()->c_str());
    if (!shader) {
      LogError(kError, "Shader Error: ");
      if (shaderdef->original_sources()) {
        for (int i = 0;
             i < static_cast<int>(shaderdef->original_sources()->size()); ++i) {
          const auto &source = shaderdef->original_sources()->Get(i);
          LogError(kError, "%s", source->c_str());
        }
      }
      LogError(kError, "VS:  -----------------------------------");
      LogError(kError, "%s", shaderdef->vertex_shader()->c_str());
      LogError(kError, "PS:  -----------------------------------");
      LogError(kError, "%s", shaderdef->fragment_shader()->c_str());
      LogError(kError, "----------------------------------------");
      LogError(kError, "%s", Renderer::Get()->last_error().c_str());
    }
    return shader;
  }
  LogError(kError, "Can\'t load shader file: %s", filename);
  Renderer::Get()->set_last_error(std::string("Couldn\'t load: ") + filename);
  return nullptr;
}

UniformHandle Shader::FindUniform(const char *uniform_name) {
  GL_CALL(glUseProgram(program_));
  return glGetUniformLocation(program_, uniform_name);
}

void Shader::SetUniform(GLint uniform_loc, const float *value,
                        size_t num_components) {
  // clang-format off
  switch (num_components) {
    case 1: GL_CALL(glUniform1f(uniform_loc, *value)); break;
    case 2: GL_CALL(glUniform2fv(uniform_loc, 1, value)); break;
    case 3: GL_CALL(glUniform3fv(uniform_loc, 1, value)); break;
    case 4: GL_CALL(glUniform4fv(uniform_loc, 1, value)); break;
    case 16: GL_CALL(glUniformMatrix4fv(uniform_loc, 1, false, value)); break;
    default: assert(0); break;
  }
  // clang-format on
}

void Shader::InitializeUniforms() {
  // Look up variables that are standard, but still optionally present in a
  // shader.
  uniform_model_view_projection_ =
      glGetUniformLocation(program_, "model_view_projection");
  uniform_model_ = glGetUniformLocation(program_, "model");

  uniform_color_ = glGetUniformLocation(program_, "color");

  uniform_light_pos_ = glGetUniformLocation(program_, "light_pos");
  uniform_camera_pos_ = glGetUniformLocation(program_, "camera_pos");

  uniform_time_ = glGetUniformLocation(program_, "time");

  // An array of vec4's. Three vec4's compose one affine transform.
  // The i'th affine transform is the translation, rotation, and
  // orientation of the i'th bone.
  uniform_bone_transforms_ = glGetUniformLocation(program_, "bone_transforms");

  // Set up the uniforms the shader uses for texture access.
  char texture_unit_name[] = "texture_unit_#####";
  for (int i = 0; i < kMaxTexturesPerShader; i++) {
    snprintf(texture_unit_name, sizeof(texture_unit_name), "texture_unit_%d",
             i);
    auto loc = glGetUniformLocation(program_, texture_unit_name);
    if (loc >= 0) GL_CALL(glUniform1i(loc, i));
  }
}

void Shader::Set(const Renderer &renderer) const {
  const int kNumVec4InBoneTransform = 3;

  GL_CALL(glUseProgram(program_));

  if (uniform_model_view_projection_ >= 0)
    GL_CALL(glUniformMatrix4fv(uniform_model_view_projection_, 1, false,
                               &renderer.model_view_projection()[0]));
  if (uniform_model_ >= 0)
    GL_CALL(glUniformMatrix4fv(uniform_model_, 1, false, &renderer.model()[0]));
  if (uniform_color_ >= 0)
    GL_CALL(glUniform4fv(uniform_color_, 1, &renderer.color()[0]));
  if (uniform_light_pos_ >= 0)
    GL_CALL(glUniform3fv(uniform_light_pos_, 1, &renderer.light_pos()[0]));
  if (uniform_camera_pos_ >= 0)
    GL_CALL(glUniform3fv(uniform_camera_pos_, 1, &renderer.camera_pos()[0]));
  if (uniform_time_ >= 0)
    GL_CALL(glUniform1f(uniform_time_, static_cast<float>(renderer.time())));
  if (uniform_bone_transforms_ >= 0 && renderer.num_bones() > 0) {
    assert(renderer.bone_transforms() != nullptr);

    const mathfu::AffineTransform *bone_transforms = renderer.bone_transforms();

    GL_CALL(glUniform4fv(uniform_bone_transforms_,
                         renderer.num_bones() * kNumVec4InBoneTransform,
                         &bone_transforms[0][0]));
  }
}

}  // namespace fplbase

