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

// Suppress warnings in external header.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <fbxsdk.h>
#pragma GCC diagnostic pop

#include <assert.h>
#include <fstream>
#include <stdarg.h>
#include <stdio.h>
#include <vector>

#include "common_generated.h"
#include "fplbase/fpl_common.h"
#include "fplutil/file_utils.h"
#include "materials_generated.h"
#include "mesh_generated.h"

namespace fpl {

static const char kTargetSeparator[] = "__";

// Each log message is given a level of importance.
// We only output messages that have level >= our current logging level.
enum LogLevel {
  kLogVerbose,
  kLogInfo,
  kLogImportant,
  kLogWarning,
  kLogError,
  kNumLogLevels
};

// Prefix log messages at this level with this message.
static const char* kLogPrefix[] = {
    "",           // kLogVerbose
    "",           // kLogInfo
    "",           // kLogImportant
    "Warning: ",  // kLogWarning
    "Error: "     // kLogError
};
static_assert(FPL_ARRAYSIZE(kLogPrefix) == kNumLogLevels,
              "kLogPrefix length is incorrect");

/// @class Logger
/// @brief Output log messages if they are above an adjustable threshold.
class Logger {
 public:
  Logger() : level_(kLogImportant) {}

  void set_level(LogLevel level) { level_ = level; }
  LogLevel level() const { return level_; }

  /// Output a printf-style message if our current logging level is
  /// >= `level`.
  void Log(LogLevel level, const char* format, ...) const {
    if (level < level_) return;

    // Prefix message with log level, if required.
    const char* prefix = kLogPrefix[level];
    if (prefix[0] != '\0') {
      printf("%s", prefix);
    }

    // Redirect output to stdout.
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
  }

 private:
  LogLevel level_;
};

/// Return the direct index into `element`. If `element` is set up to be indexed
/// directly, the return value is just `index`. Otherwise, we dereference the
/// index array to get the direct index.
template <class T>
static int ElementDirectIndex(const FbxLayerElementTemplate<T>& element,
                              int index) {
  return element.GetReferenceMode() == FbxGeometryElement::eDirect
             ? index
             : element.GetIndexArray().GetAt(index);
}

/// Return element[index], accounting for the index array, if it is used.
template <class T>
static T Element(const FbxLayerElementTemplate<T>& element, int index) {
  const int direct_index = ElementDirectIndex(element, index);
  return element.GetDirectArray().GetAt(direct_index);
}

static inline Vec3 Vec3FromFbx(const FbxVector4& v) {
  const FbxDouble* d = v.mData;
  return Vec3(static_cast<float>(d[0]), static_cast<float>(d[1]),
              static_cast<float>(d[2]));
}

static inline Vec2 Vec2FromFbx(const FbxVector2& v) {
  const FbxDouble* d = v.mData;
  return Vec2(static_cast<float>(d[0]), static_cast<float>(d[1]));
}

class FlatMesh {
 public:
  explicit FlatMesh(Logger& log) : log_(log) {}

  // Set the size of these arrays at the start so that we're not constantly
  // reallocating them. This is an optional call to improve performance.
  void Reserve(int num_surfaces, int num_control_points) {
    surfaces_.reserve(num_surfaces);
    vertices_.reserve(num_control_points);
    normals_.reserve(num_control_points);
    uvs_.reserve(num_control_points);
  }

