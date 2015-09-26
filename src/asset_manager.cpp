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
#include "common_generated.h"
#include "fplbase/asset_manager.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/utilities.h"
#include "materials_generated.h"
#include "mesh_generated.h"

namespace fpl {

static_assert(
    kBlendModeOff == static_cast<BlendMode>(matdef::BlendMode_OFF) &&
    kBlendModeTest == static_cast<BlendMode>(matdef::BlendMode_TEST) &&
    kBlendModeAlpha == static_cast<BlendMode>(matdef::BlendMode_ALPHA) &&
    kBlendModeAdd == static_cast<BlendMode>(matdef::BlendMode_ADD),
              "BlendMode enums in renderer.h and material.fbs must match.");
static_assert(kBlendModeCount == kBlendModeAdd + 1,
              "Please update static_assert above with new enum values.");

template <typename T>
T FindInMap(const std::map<std::string, T> &map, const char *name) {
  auto it = map.find(name);
  return it != map.end() ? it->second : 0;
}

AssetManager::AssetManager(Renderer &renderer) : renderer_(renderer) {
  // Empty material for default case.
  material_map_[""] = new Material();
}

Shader *AssetManager::FindShader(const char *basename) {
  return FindInMap(shader_map_, basename);
}

Shader *AssetManager::LoadShader(const char *basename) {
  auto shader = FindShader(basename);
  if (shader) return shader;
  std::string vs_file, ps_file;
  std::string filename = std::string(basename) + ".glslv";
  std::string failedfile;
  if (LoadFileWithIncludes(filename.c_str(), &vs_file, &failedfile)) {
    filename = std::string(basename) + ".glslf";
    if (LoadFileWithIncludes(filename.c_str(), &ps_file, &failedfile)) {
      shader = renderer_.CompileAndLinkShader(vs_file.c_str(), ps_file.c_str());
      if (shader) {
        shader_map_[basename] = shader;
      } else {
        LogError(kError, "Shader Error: ");
        LogError(kError, "VS:  -----------------------------------");
        LogError(kError, "%s", vs_file.c_str());
        LogError(kError, "PS:  -----------------------------------");
        LogError(kError, "%s", ps_file.c_str());
        LogError(kError, "----------------------------------------");
        LogError(kError, "%s", renderer_.last_error().c_str());
      }
      return shader;
    }
  }
  LogError(kError, "Can\'t load shader file: %s", failedfile.c_str());
  renderer_.last_error() = "Couldn\'t load: " + failedfile;
  return nullptr;
}

Texture *AssetManager::FindTexture(const char *filename) {
  return FindInMap(texture_map_, filename);
}

Texture *AssetManager::LoadTexture(const char *filename, TextureFormat format) {
  auto tex = FindTexture(filename);
  if (tex) return tex;
  tex = new Texture(renderer_, filename);
  tex->set_desired_format(format);
  loader_.QueueJob(tex);
  texture_map_[filename] = tex;
  return tex;
}

void AssetManager::StartLoadingTextures() { loader_.StartLoading(); }

bool AssetManager::TryFinalize() { return loader_.TryFinalize(); }

Material *AssetManager::FindMaterial(const char *filename) {
  return FindInMap(material_map_, filename);
}

Material *AssetManager::LoadMaterial(const char *filename) {
  auto mat = FindMaterial(filename);
  if (mat) return mat;
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(matdef::VerifyMaterialBuffer(verifier));
    auto matdef = matdef::GetMaterial(flatbuf.c_str());
    mat = new Material();
    mat->set_blend_mode(static_cast<BlendMode>(matdef->blendmode()));
    for (size_t i = 0; i < matdef->texture_filenames()->size(); i++) {
      auto format =
          matdef->desired_format() && i < matdef->desired_format()->size()
              ? static_cast<TextureFormat>(matdef->desired_format()->Get(i))
              : kFormatAuto;
      auto tex =
          LoadTexture(matdef->texture_filenames()->Get(i)->c_str(), format);
      mat->textures().push_back(tex);

      auto original_size =
          matdef->original_size() && i < matdef->original_size()->size()
              ? LoadVec2i(matdef->original_size()->Get(i))
              : tex->size();
      tex->set_original_size(original_size);
    }
    material_map_[filename] = mat;
    return mat;
  }
  renderer_.last_error() = std::string("Couldn\'t load: ") + filename;
  return nullptr;
}

void AssetManager::UnloadMaterial(const char *filename) {
  auto mat = FindMaterial(filename);
  if (!mat) return;
  mat->DeleteTextures();
  material_map_.erase(filename);
  for (auto it = mat->textures().begin(); it != mat->textures().end(); ++it) {
    texture_map_.erase((*it)->filename());
  }
}

