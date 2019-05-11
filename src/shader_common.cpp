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

#include "fplbase/preprocessor.h"
#include "fplbase/renderer.h"
#include "fplbase/shader.h"
#include "shader_generated.h"

namespace fplbase {

Shader::~Shader() {
  Clear();
  DestroyShaderImpl(impl_);
}

void Shader::Init(ShaderHandle program, ShaderHandle vs, ShaderHandle ps,
                  const std::vector<std::string> &local_defines,
                  Renderer *renderer) {
  program_ = program;
  vs_ = vs;
  ps_ = ps;
  const UniformHandle invalid = InvalidUniformHandle();
  uniform_model_view_projection_ = invalid;
  uniform_model_ = invalid;
  uniform_color_ = invalid;
  uniform_light_pos_ = invalid;
  uniform_camera_pos_ = invalid;
  uniform_time_ = invalid;
  uniform_bone_transforms_ = invalid;
  renderer_ = renderer;

  // All local defines are enabled by default.
  // The local defines may be modified by global defines in UpdateDefines().
  local_defines_ = local_defines;
  std::copy(local_defines.begin(), local_defines.end(),
            std::inserter(enabled_defines_, enabled_defines_.begin()));

  // If the shader has already been loaded, it's not dirty.
  dirty_ = !ValidShaderHandle(vs);
}

// Returns the set of `local_defines` union `global_defines_to_add` less
// `global_defines_to_omit`.
static std::set<std::string> CalculateDefines(
    const std::vector<std::string> &local_defines,
    const std::vector<std::string> &global_defines_to_add,
    const std::vector<std::string> &global_defines_to_omit) {
  std::set<std::string> defines;
  std::copy(local_defines.begin(), local_defines.end(),
            std::inserter(defines, defines.begin()));
  std::copy(global_defines_to_add.begin(), global_defines_to_add.end(),
            std::inserter(defines, defines.begin()));
  for (auto iter = global_defines_to_omit.begin();
       iter != global_defines_to_omit.end(); ++iter) {
    defines.erase(*iter);
  }
  return defines;
}

void Shader::UpdateGlobalDefines(
    const std::vector<std::string> &global_defines_to_add,
    const std::vector<std::string> &global_defines_to_omit) {
  // Do nothing if new defines are the same as the existing defines.
  std::set<std::string> defines = CalculateDefines(
      local_defines_, global_defines_to_add, global_defines_to_omit);
  if (defines == enabled_defines_) return;

  // If the new defines differ from the current ones, mark as dirty.
  enabled_defines_ = std::move(defines);
  dirty_ = true;
}

bool Shader::ReloadIfDirty() {
  if (!dirty_) return true;
  dirty_ = false;
  return ReloadInternal();
}

bool Shader::ReloadInternal() {
  ShaderSourcePair *source_pair = LoadSourceFile();
  dirty_ = false;
  if (source_pair == nullptr) return false;

  auto sh =
      renderer_->RecompileShader(source_pair->vertex_shader.c_str(),
                                 source_pair->fragment_shader.c_str(), this);
  delete source_pair;
  return sh != nullptr;
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
  auto sh =
      renderer_->RecompileShader(source_pair->vertex_shader.c_str(),
                                 source_pair->fragment_shader.c_str(), this);

  if (sh == nullptr) {
    LogError(kError, "Shader compilation error:\n%s",
             renderer_->last_error().c_str());
  }

  CallFinalizeCallback();

  dirty_ = false;
  return sh != nullptr;
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

static void BreakAndLogError(const char *cstr) {
  const size_t kMaxLength = 1024;  // Default Android log limit.
  std::string str = cstr;
  while (str.length() > kMaxLength) {
    const size_t last_newline = str.find_last_of('\n', kMaxLength);
    const size_t truncate_pos =
        last_newline == std::string::npos ? kMaxLength : last_newline;
    LogError(kError, str.substr(0, truncate_pos).c_str());
    str.erase(0, truncate_pos);
  }
  if (!str.empty()) {
    LogError(kError, str.c_str());
  }
}

Shader *Shader::LoadFromShaderDef(const char *filename) {
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(shaderdef::VerifyShaderBuffer(verifier));
    auto shaderdef = shaderdef::GetShader(flatbuf.c_str());
    auto shader = RendererBase::Get()->CompileAndLinkShader(
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
      BreakAndLogError(shaderdef->vertex_shader()->c_str());
      LogError(kError, "PS:  -----------------------------------");
      BreakAndLogError(shaderdef->fragment_shader()->c_str());
      LogError(kError, "----------------------------------------");
      BreakAndLogError(RendererBase::Get()->last_error().c_str());
    } else {
      shader->set_filename(filename);
    }
    return shader;
  }
  LogError(kError, "Can\'t load shader file: %s", filename);
  RendererBase::Get()->set_last_error(std::string("Couldn\'t load: ") +
                                      filename);
  return nullptr;
}

}  // namespace fplbase