  // Populate a single surface with data from FBX arrays.
  void AppendSurface(const char* texture_file_name, const int* indices,
                     int num_indices, const FbxVector4* vertices,
                     const FbxGeometryElementNormal& normal_element,
                     const FbxVector2* uvs, int num_control_points) {
    // Lengthen surfaces_ by one and get reference to new last element.
    surfaces_.resize(surfaces_.size() + 1);
    FlatSurface& s = surfaces_.back();

    // Remember the source file name.
    s.texture_file_name = std::string(texture_file_name == nullptr ? "" :
                                      texture_file_name);

    // Copy the index array, but account for the existing control points.
    const uint32_t first_index = static_cast<uint32_t>(s.indices.size());
    s.indices.reserve(num_indices);
    for (int i = 0; i < num_indices; ++i) {
      if (indices[i] < 0) {
        log_.Log(kLogWarning, "Index %d is negative (%d)\n", i, indices[i]);
      }
      s.indices.push_back(first_index + static_cast<uint32_t>(indices[i]));
    }

    // Append the control point data.
    const size_t start_control_point = vertices_.size();
    const size_t end_control_point = start_control_point + num_control_points;
    vertices_.reserve(end_control_point);
    normals_.reserve(end_control_point);
    uvs_.reserve(end_control_point);
    for (int i = 0; i < num_control_points; ++i) {
      vertices_.push_back(Vec3FromFbx(vertices[i]));
      normals_.push_back(Vec3FromFbx(Element(normal_element, i)));
      uvs_.push_back(Vec2FromFbx(uvs[i]));
    }

    // Log the data we just added.
    log_.Log(kLogInfo, "Surface[%d]: texture %s\n", surfaces_.size() - 1,
             HasTexture(s) ? texture_file_name : "none");
    if (log_.level() <= kLogVerbose) {
      for (size_t i = 0; i < s.indices.size(); ++i) {
        log_.Log(kLogVerbose, "index[%d]: %d\n", i, s.indices[i]);
      }
      for (size_t k = start_control_point; k < end_control_point; ++k) {
        const Vec3& v = vertices_[k];
        const Vec3& n = normals_[k];
        const Vec2& u = uvs_[k];
        log_.Log(kLogVerbose,
                 "control point[%d]:"
                 " vertex (%.3f, %.3f, %.3f)"
                 ", normal (%.3f, %.3f, %.3f)"
                 ", uv (%.3f, %.3f)\n",
                 k - start_control_point, v.x(), v.y(), v.z(), n.x(), n.y(),
                 n.z(), u.x(), u.y());
      }
    }
  }

  // Output material and mesh flatbuffers for the gathered surfaces.
  bool OutputFlatBuffer(const std::string& mesh_dir,
                        const std::string& material_dir,
                        const std::string& texture_dir,
                        const std::string& prefix) const {
    // Copy source texture files to target directory.
    const bool copy_textures = texture_dir != "";
    if (copy_textures) {
      // Ensure texture output directory exists.
      if (!CreateDirectory(texture_dir.c_str())) {
        log_.Log(kLogError, "Could not create texture output directory %s\n",
                 texture_dir.c_str());
        return false;
      }

      const std::string texture_path = DirectoryName(texture_dir) + prefix;
      CopyTextures(texture_path);
    }

    // Ensure mesh output directory exists.
    if (!CreateDirectory(mesh_dir.c_str())) {
      log_.Log(kLogError, "Could not create mesh output directory %s\n",
               mesh_dir.c_str());
      return false;
    }

    // Create final mesh file that references surfaces and textures.
    const std::string mesh_path = DirectoryName(mesh_dir) + prefix;
    const std::string material_path = DirectoryName(material_dir) + prefix;
    OutputMeshFlatBuffer(mesh_path, material_path);
    return true;
  }

 private:
  struct FlatSurface {
    std::string texture_file_name;
    std::vector<uint32_t> indices;
  };

  static bool HasTexture(const FlatSurface& s) {
    return s.texture_file_name.length() > 0;
  }

  static std::string TextureFileName(const FlatSurface& s,
                                     const std::string& texture_path) {
    assert(HasTexture(s));
    return texture_path + std::string(kTargetSeparator) +
           RemoveDirectoryFromName(s.texture_file_name);
  }

  static std::string MaterialFileName(const FlatSurface& s,
                                      const std::string& material_path) {
    if (!HasTexture(s))
      return std::string("");

    return material_path + std::string(kTargetSeparator) +
           BaseFileName(s.texture_file_name) + std::string(".") +
           std::string(matdef::MaterialExtension());
  }

