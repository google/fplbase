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
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/material.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "materials_generated.h"

namespace fplbase {

static_assert(
    kBlendModeOff == static_cast<BlendMode>(matdef::BlendMode_OFF) &&
    kBlendModeTest == static_cast<BlendMode>(matdef::BlendMode_TEST) &&
    kBlendModeAlpha == static_cast<BlendMode>(matdef::BlendMode_ALPHA) &&
    kBlendModeAdd == static_cast<BlendMode>(matdef::BlendMode_ADD) &&
    kBlendModeAddAlpha == static_cast<BlendMode>(matdef::BlendMode_ADDALPHA) &&
    kBlendModeMultiply == static_cast<BlendMode>(matdef::BlendMode_MULTIPLY) &&
    kBlendModePreMultipliedAlpha ==
        static_cast<BlendMode>(matdef::BlendMode_PREMULTIPLIEDALPHA),
    "BlendMode enums in material.h and material.fbs must match.");
static_assert(kBlendModeCount == kBlendModePreMultipliedAlpha + 2,
              "Please update static_assert above with new enum values.");

static_assert(
    kFormatAuto == static_cast<TextureFormat>(matdef::TextureFormat_AUTO) &&
    kFormat8888 == static_cast<TextureFormat>(matdef::TextureFormat_F_8888) &&
    kFormat888 == static_cast<TextureFormat>(matdef::TextureFormat_F_888) &&
    kFormat5551 == static_cast<TextureFormat>(matdef::TextureFormat_F_5551) &&
    kFormat565 == static_cast<TextureFormat>(matdef::TextureFormat_F_565) &&
    kFormatLuminance == static_cast<TextureFormat>(matdef::TextureFormat_F_8) &&
    kFormatLuminanceAlpha ==
        static_cast<TextureFormat>(matdef::TextureFormat_F_88) &&
    kFormatASTC == static_cast<TextureFormat>(matdef::TextureFormat_ASTC) &&
    kFormatPKM == static_cast<TextureFormat>(matdef::TextureFormat_PKM) &&
    kFormatKTX == static_cast<TextureFormat>(matdef::TextureFormat_KTX) &&
    kFormatNative == static_cast<TextureFormat>(matdef::TextureFormat_NATIVE),
      "TextureFormat enums in material.h and materials.fbs must match.");
static_assert(kFormatCount == kFormatLuminanceAlpha + 1,
              "Please update static_assert above with new enum values.");

void Material::Set(Renderer &renderer) {
  renderer.SetBlendMode(blend_mode_);
  for (size_t i = 0; i < textures_.size(); i++) textures_[i]->Set(i);
}

void Material::DeleteTextures() {
  for (size_t i = 0; i < textures_.size(); i++) textures_[i]->Delete();
}

Material *Material::LoadFromMaterialDef(const matdef::Material *matdef,
                                        const TextureLoaderFn &tlf) {
  if (matdef) {
    auto mat = new Material();
    mat->set_blend_mode(static_cast<BlendMode>(matdef->blendmode()));
    for (size_t i = 0; i < matdef->texture_filenames()->size(); i++) {
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      auto format =
          matdef->desired_format() && i < matdef->desired_format()->size()
              ? static_cast<TextureFormat>(matdef->desired_format()->Get(index))
              : kFormatAuto;
      auto tex = tlf(
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
    }
    return mat;
  }
  return nullptr;
}

Material *Material::LoadFromMaterialDef(const char *filename,
                                        const TextureLoaderFn &tlf) {
  const matdef::Material *def = nullptr;
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(matdef::VerifyMaterialBuffer(verifier));
    def = matdef::GetMaterial(flatbuf.c_str());
  }
  Material *mat = LoadFromMaterialDef(def, tlf);
  if (!mat) {
    RendererBase::Get()->set_last_error(std::string("Couldn\'t load: ")
                                      + filename);
  } else {
    mat->set_filename(filename);
  }
  return mat;
}

}  // namespace fplbase
