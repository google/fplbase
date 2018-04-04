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

#include "mesh_pipeline.h"

using fplutil::kLogError;
using fplutil::kLogImportant;
using fplutil::kLogInfo;
using fplutil::kLogVerbose;
using fplutil::kLogWarning;

static fplbase::VertexAttributeBitmask ParseVertexAttribute(char c) {
  for (int i = 0; i < fplbase::kVertexAttribute_Count; ++i) {
    if (c == fplbase::kVertexAttributeShortNames[i][0]) return 1 << i;
  }
  return 0;
}

static fplbase::VertexAttributeBitmask ParseVertexAttributes(const char* s) {
  fplbase::VertexAttributeBitmask vertex_attributes = 0;
  for (const char* p = s; *p != '\0'; ++p) {
    const fplbase::VertexAttributeBitmask bit = ParseVertexAttribute(*p);
    if (bit == 0) return 0;
    vertex_attributes |= bit;
  }
  return vertex_attributes;
}

static matdef::TextureFormat ParseTextureFormat(const char* s) {
  return static_cast<matdef::TextureFormat>(
      fplutil::IndexOfName(s, matdef::EnumNamesTextureFormat()));
}

static matdef::BlendMode ParseBlendMode(const char* s) {
  return static_cast<matdef::BlendMode>(
      fplutil::IndexOfName(s, matdef::EnumNamesBlendMode()));
}

static bool ParseTextureFormats(
    const std::string& arg, fplutil::Logger& log,
    std::vector<matdef::TextureFormat>* texture_formats) {
  // No texture formats specified is valid. Always use `AUTO`.
  if (arg.size() == 0) return true;

  // Loop through the comma-delimited string of texture formats.
  size_t format_start = 0;
  for (;;) {
    // Get substring with the name of one texture format.
    const size_t comma = arg.find_first_of(',', format_start);
    const std::string s = arg.substr(format_start, comma);

    // Parse the format. If it is invalid, log an error and exit.
    const matdef::TextureFormat format = ParseTextureFormat(s.c_str());
    if (format < 0) {
      log.Log(kLogError, "Invalid texture format `%s`\n", s.c_str());
      return false;
    }
    texture_formats->push_back(format);

    // Break if on last texture format. Otherwise, advance to next argument.
    if (comma == std::string::npos) return true;
    format_start = comma + 1;
  }
}

static bool TextureFormatHasAlpha(matdef::TextureFormat format) {
  return format == matdef::TextureFormat_F_8888;
}

static matdef::BlendMode DefaultBlendMode(
    const std::vector<matdef::TextureFormat>& texture_formats) {
  return texture_formats.size() > 0 && TextureFormatHasAlpha(texture_formats[0])
             ? matdef::BlendMode_ALPHA
             : matdef::BlendMode_OFF;
}

static bool Vector3FromArgs(const char* const* args, FbxVector4* out) {
  const size_t kValueCount = 3;
  double values[kValueCount];
  for (size_t i = 0; i != kValueCount; ++i) {
    const char* const arg = args[i];
    char* arg_end;
    values[i] = strtod(arg, &arg_end);
    if (arg_end == arg) {
      return false;
    }
  }
  *out = FbxVector4(values[0], values[1], values[2]);
  return true;
}