  void CopyTextures(const std::string& texture_path) const {
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatSurface& s = *it;
      if (!HasTexture(s))
        continue;

      // Copy to target location. Source location could be anywhere,
      // including a temporary file if the texture is embedded inside
      // the FBX file.
      const std::string target = TextureFileName(s, texture_path);
      const bool copy_result = CopyFile(target, s.texture_file_name);

      // Log copy operation.
      log_.Log(kLogInfo, "Copying texture file %s to %s\n",
               s.texture_file_name.c_str(), target.c_str());
      if (!copy_result) {
        log_.Log(kLogError, "Could not copy texture file %s to %s\n",
                 s.texture_file_name.c_str(), target.c_str());
      }
    }
  }

  void OutputFlatBufferBuilder(const flatbuffers::FlatBufferBuilder& fbb,
                               const std::string& file_name) const {
    // Open the file.
    FILE* file = fopen(file_name.c_str(), "wb");
    if (file == nullptr) {
      log_.Log(kLogError, "Could not open %s for writing\n", file_name.c_str());
      return;
    }

    // Write the binary data to the file and close it.
    // TODO: Add option to write json file too.
    log_.Log(kLogVerbose, "Writing %s\n", file_name.c_str());
    fwrite(fbb.GetBufferPointer(), 1, fbb.GetSize(), file);
    fclose(file);
  }

  void OutputMeshFlatBuffer(const std::string& mesh_path,
                            const std::string& material_path) const {
    flatbuffers::FlatBufferBuilder fbb;

    // Output the surfaces.
    std::vector<flatbuffers::Offset<meshdef::Surface>> surfaces_fb;
    surfaces_fb.reserve(surfaces_.size());
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const FlatSurface& s = *it;
      auto material_fb = fbb.CreateString(MaterialFileName(s, material_path));
      auto indices_fb = fbb.CreateVector(s.indices);
      auto surface_fb = meshdef::CreateSurface(fbb, indices_fb, material_fb);
      surfaces_fb.push_back(surface_fb);
    }
    auto surface_vector_fb = fbb.CreateVector(surfaces_fb);

    // Output the mesh.
    auto vertices_fb = fbb.CreateVectorOfStructs(vertices_);
    auto normals_fb = fbb.CreateVectorOfStructs(normals_);
    auto uvs_fb = fbb.CreateVectorOfStructs(uvs_);
    auto mesh_fb = meshdef::CreateMesh(fbb, surface_vector_fb, vertices_fb,
                                       normals_fb, 0, 0, uvs_fb);

    // Finalize the buffer and write it to a file.
    meshdef::FinishMeshBuffer(fbb, mesh_fb);
    OutputFlatBufferBuilder(fbb, mesh_path + std::string(".") +
                                     std::string(meshdef::MeshExtension()));
  }

  std::vector<FlatSurface> surfaces_;
  std::vector<Vec3> vertices_;
  std::vector<Vec3> normals_;
  std::vector<Vec2> uvs_;

  // Information and warnings.
  Logger& log_;
};

/// @class FbxParser
/// @brief Load FBX files and save their geometry and animations in our
///        FlatBuffer format.
class FbxParser {
 public:
  explicit FbxParser(Logger& log)
      : manager_(nullptr), scene_(nullptr), log_(log) {
    // The FbxManager is the gateway to the FBX API.
    manager_ = FbxManager::Create();
    if (manager_ == nullptr) {
      log_.Log(kLogError, "Unable to create FBX manager.\n");
      return;
    }

    // Initialize with standard IO settings.
    FbxIOSettings* ios = FbxIOSettings::Create(manager_, IOSROOT);
    manager_->SetIOSettings(ios);

    // Create an FBX scene. This object holds most objects imported/exported
    // from/to files.
    scene_ = FbxScene::Create(manager_, "My Scene");
    if (scene_ == nullptr) {
      log_.Log(kLogError, "Unable to create FBX scene.\n");
      return;
    }
  }

  ~FbxParser() {
    // Delete the FBX Manager and all objects that it created.
    if (manager_ != nullptr) manager_->Destroy();
  }

  bool Valid() const { return manager_ != nullptr && scene_ != nullptr; }

