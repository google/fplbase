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
#include <functional>
#include <stdarg.h>
#include <stdio.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common_generated.h"
#include "fplbase/fpl_common.h"
#include "fplutil/file_utils.h"
#include "materials_generated.h"
#include "mesh_generated.h"

namespace fpl {

static const char kTextureFileExtension[] = ".webp";

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
  explicit FlatMesh(Logger& log) : cur_index_buf_(nullptr), log_(log) {}

  void SetSurface(const std::string& texture_file_name) {
    // Grab existing surface for `texture_file_name`, or create a new one.
    IndexBuffer& index_buffer = surfaces_[texture_file_name];

    // Update the current index buffer to which we're logging control points.
    cur_index_buf_ = &index_buffer;

    // Log the surface switch.
    log_.Log(kLogInfo, "Surface: %s\n", texture_file_name.c_str());
  }

  // Populate a single surface with data from FBX arrays.
  void AppendPolyVert(const Vec3& vertex, const Vec3& normal, const Vec2& uv) {
    // TODO: Round values before creating.
    points_.push_back(ControlPoint(vertex, normal, uv));

    const ControlPointRef ref_to_insert(&points_.back(), points_.size() - 1);
    auto insertion = unique_.insert(ref_to_insert);

    // The `insert` call returns two values: iterator and success.
    const ControlPointRef& ref = *insertion.first;
    const bool new_control_point_created = insertion.second;

    // We recycled an existing point, so we can remove the one we added with
    // push_back().
    if (!new_control_point_created) {
      points_.pop_back();
    }

    // Append index of polygon point.
    cur_index_buf_->push_back(ref.index);

    // Log the data we just added.
    log_.Log(kLogInfo, "Point: index %d", ref.index);
    if (new_control_point_created) {
      log_.Log(kLogInfo,
               ", vertex (%.3f, %.3f, %.3f)"
               ", normal (%.3f, %.3f, %.3f)"
               ", uv (%.3f, %.3f)",
               vertex.x(), vertex.y(), vertex.z(), normal.x(), normal.y(),
               normal.z(), uv.x(), uv.y());
    }
    log_.Log(kLogInfo, "\n");
  }

  // Output material and mesh flatbuffers for the gathered surfaces.
  bool OutputFlatBuffer(const std::string& mesh_name_unformated,
                        const std::string& assets_base_dir_unformated,
                        const std::string& assets_sub_dir_unformated) const {
    // Ensure directory names end with a slash.
    const std::string mesh_name = BaseFileName(mesh_name_unformated);
    const std::string assets_base_dir =
        FormatAsDirectoryName(assets_base_dir_unformated);
    const std::string assets_sub_dir =
        FormatAsDirectoryName(assets_sub_dir_unformated);

    // Ensure output directory exists.
    const std::string assets_dir = assets_base_dir + assets_sub_dir;
    if (!CreateDirectory(assets_dir.c_str())) {
      log_.Log(kLogError, "Could not create output directory %s\n",
               assets_dir.c_str());
      return false;
    }

    // Create material files that reference the textures.
    OutputMaterialFlatBuffers(assets_base_dir, assets_sub_dir);

    // Create final mesh file that references materials relative to
    // `assets_base_dir`.
    OutputMeshFlatBuffer(mesh_name, assets_base_dir, assets_sub_dir);
    return true;
  }

 private:
  typedef uint16_t IndexBufIndex;
  typedef std::vector<IndexBufIndex> IndexBuffer;

  struct ControlPoint {
    Vec3 vertex;
    Vec3 normal;
    Vec2 uv;
    ControlPoint() : vertex(0, 0, 0), normal(0, 0, 0), uv(0, 0) {}
    ControlPoint(const Vec3& v, const Vec3& n, const Vec2& u)
        : vertex(v), normal(n), uv(u) {}
  };

  struct ControlPointRef {
    const ControlPoint* ref;
    IndexBufIndex index;
    ControlPointRef(const ControlPoint* r, size_t index)
        : ref(r), index(static_cast<IndexBufIndex>(index)) {
      assert(index <= std::numeric_limits<IndexBufIndex>::max());
    }
  };

  struct ControlPointHash {
    size_t operator()(const ControlPointRef& c) const {
      const size_t* p = reinterpret_cast<const size_t*>(c.ref);
      size_t hash = 0;
      for (size_t i = 0; i < sizeof(*c.ref) / sizeof(size_t); ++i) {
        hash ^= Rotate(p[i], i);
      }
      return hash;
    }

