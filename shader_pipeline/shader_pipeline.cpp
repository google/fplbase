// Copyright 2015 Google Inc. All rights reserved.
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

#include "shader_pipeline.h"

#include <stdio.h>
#include <string.h>
#include <vector>

#include "common_generated.h"
#include "fplbase/preprocessor.h"
#include "fplbase/utilities.h"
#include "shader_generated.h"

namespace fplbase {

static bool WriteFlatBufferBuilder(const flatbuffers::FlatBufferBuilder& fbb,
                                   const std::string& filename) {
  FILE* file = fopen(filename.c_str(), "wb");
  if (file) {
    fwrite(fbb.GetBufferPointer(), 1, fbb.GetSize(), file);
    fclose(file);
    return true;
  }
  return false;
}

static std::string ApplyVersion(const std::string& source,
                                const std::string& version) {
  std::string versioned_source;
  fplbase::SetShaderVersion(source.c_str(), version.c_str(), &versioned_source);
  return versioned_source;
}

int RunShaderPipeline(const ShaderPipelineArgs& args) {
  // Store the current load file function which we'll restore later.
  fplbase::LoadFileFunction load_fn = fplbase::SetLoadFileFunction(nullptr);

  // Provide a custom loader that will search include paths for files.  This
  // loader will use the previous file loader for the actual loading operation.
  fplbase::SetLoadFileFunction([&load_fn, &args](const char* filename,
                                                 std::string* dest) {
    // First try to load the file at the given path.
    if (load_fn(filename, dest)) {
      return true;
    }

    // Otherwise, try to load from each of the include dirs (but only for
    // #included files).
    if (args.vertex_shader.compare(filename) != 0 &&
        args.fragment_shader.compare(filename) != 0) {
      std::string path;
      for (const auto& dir : args.include_dirs) {
        path = dir;
        if (path.back() != '/' && path.back() != '\\') {
          path += '/';
        }
        path += filename;
        if (load_fn(path.c_str(), dest)) {
          return true;
        }
      }
    }

    return false;
  });

  // Read
  int status = 0;
  std::string vsh;
  std::string fsh;
  std::string error_message;
  const char* const* defines = args.defines.data();
  if (!fplbase::LoadFileWithDirectives(args.vertex_shader.c_str(), &vsh,
                                       defines, &error_message)) {
    printf("Unable to load file: %s \n%s\n", args.vertex_shader.c_str(),
           error_message.c_str());
    status = 1;
  }

  if (!status &&
      !fplbase::LoadFileWithDirectives(args.fragment_shader.c_str(), &fsh,
                                       defines, &error_message)) {
    printf("Unable to load file: %s \n%s\n", args.vertex_shader.c_str(),
           error_message.c_str());
    status = 1;
  }

  // Restore the previous load file function.
  fplbase::SetLoadFileFunction(load_fn);

  if (status != 0) {
    return status;
  }

  if (!args.version.empty()) {
    vsh = ApplyVersion(vsh, args.version);
    fsh = ApplyVersion(fsh, args.version);
  }

  // Create the FlatBuffer for the Shader.
  flatbuffers::FlatBufferBuilder fbb;
  auto vsh_fb = fbb.CreateString(vsh);
  auto fsh_fb = fbb.CreateString(fsh);

  std::vector<flatbuffers::Offset<flatbuffers::String>> sources_vector;
  sources_vector.push_back(fbb.CreateString(args.vertex_shader));
  sources_vector.push_back(fbb.CreateString(args.fragment_shader));
  auto sources_fb = fbb.CreateVector(sources_vector);

  auto shader_fb = shaderdef::CreateShader(fbb, vsh_fb, fsh_fb, sources_fb);
  shaderdef::FinishShaderBuffer(fbb, shader_fb);

  // Save the Shader FlatBuffer to disk.
  if (!WriteFlatBufferBuilder(fbb, args.output_file)) {
    printf("Could not open %s for writing.\n", args.output_file.c_str());
    return 1;
  }

  // Success.
  return 0;
}

}  // namespace fplbase
