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
#include "fplbase/texture.h"
#include "fplbase/preprocessor.h"
#include "fplbase/utilities.h"
#include "mesh_generated.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

void FileAsset::Load() {
  if (LoadFile(filename_.c_str(), &contents)) {
    // This is just to signal the load succeeded. data_ doesn't own the memory.
    data_ = reinterpret_cast<const uint8_t *>(contents.c_str());
  }
}

bool FileAsset::Finalize() {
  // Since the asset was already "created", this is all we have to do here.
  data_ = nullptr;
  CallFinalizeCallback();
  return true;
}

bool FileAsset::IsValid() { return true; }

template <typename T>
T FindInMap(const std::map<std::string, T> &map, const char *name) {
  auto it = map.find(name);
  return it != map.end() ? it->second : 0;
}

template <typename T>
void DestructAssetsInMap(std::map<std::string, T> &map) {
  for (auto it = map.begin(); it != map.end(); ++it) {
    delete it->second;
  }
  map.clear();
}

AssetManager::AssetManager(Renderer &renderer)
    : renderer_(renderer), texture_scale_(mathfu::kOnes2f) {
  // Empty material for default case.
  material_map_[""] = new Material();
}

void AssetManager::ClearAllAssets() {
  DestructAssetsInMap(material_map_);
  DestructAssetsInMap(texture_atlas_map_);
  DestructAssetsInMap(mesh_map_);
  DestructAssetsInMap(shader_map_);
  DestructAssetsInMap(texture_map_);
  DestructAssetsInMap(file_map_);
}

Shader *AssetManager::FindShader(const char *basename) {
  return FindInMap(shader_map_, basename);
}

Shader *AssetManager::LoadShaderHelper(
    const char *basename, const std::vector<std::string> &local_defines,
    const char *alias, bool async) {
  auto shader = FindShader(alias != nullptr ? alias : basename);
  const bool found = shader != nullptr;
  if (!found) {
    shader = new Shader(basename, local_defines, &renderer_);
  }
  shader->UpdateGlobalDefines(defines_to_add_, defines_to_omit_);
  return found ? shader : LoadOrQueue(shader, shader_map_, async, alias);
}

Shader *AssetManager::LoadShader(const char *basename,
                                 const std::vector<std::string> &local_defines,
                                 bool async, const char *alias) {
  return LoadShaderHelper(basename, local_defines, alias, async);
}

Shader *AssetManager::LoadShader(const char *basename, bool async,
                                 const char *alias) {
  static const std::vector<std::string> empty_defines;
  return LoadShader(basename, empty_defines, async, alias);
}

void AssetManager::ResetGlobalShaderDefines(
    const std::vector<std::string> &defines_to_add,
    const std::vector<std::string> &defines_to_omit) {
  defines_to_add_ = defines_to_add;
  defines_to_omit_ = defines_to_omit;
  for (auto iter = shader_map_.begin(); iter != shader_map_.end(); ++iter) {
    Shader *shader = iter->second;
    shader->UpdateGlobalDefines(defines_to_add_, defines_to_omit_);
  }
}

void AssetManager::ForEachShaderWithDefine(const char *define,
    const std::function<void(Shader *)> &func) {
  // Use a simple for loop to visit all shaders with 'define' specified, since
  // we only have limited shaders currently. TODO(yifengh): optimize this if
  // there is a growing number of shaders.
  for (auto iter = shader_map_.begin(); iter != shader_map_.end(); ++iter) {
    Shader *shader = iter->second;
    if (ValidShaderHandle(shader->program()) && shader->HasDefine(define)) {
      func(shader);
    }
  }
}

Shader *AssetManager::LoadShaderDef(const char *filename) {
  auto shader = FindShader(filename);
  if (shader) return shader;
  shader = Shader::LoadFromShaderDef(filename);
  if (!shader) return nullptr;
  shader_map_[filename] = shader;
  return shader;
}

void AssetManager::UnloadShader(const char *filename) {
  auto shader = FindShader(filename);
  if (!shader || shader->DecreaseRefCount()) return;
  loader_.AbortJob(shader);
  shader_map_.erase(filename);
  delete shader;
}

Texture *AssetManager::FindTexture(const char *filename) {
  return FindInMap(texture_map_, filename);
}

Texture *AssetManager::LoadTexture(const char *filename, TextureFormat format,
                                   TextureFlags flags) {
  auto tex = FindTexture(filename);
  if (tex) return tex;
  tex = new Texture(filename, format, flags);
  return LoadOrQueue(tex, texture_map_, (flags & kTextureFlagsLoadAsync) != 0,
                     nullptr /* alias */);
}

