// Copyright 2017 Google Inc. All rights reserved.
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

static bool ParseShaderPipelineArgs(int argc, char** argv,
                                    fplbase::ShaderPipelineArgs* args) {
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

      // -d switch
    } else if (arg == "-d" || arg == "--defines") {
      if (i < argc - 2) {
        ++i;
        args->defines.insert(args->defines.end(), argv[i]);
      } else {
        valid_args = false;
      }

      // -i switch
    } else if (arg == "-i" || arg == "--include_dir") {
      if (i < argc - 2) {
        ++i;
        args->include_dirs.insert(args->include_dirs.end(), argv[i]);
      } else {
        valid_args = false;
      }

      // --version switch
    } else if (arg == "--version") {
      if (i < argc - 2) {
        ++i;
        args->version = argv[i];
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
  // Null-terminate the vector so the resulting array is null-termintated.
  args->defines.insert(args->defines.end(), nullptr);

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
        "  -fs, --fragment-shader FRAGMENT_SHADER\n"
        "  -i,  --include_dir DIRECTORY\n"
        "  -d,  --defines DEFINITION\n"
        "       --version VERSION\n");
  }

  return valid_args;
}

int main(int argc, char** argv) {
  // Parse the command line arguments.
  fplbase::ShaderPipelineArgs args;
  if (!ParseShaderPipelineArgs(argc, argv, &args)) {
    return 1;
  }
  return fplbase::RunShaderPipeline(args);
}