  bool Load(const char* file_name) {
    if (!Valid()) return false;

    // Create the importer and initialize with the file.
    FbxImporter* importer = FbxImporter::Create(manager_, "");
    const bool init_status =
        importer->Initialize(file_name, -1, manager_->GetIOSettings());

    // Check the SDK and pipeline versions.
    int sdk_major = 0, sdk_minor = 0, sdk_revision = 0;
    int file_major = 0, file_minor = 0, file_revision = 0;
    FbxManager::GetFileFormatVersion(sdk_major, sdk_minor, sdk_revision);
    importer->GetFileVersion(file_major, file_minor, file_revision);

    // Report version information.
    log_.Log(kLogImportant,
             "Loading %s (version %d.%d.%d) with SDK version %d.%d.%d\n",
             file_name, file_major, file_minor, file_revision, sdk_major,
             sdk_minor, sdk_revision);

    // Exit on load error.
    if (!init_status) {
      FbxString error = importer->GetStatus().GetErrorString();
      log_.Log(kLogError, "%s\n\n", error.Buffer());
      return false;
    }
    if (!importer->IsFBX()) {
      log_.Log(kLogError, "Not an FBX file\n\n");
      return false;
    }

    // Import the scene.
    const bool import_status = importer->Import(scene_);

    // Clean-up temporaries.
    importer->Destroy();

    // Exit if the import failed.
    if (!import_status) return false;

    // Bring the geo into our format.
    ConvertGeometry();
    return true;
  }

  // Gather converted geometry into our `FlatMesh` class.
  void GatherFlatMesh(FlatMesh* out) const {
    // Pre-count the number of surfaces and control points so that we're
    // not constantly resizing `out`.
    MeshCount count;
    CountMeshes(scene_->GetRootNode(), &count);
    out->Reserve(count.num_meshes, count.num_control_points);

    // Traverse the scene and output one surface per mesh.
    GatherFlatMeshRecursive(scene_->GetRootNode(), out);
  }

 private:
  struct MeshCount {
    MeshCount() : num_meshes(0), num_control_points(0) {}
    int num_meshes;
    int num_control_points;
  };

  void CountMeshes(FbxNode* node, MeshCount* count) const {
    if (node == nullptr) return;

    // For each mesh, increment our counters.
    FbxMesh* mesh = node->GetMesh();
    if (mesh != nullptr) {
      count->num_meshes++;
      count->num_control_points += mesh->GetControlPointsCount();
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      CountMeshes(node->GetChild(i), count);
    }
  }

  void ConvertGeometry() {
    // Ensure each mesh has normals in correct format.
    ConvertGeometryRecursive(scene_->GetRootNode());

    // Ensure each mesh has only one texture, and only triangles.
    FbxGeometryConverter geo_converter(manager_);
    geo_converter.SplitMeshesPerMaterial(scene_, true);
    geo_converter.Triangulate(scene_, true);
  }

  void ConvertGeometryRecursive(FbxNode* node) {
    if (node == nullptr) return;

    FbxMesh* mesh = node->GetMesh();
    if (mesh != nullptr) {
      // Duplicate control points so that each control point has only one
      // normal. If we had a cube, for example, we'd go from 8 control points
      // (the corners) to 24 control points (three faces adjaced to each
      // corner).
      FbxGeometryConverter geo_converter(manager_);
      geo_converter.EmulateNormalsByPolygonVertex(mesh);
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      ConvertGeometryRecursive(node->GetChild(i));
    }
  }

  // Get the UVs for a mesh.
  const FbxGeometryElementUV* UvElement(const FbxMesh* mesh) const {
    // Grab texture coordinates.
    const int uv_count = mesh->GetElementUVCount();
    if (uv_count <= 0) {
      log_.Log(kLogWarning, "No UVs for mesh %s\n", mesh->GetName());
      return nullptr;
    }

    // Always use the first UV set.
    const FbxGeometryElementUV* uv_element = mesh->GetElementUV(0);

    // Warn if multiple UV sets exist.
    if (uv_count > 1 && log_.level() >= kLogWarning) {
      FbxStringList uv_set_names;
      mesh->GetUVSetNames(uv_set_names);
      log_.Log(kLogWarning, "Multiple UVs for mesh %s. Using %s. Ignoring %s\n",
               mesh->GetName(), uv_set_names.GetStringAt(0),
               uv_set_names.GetStringAt(1));
    } else {
      log_.Log(kLogVerbose, "Using UV map %s for mesh %s.\n",
               uv_element->GetName(), mesh->GetName());
    }

    return uv_element;
  }