Mesh *AssetManager::FindMesh(const char *filename) {
  return FindInMap(mesh_map_, filename);
}

template <typename T>
void CopyAttribute(const T *attr, uint8_t *&buf) {
  auto dest = (T *)buf;
  *dest = *attr;
  buf += sizeof(T);
}

Mesh *AssetManager::LoadMesh(const char *filename) {
  auto mesh = FindMesh(filename);
  if (mesh) return mesh;
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(meshdef::VerifyMeshBuffer(verifier));
    auto meshdef = meshdef::GetMesh(flatbuf.c_str());
    const bool skin = meshdef->skin_indices() && meshdef->skin_weights() &&
                      meshdef->bone_transforms() && meshdef->bone_parents() &&
                      meshdef->shader_to_mesh_bones();
    // Collect what attributes are available.
    std::vector<Attribute> attrs;
    attrs.push_back(kPosition3f);
    if (meshdef->normals()) attrs.push_back(kNormal3f);
    if (meshdef->tangents()) attrs.push_back(kTangent4f);
    if (meshdef->colors()) attrs.push_back(kColor4ub);
    if (meshdef->texcoords()) attrs.push_back(kTexCoord2f);
    if (skin) {
      attrs.push_back(kBoneIndices4ub);
      attrs.push_back(kBoneWeights4ub);
    }
    attrs.push_back(kEND);
    auto vert_size = Mesh::VertexSize(attrs.data());
    // Create an interleaved buffer. Would be cool to do this without
    // the additional copy, but that's not easy in OpenGL.
    // Could use multiple buffers instead, but likely less efficient.
    auto buf = new uint8_t[vert_size * meshdef->positions()->Length()];
    auto p = buf;
    for (size_t i = 0; i < meshdef->positions()->Length(); i++) {
      if (meshdef->positions()) CopyAttribute(meshdef->positions()->Get(i), p);
      if (meshdef->normals()) CopyAttribute(meshdef->normals()->Get(i), p);
      if (meshdef->tangents()) CopyAttribute(meshdef->tangents()->Get(i), p);
      if (meshdef->colors()) CopyAttribute(meshdef->colors()->Get(i), p);
      if (meshdef->texcoords()) CopyAttribute(meshdef->texcoords()->Get(i), p);
      if (skin) {
        CopyAttribute(meshdef->skin_indices()->Get(i), p);
        CopyAttribute(meshdef->skin_weights()->Get(i), p);
      }
    }
    vec3 max = meshdef->max_position() ? LoadVec3(meshdef->max_position())
                                       : mathfu::kZeros3f;
    vec3 min = meshdef->min_position() ? LoadVec3(meshdef->min_position())
                                       : mathfu::kZeros3f;
    mesh = new Mesh(buf, meshdef->positions()->Length(), vert_size,
                    attrs.data(), meshdef->max_position() ? &max : nullptr,
                    meshdef->min_position() ? &min : nullptr);
    delete[] buf;
    // Load the bone information.
    if (skin) {
      const size_t num_bones = meshdef->bone_parents()->Length();
      assert(meshdef->bone_transforms()->Length() == num_bones);
      std::unique_ptr<mat4[]> bone_transforms(new mat4[num_bones]);
      std::vector<const char*> bone_names(num_bones);
      for (size_t i = 0; i < num_bones; ++i) {
        bone_transforms[i] = LoadAffineMat4(meshdef->bone_transforms()->Get(i));
        bone_names[i] = meshdef->bone_names()->Get(i)->c_str();
      }
      const uint8_t* bone_parents = meshdef->bone_parents()->data();
      mesh->SetBones(&bone_transforms[0], bone_parents, &bone_names[0],
                     num_bones, meshdef->shader_to_mesh_bones()->Data(),
                     meshdef->shader_to_mesh_bones()->Length());
    }

    // Load indices an materials.
    for (size_t i = 0; i < meshdef->surfaces()->size(); i++) {
      auto surface = meshdef->surfaces()->Get(i);
      auto mat = LoadMaterial(surface->material()->c_str());
      if (!mat) {
        delete mesh;
        return nullptr;
      }  // Error msg already set.
      mesh->AddIndices(
          reinterpret_cast<const uint16_t *>(surface->indices()->Data()),
          surface->indices()->Length(), mat);
    }
    mesh_map_[filename] = mesh;
    return mesh;
  }
  renderer_.last_error() = std::string("Couldn\'t load: ") + filename;
  return nullptr;
}

void AssetManager::UnloadMesh(const char *filename) {
  auto mesh = FindMesh(filename);
  if (!mesh) return;
  mesh_map_.erase(filename);
  delete mesh;
}

}  // namespace fpl
