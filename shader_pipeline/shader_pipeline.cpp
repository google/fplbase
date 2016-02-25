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

#include <stdio.h>
#include <string.h>

#include "common_generated.h"
#include "fplbase/preprocessor.h"
#include "fplbase/utilities.h"
#include "shader_generated.h"

struct ShaderPipelineArgs {
  std::string vertex_shader;    /// The vertex shader source file.
  std::string fragment_shader;  /// The fragment shader source file.
  std::string output_file;      /// The output fplshader file.
};

static bool ParseShaderPipelineArgs(int argc, char** argv,
                                    ShaderPipelineArgs* args) {
  bool valid_args = true;

  // Last parameter is used as the output file.
  if (argc > 1) {
    args->output_file = std::string(argv[argc - 1]);
  } else {
    valid_args = false;
  }

  // Parse switches.
  for (int i = 1; i < argc - 1; ++i) {
    const std::string arg = argv[i];

    // -vs switch
    if (arg == "-vs" || arg == "--vertex-shader") {
      if (i < argc - 2) {
        ++i;
        args->vertex_shader = std::string(argv[i]);
      } else {
        valid_args = false;
      }

      // -fs switch
    } else if (arg == "-fs" || arg == "--fragment-shader") {
      if (i < argc - 2) {
        ++i;
        args->fragment_shader = std::string(argv[i]);
      } else {
        valid_args = false;
      }

      // all other (non-empty) arguments
    } else if (arg != "") {
      printf("Unknown parameter: %s\n", arg.c_str());
      valid_args = false;
    }

    if (!valid_args) break;
  }

  if (args->vertex_shader.empty() || args->fragment_shader.empty()) {
    valid_args = false;
  }

  // Print usage.
  if (!valid_args) {
    printf(
        "Usage: shader_pipeline -vs VERTEX_SHADER -fs FRAGMENT_SHADER\n"
        "                       OUTPUT_FILE\n"
        "\n"
        "Pipeline to generate fplshader files from individual vertex and \n"
        "fragment shader files.\n"
        "\n"
        "Options:\n"
        "  -vs, --vertex-shader VERTEX_SHADER\n"
        "  -fs, --fragment-shader FRAGMENT_SHADER\n");
  }

  return valid_args;
}

bool WriteFlatBufferBuilder(const flatbuffers::FlatBufferBuilder& fbb,
                            const std::string& filename) {
  FILE* file = fopen(filename.c_str(), "wb");
  if (file) {
    fwrite(fbb.GetBufferPointer(), 1, fbb.GetSize(), file);
    fclose(file);
    return true;
  }
  return false;
}

int main(int argc, char** argv) {
  // Parse the command line arguments.
  ShaderPipelineArgs args;
  if (!ParseShaderPipelineArgs(argc, argv, &args)) {
    return 1;
  }

  // Read
  std::string vsh;
  std::string fsh;
  std::string error_message;
  if (!fplbase::LoadFileWithDirectives(args.vertex_shader.c_str(), &vsh,
                                       &error_message)) {
    printf("Unable to load file: %s \n%s\n", args.vertex_shader.c_str(),
           error_message.c_str());
    return 1;
  }

  if (!fplbase::LoadFileWithDirectives(args.fragment_shader.c_str(), &fsh,
                                       &error_message)) {
    printf("Unable to load file: %s \n%s\n", args.vertex_shader.c_str(),
           error_message.c_str());
    return 1;
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