  /// Gather one UV for each control point.
  /// @param uv_element source UVs
  /// @param mesh control points
  /// @param uvs output UVs, one per control point.
  void UvByControlPoint(const FbxGeometryElementUV* uv_element,
                        const FbxMesh* mesh, FbxVector2* uvs) const {
    const int num_control_points = mesh->GetControlPointsCount();

    switch (uv_element->GetMappingMode()) {
      // If source UVs are already listed per-control point, we simply
      // dereference them in control-index order.
      case FbxGeometryElement::eByControlPoint: {
        const int num_control_points = mesh->GetControlPointsCount();
        for (int control_index = 0; control_index < num_control_points;
             ++control_index) {
          const FbxVector2 uv = Element(*uv_element, control_index);
          uvs[control_index] = uv;
        }
        break;
      }

      // If source UVs are listed per polygon-vertex, then there will be
      // multiple UVs per control point. However, all the UVs should be the
      // same, since we've already called SplitMeshesPerMaterial(). That is,
      // since there is only ever one texture per mesh, a given vertex should
      // have the same UV no matter which polygon it belongs to.
      case FbxGeometryElement::eByPolygonVertex: {
        const FbxDouble kMaxUvDist = 0.005f;

        // Invalidate the output UVs.
        const FbxVector2 kInvalidUv(-100.0f, -100.0f);
        for (int control_index = 0; control_index < num_control_points;
             ++control_index) {
          uvs[control_index] = kInvalidUv;
        }

        // Loop through each vertex of each polygon.
        int vertex_counter = 0;
        const int num_polys = mesh->GetPolygonCount();
        for (int poly_index = 0; poly_index < num_polys; ++poly_index) {
          const int num_verts = mesh->GetPolygonSize(poly_index);
          for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
            // Get the control index for this poly, vert combination.
            const int control_index =
                mesh->GetPolygonVertex(poly_index, vert_index);
            log_.Log(kLogVerbose, "poly %d + vert %d --> control point %d",
                     poly_index, vert_index, control_index);

            // Get the UV for this poly, vert combo.
            const FbxVector2 uv = Element(*uv_element, vertex_counter);

            // Record UV or check that it matches UV previously recorded.
            const bool is_uv_already_set = uvs[control_index] != kInvalidUv;
            if (is_uv_already_set) {
              // Warn if doesn't match the existing UV.
              const bool matches = uv.Distance(uvs[control_index]) < kMaxUvDist;
              log_.Log(kLogVerbose, matches ? ", matches\n" : ", mis-match\n");
              if (!matches) {
                log_.Log(kLogWarning,
                         "Multiple UVs (%.3f, %.3f) and (%.3f, %.3f)"
                         " for control point %d\n",
                         uvs[control_index].mData[0],
                         uvs[control_index].mData[1], uv.mData[0], uv.mData[1],
                         control_index);
              }
            } else {
              log_.Log(kLogVerbose, ", UV (%.3f, %.3f)\n", uv.mData[0],
                       uv.mData[1]);
              uvs[control_index] = uv;
            }

            // Control points are listed in order of poly + vertex.
            vertex_counter++;
          }
        }
        break;
      }

      default:
        assert(false);
    }
  }

  // Get the texture for a mesh node.
  const FbxFileTexture* TextureFromNode(FbxNode* node) const {
    // Check every material attached to this node.
    const int material_count = node->GetMaterialCount();
    for (int material_index = 0; material_index < material_count;
         ++material_index) {
      const FbxSurfaceMaterial* material = node->GetMaterial(material_index);
      if (material == nullptr) continue;

      // We only check the diffuse materials. We might want to check others too.
      const FbxProperty property =
          material->FindProperty(FbxSurfaceMaterial::sDiffuse);
      const int texture_count = property.GetSrcObjectCount<FbxFileTexture>();
      if (texture_count == 0) continue;

      // Grab the first texture.
      const FbxFileTexture* texture =
          FbxCast<FbxFileTexture>(property.GetSrcObject<FbxFileTexture>(0));

      // Warn if there are extra unused textures.
      if (texture_count > 1 && log_.level() <= kLogWarning) {
        const FbxFileTexture* texture1 = FbxCast<FbxFileTexture>(
            property.GetSrcObject<FbxFileTexture>(1));
        log_.Log(kLogWarning,
                 "Material %s has multiple textures. Using %s. Ignoring %s.\n",
                 material->GetName(), texture->GetFileName(),
                 texture1->GetFileName());
      }

      // Log the texture we found and return.
      if (texture != nullptr) {
        log_.Log(kLogInfo, "%s using texture %s\n", node->GetName(),
                 texture->GetFileName());
        return texture;
      }
    }

    log_.Log(kLogWarning, "%s has no diffuse textures.\n", node->GetName());
    return nullptr;
  }

  // For each mesh in the tree of nodes under `node`, add a surface to `out`.
  void GatherFlatMeshRecursive(FbxNode* node, FlatMesh* out) const {
    if (node == nullptr) return;
    log_.Log(kLogVerbose, "Node: %s\n", node->GetName());

    // We're only interested in meshes, for the moment.
    FbxMesh* mesh = node->GetMesh();
    if (mesh != nullptr) {
      const FbxFileTexture* texture = TextureFromNode(node);
      GatherFlatSurface(mesh, texture, out);
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      GatherFlatMeshRecursive(node->GetChild(i), out);
    }
  }

  void GatherFlatSurface(const FbxMesh* mesh, const FbxFileTexture* texture,
                         FlatMesh* out) const {
    // Get vertex buffer for mesh geometry.
    const int num_control_points = mesh->GetControlPointsCount();
    const FbxVector4* vertices = mesh->GetControlPoints();

    // Get UVs per-control point.
    const FbxGeometryElementUV* uv_element = UvElement(mesh);
    std::vector<FbxVector2> uvs(num_control_points);
    UvByControlPoint(uv_element, mesh, &uvs[0]);

    // Get index buffer. Indices are into control points.
    const int* indices = mesh->GetPolygonVertices();
    const int num_indices = mesh->GetPolygonVertexCount();

    // Normals should already be mapped per-control point because we've called
    // EmulateNormalsByPolygonVertex().
    const FbxGeometryElementNormal* normal_element = mesh->GetElementNormal();
    if (normal_element->GetMappingMode() !=
        FbxGeometryElement::eByControlPoint) {
      log_.Log(kLogWarning, "Normals are not mapped per-control point\n");
    }

    // Fill out the output structure.
    const char* texture_file_name = texture == nullptr ? nullptr
                                  : texture->GetFileName();
    out->AppendSurface(texture_file_name, indices, num_indices, vertices,
                       *normal_element, &uvs[0], num_control_points);
  };

  // Entry point to the FBX SDK.
  FbxManager* manager_;

  // Hold the FBX file data.
  FbxScene* scene_;

  // Information and warnings.
  Logger& log_;
};