    static size_t Rotate(size_t x, size_t bytes) {
      return (x << (8 * bytes)) | (x >> (8 * (sizeof(size_t) - bytes)));
    }
  };

  struct ControlPointsEqual {
    bool operator()(const ControlPointRef& a, const ControlPointRef& b) const {
      return memcmp(a.ref, b.ref, sizeof(*a.ref)) == 0;
    }
  };

  typedef std::unordered_map<std::string, IndexBuffer> SurfaceMap;
  typedef std::unordered_set<ControlPointRef, ControlPointHash,
                             ControlPointsEqual> ControlPointSet;

  static bool HasTexture(const std::string& texture_file_name) {
    return texture_file_name.length() > 0;
  }

  static std::string TextureBaseFileName(const std::string& texture_file_name,
                                         const std::string& assets_sub_dir) {
    assert(HasTexture(texture_file_name));
    return assets_sub_dir + BaseFileName(texture_file_name);
  }

  static std::string TextureFileName(const std::string& texture_file_name,
                                     const std::string& assets_sub_dir) {
    return TextureBaseFileName(texture_file_name, assets_sub_dir) +
           kTextureFileExtension;
  }

  static std::string MaterialFileName(const std::string& texture_file_name,
                                      const std::string& assets_sub_dir) {
    return TextureBaseFileName(texture_file_name, assets_sub_dir) + "." +
           matdef::MaterialExtension();
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

  void OutputMaterialFlatBuffers(const std::string& assets_base_dir,
                                 const std::string& assets_sub_dir) const {
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const std::string& texture_file_name = it->first;
      if (!HasTexture(texture_file_name)) continue;

      // TODO: add alpha, format, etc. here instead of using defaults.
      flatbuffers::FlatBufferBuilder fbb;
      auto texture_fb =
          fbb.CreateString(TextureFileName(texture_file_name, assets_sub_dir));
      auto texture_vector_fb = fbb.CreateVector(&texture_fb, 1);
      auto material_fb = matdef::CreateMaterial(fbb, texture_vector_fb);
      matdef::FinishMaterialBuffer(fbb, material_fb);

      const std::string full_material_file_name =
          assets_base_dir + MaterialFileName(texture_file_name, assets_sub_dir);
      OutputFlatBufferBuilder(fbb, full_material_file_name);
    }
  }

  void OutputMeshFlatBuffer(const std::string& mesh_name,
                            const std::string& assets_base_dir,
                            const std::string& assets_sub_dir) const {
    flatbuffers::FlatBufferBuilder fbb;

    const std::string rel_mesh_file_name =
        assets_sub_dir + mesh_name + "." + meshdef::MeshExtension();
    const std::string full_mesh_file_name =
        assets_base_dir + rel_mesh_file_name;
    log_.Log(kLogImportant, "Mesh %s has %d verts\n",
             rel_mesh_file_name.c_str(), points_.size());

    // Output the surfaces.
    std::vector<flatbuffers::Offset<meshdef::Surface>> surfaces_fb;
    surfaces_fb.reserve(surfaces_.size());
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      const std::string& texture_file_name = it->first;
      const IndexBuffer& index_buf = it->second;
      const std::string material_file_name =
          HasTexture(texture_file_name)
              ? MaterialFileName(texture_file_name, assets_sub_dir)
              : std::string("");
      auto material_fb = fbb.CreateString(material_file_name);
      auto indices_fb = fbb.CreateVector(index_buf);
      auto surface_fb = meshdef::CreateSurface(fbb, indices_fb, material_fb);
      surfaces_fb.push_back(surface_fb);

      log_.Log(kLogImportant, "  Surface %s has %d indices\n",
               material_file_name.c_str(), index_buf.size());
    }
    auto surface_vector_fb = fbb.CreateVector(surfaces_fb);

    // Output the mesh.
    // First convert to structure-of-array format.
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    vertices.reserve(points_.size());
    normals.reserve(points_.size());
    uvs.reserve(points_.size());
    for (auto it = points_.begin(); it != points_.end(); ++it) {
      const ControlPoint& p = *it;
      vertices.push_back(p.vertex);
      normals.push_back(p.normal);
      uvs.push_back(p.uv);
    }

    // Then create a FlatBuffer vector for each array.
    auto vertices_fb = fbb.CreateVectorOfStructs(vertices);
    auto normals_fb = fbb.CreateVectorOfStructs(normals);
    auto uvs_fb = fbb.CreateVectorOfStructs(uvs);
    auto mesh_fb = meshdef::CreateMesh(fbb, surface_vector_fb, vertices_fb,
                                       normals_fb, 0, 0, uvs_fb);
    meshdef::FinishMeshBuffer(fbb, mesh_fb);

