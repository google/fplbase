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
#include "fplbase/texture.h"
#include "fplbase/preprocessor.h"
#include "fplbase/utilities.h"
#include "materials_generated.h"
#include "mesh_generated.h"
#include "shader_generated.h"
#include "texture_atlas_generated.h"

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

static_assert(
    kBlendModeOff == static_cast<BlendMode>(matdef::BlendMode_OFF) &&
        kBlendModeTest == static_cast<BlendMode>(matdef::BlendMode_TEST) &&
        kBlendModeAlpha == static_cast<BlendMode>(matdef::BlendMode_ALPHA) &&
        kBlendModeAdd == static_cast<BlendMode>(matdef::BlendMode_ADD) &&
        kBlendModeAddAlpha ==
            static_cast<BlendMode>(matdef::BlendMode_ADDALPHA) &&
        kBlendModeMultiply ==
            static_cast<BlendMode>(matdef::BlendMode_MULTIPLY) &&
        kBlendModePreMultipliedAlpha ==
        static_cast<BlendMode>(matdef::BlendMode_PREMULTIPLIEDALPHA),
    "BlendMode enums in material.h and material.fbs must match.");
static_assert(kBlendModeCount == kBlendModePreMultipliedAlpha + 1,
              "Please update static_assert above with new enum values.");
static_assert(
    kFormatAuto == static_cast<TextureFormat>(matdef::TextureFormat_AUTO) &&
    kFormat8888 == static_cast<TextureFormat>(matdef::TextureFormat_F_8888) &&
    kFormat888 == static_cast<TextureFormat>(matdef::TextureFormat_F_888) &&
    kFormat5551 == static_cast<TextureFormat>(matdef::TextureFormat_F_5551) &&
    kFormat565 == static_cast<TextureFormat>(matdef::TextureFormat_F_565) &&
    kFormatLuminance == static_cast<TextureFormat>(matdef::TextureFormat_F_8) &&
    kFormatASTC == static_cast<TextureFormat>(matdef::TextureFormat_ASTC) &&
    kFormatPKM == static_cast<TextureFormat>(matdef::TextureFormat_PKM) &&
    kFormatKTX == static_cast<TextureFormat>(matdef::TextureFormat_KTX) &&
    kFormatNative == static_cast<TextureFormat>(matdef::TextureFormat_NATIVE),
      "TextureFormat enums in material.h and material.fbs must match.");
static_assert(kFormatCount == kFormatNative + 1,
              "Please update static_assert above with new enum values.");

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

Shader *AssetManager::LoadShaderHelper(const char *basename,
                                       const std::vector<std::string> &defines,
                                       const char *alias, bool should_reload,
                                       bool async) {
  auto shader = FindShader(alias != nullptr ? alias : basename);
  if (!should_reload && shader)
    return shader;
  if (shader) {
    // should_reload must be true.
    shader->Reload(basename, defines);
    return shader;
  } else {
    shader = new Shader(basename, defines, &renderer_);
    return LoadOrQueue(shader, shader_map_, async, alias);
  }
}

Shader *AssetManager::LoadShader(const char *basename,
                                 const std::vector<std::string> &defines,
                                 bool async, const char *alias) {
  return LoadShaderHelper(basename, defines, alias, false /* should_reload */,
                          async);
}

Shader *AssetManager::LoadShader(const char *basename, bool async,
                                 const char *alias) {
  static const std::vector<std::string> empty_defines;
  return LoadShader(basename, empty_defines, async, alias);
}

Shader *AssetManager::ReloadShader(const char *basename,
                                   const std::vector<std::string> &defines,
                                   const char *alias) {
  return LoadShaderHelper(basename, defines, alias, true /* should_reload */,
                          false /* async */);
}

void AssetManager::ResetGlobalShaderDefines(
    const std::vector<std::string> &defines_to_add,
    const std::vector<std::string> &defines_to_omit) {
  defines_to_add_ = defines_to_add;
  defines_to_omit_ = defines_to_omit;
  for (auto iter = shader_map_.begin(); iter != shader_map_.end(); ++iter) {
    iter->second->SetDirty();
  }
}

bool AssetManager::ReloadShaderWithGlobalDefinesIfDirty(Shader *shader) {
  if (shader->IsDirty()) {
    shader->ReloadDefines(defines_to_add_, defines_to_omit_);
    return true;
  }
  return false;
}