struct MeshPipelineArgs {
  MeshPipelineArgs()
      : fbx_file(""),
        mesh_dir(""),
        texture_dir(""),
        log_level(kLogImportant),
        copy_textures(true) {}

  std::string fbx_file;       /// FBX input file to convert.
  std::string target_prefix;  /// Prepend to all output files.
  std::string mesh_dir;       /// Output directory for FlatBuffer meshes.
  std::string material_dir;   /// Directory with fplmat files used by meshes.
  std::string texture_dir;    /// Output directory for textures.
  LogLevel log_level;         /// Amount of logging to dump during conversion.
  bool copy_textures;         /// Copy the texture files to target_dir, too.
};

static bool ParseMeshPipelineArgs(int argc, char** argv, Logger& log,
                                  MeshPipelineArgs* args) {
  bool valid_args = true;

  // Last parameter is used as file name.
  if (argc > 1) {
    args->fbx_file = std::string(argv[argc - 1]);
    args->target_prefix = BaseFileName(args->fbx_file);
  }

  // Ensure file name is valid.
  const bool valid_fbx_file =
      args->fbx_file.length() > 0 && args->fbx_file[0] != '-';
  if (!valid_fbx_file) {
    valid_args = false;
  }

  // Parse switches.
  for (int i = 1; i < argc - 1; ++i) {
    const std::string arg = argv[i];

    // -v switch
    if (arg == "-v" || arg == "--verbose") {
      args->log_level = kLogVerbose;

      // -l switch
    } else if (arg == "-l" || arg == "--leave-textures") {
      args->copy_textures = false;

      // -o switch
    } else if (arg == "-o") {
      if (i + 1 < argc - 1) {
        i++;
        args->mesh_dir = arg;
      } else {
        valid_args = false;
      }

      // -m switch
    } else if (arg == "-m") {
      if (i + 1 < argc - 1) {
        i++;
        args->material_dir = arg;
      } else {
        valid_args = false;
      }

      // -t switch
    } else if (arg == "-t") {
      if (i + 1 < argc - 1) {
        i++;
        args->texture_dir = arg;
      } else {
        valid_args = false;
      }

      // -p switch
    } else if (arg == "-p") {
      if (i + 1 < argc - 1) {
        i++;
        args->target_prefix = arg;
      } else {
        args->target_prefix = std::string("");
      }

      // Invalid switch.
    } else {
      log.Log(kLogError, "Unknown parameter: %s\n", arg.c_str());
      valid_args = false;
    }
  }

  // Print usage.
  if (!valid_args) {
    log.Log(
        kLogImportant,
        "Usage: mesh_pipeline [-v] [-l] [-o MESH_DIR] [-m MATERIAL_DIR]"
        " [-t TEXTURE_DIR] [-p TARGET_PREFIX] FBX_FILE\n"
        "Pipeline to convert FBX mesh data into FlatBuffer mesh data\n\n"
        "Options:\n"
        "  -v, --verbose        output all informative messages\n"
        "  -o MESH_DIR          directory for generated mesh files.\n"
        "                       If unspecified, use current directory.\n"
        "  -m MATERIAL_DIR      directory that holds material files for mesh\n"
        "                       textures. If unspecified, use mesh directory.\n"
        "                       We expect a texture_name.fplmat file to exist\n"
        "                       for every texture.\n"
        "  -t TEXTURE_DIR       directory for copied texture files; if -t is\n"
        "                       not specified, textures are not copied.\n"
        "  -p TARGET_PREFIX     prepend all output file names with this\n"
        "                       by default, use base name of FBX_FILE.\n"
        "                       if TARGET_PREFIX omitted, use no prefix.\n");
  } else {
    // By default, we assume material files are in the same directory as
    // the output mesh files.
    if (args->material_dir == "") {
      args->material_dir = args->mesh_dir;
    }
  }

  return valid_args;
}

}  // namespace fpl

int main(int argc, char** argv) {
  using namespace fpl;
  Logger log;

  // Parse the command line arguments.
  MeshPipelineArgs args;
  if (!ParseMeshPipelineArgs(argc, argv, log, &args)) return 1;

  // Update the amount of information we're dumping.
  log.set_level(args.log_level);

  // Load the FBX file.
  FbxParser pipe(log);
  const bool load_status = pipe.Load(args.fbx_file.c_str());
  if (!load_status) return 1;

  // Gather data into a format conducive to our FlatBuffer format.
  FlatMesh mesh(log);
  pipe.GatherFlatMesh(&mesh);

  // Output gathered data to a binary FlatBuffer.
  const bool output_status = mesh.OutputFlatBuffer(
      args.mesh_dir, args.material_dir, args.texture_dir, args.target_prefix);
  if (!output_status) return 1;

  // Success.
  return 0;
}