static bool ParseMeshPipelineArgs(int argc, char** argv, fplutil::Logger& log,
                                  fplbase::MeshPipelineArgs* args) {
  bool valid_args = true;

  // Last parameter is used as file name.
  if (argc > 1) {
    args->fbx_file = std::string(argv[argc - 1]);
  }

  // Ensure file name is valid.
  const bool valid_fbx_file =
      args->fbx_file.length() > 0 && args->fbx_file[0] != '-';
  if (!valid_fbx_file) {
    valid_args = false;
  }

  FbxVector4 bake_translation(0.0, 0.0, 0.0);
  FbxVector4 bake_rotation(0.0, 0.0, 0.0);
  FbxVector4 bake_scale(1.0, 1.0, 1.0);

  // Parse switches.
  const char* const* arg_end = argv + argc;
  for (int i = 1; i < argc - 1; ++i) {
    const std::string arg = argv[i];
    const char* const* arg_it = argv + i + 1;

    // -v switch
    if (arg == "-v" || arg == "--verbose") {
      args->log_level = kLogVerbose;

      // -d switch
    } else if (arg == "-d" || arg == "--details") {
      args->log_level = kLogImportant;

      // -i switch
    } else if (arg == "-i" || arg == "--info") {
      args->log_level = kLogInfo;

      // -b switch
    } else if (arg == "-b" || arg == "--base-dir") {
      if (i + 1 < argc - 1) {
        args->asset_base_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

      // -r switch
    } else if (arg == "-r" || arg == "--relative-dir") {
      if (i + 1 < argc - 1) {
        args->asset_rel_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-e" || arg == "--texture-extension") {
      if (i + 1 < argc - 1) {
        args->texture_extension = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

      // -h switch
    } else if (arg == "-h" || arg == "--hierarchy") {
      // This switch has been deprecated.

      // -c switch
    } else if (arg == "-c" || arg == "--center") {
      args->recenter = true;

      // -l switch
    } else if (arg == "-l" || arg == "--non-interleaved") {
      args->interleaved = false;

    } else if (arg == "--force-32-bit-indices") {
      args->force32 = true;

    } else if (arg == "--no-textures") {
      args->gather_textures = false;

    } else if (arg == "--embed-materials") {
      args->embed_materials = true;

      // -f switch
    } else if (arg == "-f" || arg == "--texture-formats") {
      if (i + 1 < argc - 1) {
        valid_args = ParseTextureFormats(std::string(argv[i + 1]), log,
                                         &args->texture_formats);
        if (!valid_args) {
          log.Log(kLogError, "Unknown texture format: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

      // -m switch
    } else if (arg == "-m" || arg == "--blend-mode") {
      if (i + 1 < argc - 1) {
        args->blend_mode = ParseBlendMode(argv[i + 1]);
        valid_args = args->blend_mode >= 0;
        if (!valid_args) {
          log.Log(kLogError, "Unknown blend mode: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-a" || arg == "--axes") {
      if (i + 1 < argc - 1) {
        args->axis_system = fplutil::AxisSystemFromName(argv[i + 1]);
        valid_args = args->axis_system >= 0;
        if (!valid_args) {
          log.Log(kLogError, "Unknown coordinate system: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-u" || arg == "--unit") {
      if (i + 1 < argc - 1) {
        args->distance_unit_scale = fplutil::DistanceUnitFromName(argv[i + 1]);
        valid_args = args->distance_unit_scale > 0.0f;
        if (!valid_args) {
          log.Log(kLogError, "Unknown distance unit: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

    } else if (arg == "-bt" || arg == "--bake-translation") {
      if (arg_it + 3 > arg_end || !Vector3FromArgs(arg_it, &bake_translation)) {
        valid_args = false;
        log.Log(kLogError, "Invalid --bake-translation %s %s %s\n\n",
                arg_it[0], arg_it[1], arg_it[2]);
      }
      i += 3;

    } else if (arg == "-br" || arg == "--bake-rotation") {
      if (arg_it + 3 > arg_end || !Vector3FromArgs(arg_it, &bake_rotation)) {
        valid_args = false;
        log.Log(kLogError, "Invalid --bake-rotation %s %s %s\n\n",
                arg_it[0], arg_it[1], arg_it[2]);
      }
      i += 3;

    } else if (arg == "-bs" || arg == "--bake-scale") {
      if (arg_it + 3 > arg_end || !Vector3FromArgs(arg_it, &bake_scale)) {
        valid_args = false;
        log.Log(kLogError, "Invalid --bake-scale %s %s %s\n\n",
                arg_it[0], arg_it[1], arg_it[2]);
      }
      i += 3;

    } else if (arg == "--attrib" || arg == "--vertex-attributes") {
      if (i + 1 < argc - 1) {
        args->vertex_attributes = ParseVertexAttributes(argv[i + 1]);
        valid_args = args->vertex_attributes != 0;
        if (!valid_args) {
          log.Log(kLogError, "Unknown vertex attributes: %s\n\n", argv[i + 1]);
        }
        i++;
      } else {
        valid_args = false;
      }

      // ignore empty arguments
    } else if (arg == "") {
      // Invalid switch.
    } else {
      log.Log(kLogError, "Unknown parameter: %s\n", arg.c_str());
      valid_args = false;
    }

    if (!valid_args) break;
  }

  // If blend mode not explicitly specified, calculate it from the texture
  // formats.
  if (args->blend_mode < 0) {
    args->blend_mode = DefaultBlendMode(args->texture_formats);
  }

  // Print usage.
  if (!valid_args) {
    static const char kOptionIndent[] = "                           ";
    log.Log(
        kLogImportant,
        "Usage: mesh_pipeline [-b ASSET_BASE_DIR] [-r ASSET_REL_DIR]\n"
        "                     [-e TEXTURE_EXTENSION] [-f TEXTURE_FORMATS]\n"
        "                     [-m BLEND_MODE] [-a AXES] [-u (unit)|(scale)]\n"
        "                     [--attrib p|n|t|q|u|v|c|b]\n"
        "                     [--force-32-bit-indices] [--no-textures]\n"
        "                     [--embed-materials] [-h] [-c] [-l] [-v|-d|-i]\n"
        "                     FBX_FILE\n"
        "\n"
        "Pipeline to convert FBX mesh data into FlatBuffer mesh data.\n"
        "We output a .fplmesh file and (potentially several) .fplmat files,\n"
        "one for each material. The files have the same base name as\n"
        "FBX_FILE, with a number appended to the .fplmat files if required.\n"
        "The .fplmesh file references the .fplmat files.\n"
        "The .fplmat files reference the textures.\n"
        "\n"
        "Options:\n"
        "  -b, --base-dir ASSET_BASE_DIR\n"
        "  -r, --relative-dir ASSET_REL_DIR\n"
        "                The .fplmesh file and the .fplmat files are output\n"
        "                to the ASSET_BASE_DIR/ASSET_REL_DIR directory.\n"
        "                ASSET_BASE_DIR is the working directory of your app,\n"
        "                from which all files are loaded. The .fplmesh file\n"
        "                references the .fplmat file relative to\n"
        "                ASSET_BASE_DIR, that is, by prefixing ASSET_REL_DIR.\n"
        "                If ASSET_BASE_DIR is unspecified, use current\n"
        "                directory. If ASSET_REL_DIR is unspecified, output\n"
        "                and reference files from ASSET_BASE_DIR.\n"
        "  -e, --texture-extension TEXTURE_EXTENSION\n"
        "                material files use this extension for texture files.\n"
        "                Useful if your textures are externally converted\n"
        "                to a different file format.\n"
        "                If unspecified, uses original file extension.\n"
        "  -f, --texture-formats TEXTURE_FORMATS\n"
        "                comma-separated list of formats for each output\n"
        "                texture. For example, if a mesh has two textures\n"
        "                then `AUTO,F_888` will ensure the second texture's\n"
        "                material has 8-bits of RGB precision.\n"
        "                Default is %s.\n"
        "                Valid possibilities:\n",
        matdef::EnumNameTextureFormat(fplbase::kDefaultTextureFormat));
    LogOptions(kOptionIndent, matdef::EnumNamesTextureFormat(), &log);

    log.Log(
        kLogImportant,
        "  -m, --blend-mode BLEND_MODE\n"
        "                rendering blend mode for the generated materials.\n"
        "                If texture format has an alpha channel, defaults to\n"
        "                ALPHA. Otherwise, defaults to OFF.\n"
        "                Valid possibilities:\n");
    LogOptions(kOptionIndent, matdef::EnumNamesBlendMode(), &log);

    log.Log(
        kLogImportant,
        "  -a, --axes AXES\n"
        "                coordinate system of exported file, in format\n"
        "                    (up-axis)(front-axis)(left-axis) \n"
        "                where,\n"
        "                    'up' = [x|y|z]\n"
        "                    'front' = [+x|-x|+y|-y|+z|-z], is the axis\n"
        "                      pointing out of the front of the mesh.\n"
        "                      For example, the vector pointing out of a\n"
        "                      character's belly button.\n"
        "                    'left' = [+x|-x|+y|-y|+z|-z], is the axis\n"
        "                      pointing out the left of the mesh.\n"
        "                      For example, the vector from the character's\n"
        "                      neck to his left shoulder.\n"
        "                For example, 'z+y+x' is z-axis up, positive y-axis\n"
        "                out of a character's belly button, positive x-axis\n"
        "                out of a character's left side.\n"
        "                If unspecified, use file's coordinate system.\n"
        "  -u, --unit (unit)|(scale)\n"
        "                Outputs mesh in target units. You can override the\n"
        "                FBX file's distance unit with this option.\n"
        "                For example, if your game runs in meters,\n"
        "                specify '-u m' to ensure the output .fplmesh file\n"
        "                is in meters, no matter the distance unit of the\n"
        "                FBX file.\n"
        "                (unit) can be one of the following:\n");
    LogOptions(kOptionIndent, fplutil::DistanceUnitNames(), &log);

    log.Log(
        kLogImportant,
        "                (scale) is the number of centimeters in your\n"
        "                distance unit. For example, instead of '-u inches',\n"
        "                you could also use '-u 2.54'.\n"
        "                If unspecified, use FBX file's unit.\n"
        "  -bt, --bake-translation X Y Z\n"
        "                Bake translation into vertices.\n"
        "  -br, --bake-rotation X Y Z\n"
        "                Bake axis rotations (in degrees) into vertices.\n"
        "  -bs, --bake-scale X Y Z\n"
        "                Bake scale into vertices.\n"
        "  --attrib, --vertex-attributes ATTRIBUTES\n"
        "                Composition of the output vertex buffer.\n"
        "                If unspecified, output attributes in source file.\n"
        "                ATTRIBUTES is a combination of the following:\n");
    LogOptions(kOptionIndent, fplbase::kVertexAttributeShortNames, &log);

    log.Log(
        kLogImportant,
        "                For example, '--attrib pu' outputs the positions and\n"
        "                UVs into the vertex buffer, but ignores normals,\n"
        "                colors, and all other per-vertex data.\n"
        "  -c, --center  ensure world origin is inside geometry bounding box\n"
        "                by adding a translation if required.\n"
        "  -l, --non-interleaved\n"
        "                Write out vextex attributes in non-interleaved\n"
        "                format (per-attribute arrays).\n"
        "  --force-32-bit-indices\n"
        "                By default, decides to use 16 or 32 bit indices\n"
        "                on index count. This makes it always use 32 bit.\n"
        "  --no-textures\n"
        "                Do not search for textures or create .fplmat files.\n"
        "  --embed-materials\n"
        "                Embeds the material data directly into the .fplmesh\n"
        "                file instead of generating separate .fplmat files.\n"
        "  -v, --verbose output all informative messages\n"
        "  -d, --details output important informative messages\n"
        "  -i, --info    output more than details, less than verbose\n");
  }

  args->bake_transform.SetTRS(bake_translation, bake_rotation, bake_scale);
  return valid_args;
}

int main(int argc, char** argv) {
  fplutil::Logger log;

  // Parse the command line arguments.
  fplbase::MeshPipelineArgs args;
  if (!ParseMeshPipelineArgs(argc, argv, log, &args)) return 1;

  return fplbase::RunMeshPipeline(args, log);
}