void AssetManager::ForEachShaderWithDefine(const char *define,
                                           std::function<void(Shader *)> func) {
  // Use a simple for loop to visit all shaders with 'define' specified, since
  // we only have limited shaders currently. TODO(yifengh): optimize this if
  // there is a growing number of shaders.
  for (auto iter = shader_map_.begin(); iter != shader_map_.end(); ++iter) {
    Shader *shader = iter->second;
    if (shader->program() > 0 && shader->HasDefine(define)) {
      func(shader);
    }
  }
}

Shader *AssetManager::LoadShaderDef(const char *filename) {
  auto shader = FindShader(filename);
  if (shader) return shader;

  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(shaderdef::VerifyShaderBuffer(verifier));
    auto shaderdef = shaderdef::GetShader(flatbuf.c_str());

    shader =
        renderer_.CompileAndLinkShader(shaderdef->vertex_shader()->c_str(),
                                       shaderdef->fragment_shader()->c_str());
    if (shader) {
      shader_map_[filename] = shader;
    } else {
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
      LogError(kError, "%s", renderer_.last_error().c_str());
    }
    return shader;
  }
  LogError(kError, "Can\'t load shader file: %s", filename);
  renderer_.set_last_error(std::string("Couldn\'t load: ") + filename);
  return nullptr;
}

void AssetManager::UnloadShader(const char *filename) {
  auto shader = FindShader(filename);
  if (!shader || shader->DecreaseRefCount()) return;
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

bool AssetManager::TryFinalize() { return loader_.TryFinalize(); }

void AssetManager::UnloadTexture(const char *filename) {
  auto tex = FindTexture(filename);
  if (!tex || tex->DecreaseRefCount()) return;
  texture_map_.erase(filename);
  delete tex;
}

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
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      auto format =
          matdef->desired_format() && i < matdef->desired_format()->size()
              ? static_cast<TextureFormat>(matdef->desired_format()->Get(index))
              : kFormatAuto;
      auto tex = LoadTexture(
          matdef->texture_filenames()->Get(index)->c_str(), format,
          (matdef->mipmaps() ? kTextureFlagsUseMipMaps : kTextureFlagsNone) |
              (matdef->is_cubemap() && matdef->is_cubemap()->Get(index)
                   ? kTextureFlagsIsCubeMap
                   : kTextureFlagsNone) |
              (matdef->wrapmode() == matdef::TextureWrap_CLAMP
                   ? kTextureFlagsClampToEdge
                   : kTextureFlagsNone));
      if (!tex) {
        delete mat;
        return nullptr;
      }
      mat->textures().push_back(tex);
      auto original_size =
          matdef->original_size() && index < matdef->original_size()->size()
              ? LoadVec2i(matdef->original_size()->Get(index))
              : tex->size();
      tex->set_original_size(original_size);

      tex->set_scale(texture_scale_);
    }
    material_map_[filename] = mat;
    return mat;
  }
  renderer_.set_last_error(std::string("Couldn\'t load: ") + filename);
  return nullptr;
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
  mesh = new Mesh(filename, [this](const char *filename) {
    return LoadMaterial(filename);
  });
  return LoadOrQueue(mesh, mesh_map_, async, nullptr /* alias */);
}

void AssetManager::UnloadMesh(const char *filename) {
  auto mesh = FindMesh(filename);
  if (!mesh || mesh->DecreaseRefCount()) return;
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
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(atlasdef::VerifyTextureAtlasBuffer(verifier));
    auto atlasdef = atlasdef::GetTextureAtlas(flatbuf.c_str());
    Texture *atlas_texture =
        LoadTexture(atlasdef->texture_filename()->c_str(), format, flags);
    atlas = new TextureAtlas();
    atlas->set_atlas_texture(atlas_texture);
    for (size_t i = 0; i < atlasdef->entries()->Length(); ++i) {
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      atlas->index_map().insert(std::make_pair(
          atlasdef->entries()->Get(index)->name()->str(), index));
      vec2 size = LoadVec2(atlasdef->entries()->Get(index)->size());
      vec2 location = LoadVec2(atlasdef->entries()->Get(index)->location());
      atlas->subtexture_bounds().push_back(
          vec4(location.x(), location.y(), size.x(), size.y()));
    }
    texture_atlas_map_[filename] = atlas;
    return atlas;
  }
  renderer_.set_last_error(std::string("Couldn\'t load: ") + filename);
  return nullptr;
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