    // Write the buffer to a file.
    OutputFlatBufferBuilder(fbb, full_mesh_file_name);
  }

  SurfaceMap surfaces_;
  ControlPointSet unique_;
  std::vector<ControlPoint> points_;
  IndexBuffer* cur_index_buf_;

  // Information and warnings.
  Logger& log_;
};

static std::string FindSourceTextureFileName(
    const std::string& source_mesh_name, const std::string& texture_name) {
  // If the texture name is relative, check for it relative to the
  // source mesh's directory.
  const std::string source_dir = DirectoryName(source_mesh_name);
  if (!AbsoluteFileName(texture_name)) {
    const std::string texture_rel_name = source_dir + texture_name;
    if (FileExists(texture_rel_name)) return texture_rel_name;
  }

  // If the texture exists in the same directory as the source mesh, use it.
  const std::string texture_no_dir = RemoveDirectoryFromName(texture_name);
  const std::string texture_in_source_dir = source_dir + texture_no_dir;
  if (FileExists(texture_in_source_dir)) return texture_in_source_dir;

  // Check to see if there's a texture with the same base name as the mesh.
  const std::string source_name = BaseFileName(source_mesh_name);
  const std::string texture_extension = FileExtension(texture_name);
  const std::string source_texture =
      source_dir + source_name + "." + texture_extension;
  if (FileExists(source_texture)) return source_texture;

  // As a last resort, use the texture name as supplied. We don't want to
  // do this, normally, since the name can be an absolute path on the drive,
  // or relative to the directory we're currently running from.
  if (FileExists(texture_name)) return texture_name;

  // Texture can't be found.
  return "";
}

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

    // Remember the source file name so we can search for textures nearby.
    mesh_file_name_ = std::string(file_name);

    // Bring the geo into our format.
    ConvertGeometry();
    return true;
  }

  // Gather converted geometry into our `FlatMesh` class.
  void GatherFlatMesh(FlatMesh* out) const {
    // Traverse the scene and output one surface per mesh.
    GatherFlatMeshRecursive(scene_->GetRootNode(), out);
  }

 private:
  void ConvertGeometry() {
    // Ensure each mesh has only one texture, and only triangles.
    FbxGeometryConverter geo_converter(manager_);
    geo_converter.SplitMeshesPerMaterial(scene_, true);
    geo_converter.Triangulate(scene_, true);
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
        const FbxFileTexture* texture1 =
            FbxCast<FbxFileTexture>(property.GetSrcObject<FbxFileTexture>(1));
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

  std::string TextureFileName(FbxNode* node) const {
    // Grab the texture attached to this node.
    const FbxFileTexture* texture = TextureFromNode(node);
    if (texture == nullptr) {
      log_.Log(kLogWarning, "No texture found for node %s\n", node->GetName());
      return "";
    }

    // Look for a texture on disk that matches the texture referenced by
    // the FBX.
    const std::string texture_file_name = FindSourceTextureFileName(
        mesh_file_name_, std::string(texture->GetFileName()));
    log_.Log(kLogInfo, "Mapping FBX texture %s to texture file %s\n",
             texture->GetFileName(), texture_file_name.c_str());
    return texture_file_name;
  }

  // For each mesh in the tree of nodes under `node`, add a surface to `out`.
  void GatherFlatMeshRecursive(FbxNode* node, FlatMesh* out) const {
    if (node == nullptr) return;
    log_.Log(kLogVerbose, "Node: %s\n", node->GetName());

    // We're only interested in meshes, for the moment.
    FbxMesh* mesh = node->GetMesh();
    if (mesh != nullptr) {
      const std::string texture_file_name = TextureFileName(node);
      out->SetSurface(texture_file_name);
      GatherFlatSurface(mesh, out);
    }

    // Recursively traverse each node in the scene
    for (int i = 0; i < node->GetChildCount(); i++) {
      GatherFlatMeshRecursive(node->GetChild(i), out);
    }
  }

  void GatherFlatSurface(const FbxMesh* mesh, FlatMesh* out) const {
    const FbxVector4* vertices = mesh->GetControlPoints();
    const FbxGeometryElementUV* uv_element = UvElement(mesh);
    const FbxGeometryElementNormal* normal_element = mesh->GetElementNormal();

    const bool uv_by_control_index =
        uv_element->GetMappingMode() == FbxGeometryElement::eByControlPoint;
    const bool normal_by_control_index =
        normal_element->GetMappingMode() == FbxGeometryElement::eByControlPoint;

    // Loop through every poly in the mesh.
    int vertex_counter = 0;
    const int num_polys = mesh->GetPolygonCount();
    for (int poly_index = 0; poly_index < num_polys; ++poly_index) {
      // Ensure polygon is a triangle. This should be true since we call
      // Triangulate() when we load the scene.
      const int num_verts = mesh->GetPolygonSize(poly_index);
      if (num_verts != 3) {
        log_.Log(kLogWarning, "mesh %s poly %d has %d verts instead of 3\n",
                 mesh->GetName(), poly_index, num_verts);
        continue;
      }

      // Loop through all three verts.
      for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
        // Get the control index for this poly, vert combination.
        const int control_index =
            mesh->GetPolygonVertex(poly_index, vert_index);

        // Depending on the FBX format, normals and UVs are indexed either
        // by control point or by polygon-vertex.
        const FbxVector4 normal_fbx =
            Element(*normal_element,
                    normal_by_control_index ? control_index : vertex_counter);
        const FbxVector2 uv_fbx = Element(
            *uv_element, uv_by_control_index ? control_index : vertex_counter);

        // Output this poly-vert.
        // Note that the v-axis is flipped between FBX UVs and FlatBuffer UVs.
        const Vec3 vertex = Vec3FromFbx(vertices[control_index]);
        const Vec3 normal = Vec3FromFbx(normal_fbx);
        const Vec2 uv =
            Vec2FromFbx(FbxVector2(uv_fbx.mData[0], 1.0 - uv_fbx.mData[1]));
        out->AppendPolyVert(vertex, normal, uv);

        // Control points are listed in order of poly + vertex.
        vertex_counter++;
      }
    }
  };

  // Entry point to the FBX SDK.
  FbxManager* manager_;

  // Hold the FBX file data.
  FbxScene* scene_;

  // Name of source mesh file. Used to search for textures, when the textures
  // are not found in their referenced location.
  std::string mesh_file_name_;

  // Information and warnings.
  Logger& log_;
};