void AssetManager::StartLoadingTextures() { loader_.StartLoading(); }

void AssetManager::StopLoadingTextures() { loader_.PauseLoading(); }

bool AssetManager::TryFinalize() { return loader_.TryFinalize(); }

void AssetManager::UnloadTexture(const char *filename) {
  auto tex = FindTexture(filename);
  if (!tex || tex->DecreaseRefCount()) return;
  loader_.AbortJob(tex);
  texture_map_.erase(filename);
  delete tex;
}

Material *AssetManager::FindMaterial(const char *filename) {
  return FindInMap(material_map_, filename);
}

Material *AssetManager::LoadMaterial(const char *filename,
                                     bool async_resources) {
  auto mat = FindMaterial(filename);
  if (mat) return mat;
  mat = Material::LoadFromMaterialDef(filename,
    [&](const char *filename, TextureFormat format,
        TextureFlags flags) -> Texture* {
      auto tex = LoadTexture(filename, format, flags |
        (async_resources ? kTextureFlagsLoadAsync : kTextureFlagsNone));
      tex->set_scale(texture_scale_);
      return tex;
    });
  if (!mat) return nullptr;
  material_map_[filename] = mat;
  return mat;
}

void AssetManager::UnloadMaterial(const char *filename) {
  auto mat = FindMaterial(filename);
  if (!mat || mat->DecreaseRefCount()) return;
  mat->DeleteTextures();
  material_map_.erase(filename);
  for (auto it = mat->textures().begin(); it != mat->textures().end(); ++it) {
    texture_map_.erase((*it)->filename());
  }
}

Mesh *AssetManager::FindMesh(const char *filename) {
  return FindInMap(mesh_map_, filename);
}

Mesh *AssetManager::LoadMesh(const char *filename, bool async) {
  auto mesh = FindMesh(filename);
  if (mesh) return mesh;

  auto async_flags = (async ? kTextureFlagsLoadAsync : kTextureFlagsNone);
  auto load_texture_fn = [this, async_flags](const char *filename,
                                             TextureFormat format,
                                             TextureFlags flags) -> Texture * {
    auto tex = LoadTexture(filename, format, flags | async_flags);
    tex->set_scale(texture_scale_);
    return tex;
  };

  mesh =
      new Mesh(filename, [this, async, load_texture_fn](const char *filename,
                                                 const matdef::Material *def) {
        if (def) {
          return Material::LoadFromMaterialDef(def, load_texture_fn);
        } else {
          return LoadMaterial(filename, async);
        }
      });
  return LoadOrQueue(mesh, mesh_map_, async, nullptr /* alias */);
}

void AssetManager::UnloadMesh(const char *filename) {
  auto mesh = FindMesh(filename);
  if (!mesh || mesh->DecreaseRefCount()) return;
  loader_.AbortJob(mesh);
  mesh_map_.erase(filename);
  delete mesh;
}

TextureAtlas *AssetManager::FindTextureAtlas(const char *filename) {
  return FindInMap(texture_atlas_map_, filename);
}

TextureAtlas *AssetManager::LoadTextureAtlas(const char *filename,
                                             TextureFormat format,
                                             TextureFlags flags) {
  auto atlas = FindTextureAtlas(filename);
  if (atlas) return atlas;
  atlas = TextureAtlas::LoadTextureAtlas(filename, format, flags,
    [&](const char *filename, TextureFormat format, TextureFlags flags) {
      return LoadTexture(filename, format, flags);
    });
  if (!atlas) return nullptr;
  texture_atlas_map_[filename] = atlas;
  return atlas;
}

void AssetManager::UnloadTextureAtlas(const char *filename) {
  auto atlas = FindTextureAtlas(filename);
  if (!atlas || atlas->DecreaseRefCount()) return;
  texture_atlas_map_.erase(filename);
  delete atlas;
}

FileAsset *AssetManager::FindFileAsset(const char *filename) {
  return FindInMap(file_map_, filename);
}

FileAsset *AssetManager::LoadFileAsset(const char *filename) {
  auto file = FindFileAsset(filename);
  if (file) return file;
  file = new FileAsset();
  if (LoadFile(filename, &file->contents)) {
    file_map_[filename] = file;
    return file;
  }
  delete file;
  return nullptr;
}

void AssetManager::UnloadFileAsset(const char *filename) {
  auto file = FindFileAsset(filename);
  if (!file || file->DecreaseRefCount()) return;
  file_map_.erase(filename);
  delete file;
}

}  // namespace fplbase