struct MeshPipelineArgs {
  MeshPipelineArgs()
      : fbx_file(""),
        asset_base_dir(""),
        asset_rel_dir(""),
        log_level(kLogImportant) {}

  std::string fbx_file;        /// FBX input file to convert.
  std::string asset_base_dir;  /// Directory from which all assets are loaded.
  std::string asset_rel_dir;   /// Directory (relative to base) to output files.
  LogLevel log_level;          /// Amount of logging to dump during conversion.
};

static bool ParseMeshPipelineArgs(int argc, char** argv, Logger& log,
                                  MeshPipelineArgs* args) {
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

  // Parse switches.
  for (int i = 1; i < argc - 1; ++i) {
    const std::string arg = argv[i];

    // -v switch
    if (arg == "-v" || arg == "--verbose") {
      args->log_level = kLogVerbose;

      // -b switch
    } else if (arg == "-b") {
      if (i + 1 < argc - 1) {
        args->asset_base_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
      }

      // -r switch
    } else if (arg == "-r") {
      if (i + 1 < argc - 1) {
        args->asset_rel_dir = std::string(argv[i + 1]);
        i++;
      } else {
        valid_args = false;
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
        "Usage: mesh_pipeline [-v] [-b ASSET_BASE_DIR] [-r ASSET_REL_DIR]"
        " FBX_FILE\n"
        "Pipeline to convert FBX mesh data into FlatBuffer mesh data.\n"
        "We output a .fplmesh file with the same base name as FBX_FILE.\n"
        "For every texture referenced by the FBX, we output a .fplmat file\n"
        "to load the texture. The .fplmesh file references all .fplmat files\n"
        "by names relative to ASSET_BASE_DIR.\n\n"
        "Options:\n"
        "  -v, --verbose        output all informative messages\n"
        "  -b ASSET_BASE_DIR    directory from which all assets are loaded;\n"
        "                       material file paths are relative to here.\n"
        "                       If unspecified, current directory.\n"
        "  -r ASSET_REL_DIR     directory to put all output files; relative\n"
        "                       to ASSET_BASE_DIR. If unspecified, current\n"
        "                       directory.\n");
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
      args.fbx_file, args.asset_base_dir, args.asset_rel_dir);
  if (!output_status) return 1;

  // Success.
  return 0;
}
