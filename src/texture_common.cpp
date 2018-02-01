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
#include "fplbase/renderer.h"
#include "fplbase/texture.h"
#include "fplbase/texture_atlas.h"
#include "fplbase/utilities.h"
#include "mathfu/glsl_mappings.h"
#include "texture_atlas_generated.h"
#include "texture_headers.h"
#include "webp/decode.h"

// STB_image to resize PNG/JPG images.
// Disable warnings in STB_image_resize.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)  // unused reference
#pragma warning(disable : 4244)  // conversion possible loss of data
#pragma warning(disable : 4189)  // local variable not referenced
#pragma warning(disable : 4702)  // unreachable code
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif /* _MSC_VER */

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#include "stb_image.h"
#include "stb_image_resize.h"

// Pop warning status.
#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

using mathfu::vec2;
using mathfu::vec2i;

namespace fplbase {

// Returns true if the file has a Resource Interchange File Format (RIFF) header
// whose first chunk has a WEBP FOURCC. This will not check chunks other than
// the first one. https://developers.google.com/speed/webp/docs/riff_container
static bool HasWebpHeader(const std::string &file) {
  return file.size() > 12 && file.substr(0, 4) == "RIFF" &&
         file.substr(8, 4) == "WEBP";
}

static void MultiplyRgbByAlpha(uint8_t *rgba_ptr, int width, int height) {
  const int num_pixels = width * height;
  for (int i = 0; i < num_pixels; ++i, rgba_ptr += 4) {
    const auto alpha = static_cast<uint16_t>(rgba_ptr[3]);
    rgba_ptr[0] = static_cast<uint8_t>(
        (static_cast<uint16_t>(rgba_ptr[0]) * alpha) / 255);
    rgba_ptr[1] = static_cast<uint8_t>(
        (static_cast<uint16_t>(rgba_ptr[1]) * alpha) / 255);
    rgba_ptr[2] = static_cast<uint8_t>(
        (static_cast<uint16_t>(rgba_ptr[2]) * alpha) / 255);
  }
}

Texture::Texture(const char *filename, TextureFormat format, TextureFlags flags)
    : AsyncAsset(filename ? filename : ""),
      impl_(CreateTextureImpl()),
      id_(InvalidTextureHandle()),
      size_(mathfu::kZeros2i),
      original_size_(mathfu::kZeros2i),
      scale_(mathfu::kOnes2f),
      texture_format_(kFormat888),
      target_(TextureTargetFromFlags(flags)),
      desired_(format),
      flags_(flags),
      is_external_(false) {}

Texture::~Texture() {
  if (data_) {
    free(const_cast<uint8_t *>(data_));
    data_ = nullptr;
  }

  Delete();
  DestroyTextureImpl(impl_);
}

void Texture::Load() {
  data_ = LoadAndUnpackTexture(filename_.c_str(), scale_, flags_, &size_,
                               &texture_format_);
  SetOriginalSizeIfNotYetSet(size_);
}

void Texture::LoadFromMemory(const uint8_t *data, const vec2i &size,
                             TextureFormat texture_format) {
  size_ = size;
  SetOriginalSizeIfNotYetSet(size_);
  texture_format_ = texture_format;
  id_ = CreateTexture(data, size_, texture_format_, desired_, flags_, impl_);
  is_external_ = false;
}

bool Texture::Finalize() {
  if (data_) {
    id_ = CreateTexture(data_, size_, texture_format_, desired_, flags_, impl_);
    is_external_ = false;
    free(const_cast<uint8_t *>(data_));
    data_ = nullptr;
  }
  CallFinalizeCallback();
  return ValidTextureHandle(id_);
}

void Texture::Set(size_t unit) { Set(unit, nullptr); }

void Texture::Set(size_t unit, Renderer *) const {
  const_cast<Texture *>(this)->Set(unit);
}

void Texture::Set(size_t unit) const { const_cast<Texture *>(this)->Set(unit); }

uint16_t *Texture::Convert8888To5551(const uint8_t *buffer, const vec2i &size) {
  auto buffer16 = new uint16_t[size.x * size.y];
  for (int i = 0; i < size.x * size.y; i++) {
    auto c = &buffer[i * 4];
    buffer16[i] = ((c[0] >> 3) << 11) | ((c[1] >> 3) << 6) |
                  ((c[2] >> 3) << 1) | ((c[3] >> 7) << 0);
  }
  return buffer16;
}

uint16_t *Texture::Convert888To565(const uint8_t *buffer, const vec2i &size) {
  auto buffer16 = new uint16_t[size.x * size.y];
  for (int i = 0; i < size.x * size.y; i++) {
    auto c = &buffer[i * 3];
    buffer16[i] = ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | ((c[2] >> 3) << 0);
  }
  return buffer16;
}

void Texture::SetTextureId(TextureTarget target, TextureHandle id) {
  target_ = target;
  id_ = id;
  is_external_ = true;
}

uint8_t *Texture::UnpackTGA(const void *tga_buf, TextureFlags flags,
                            vec2i *dimensions, TextureFormat *texture_format) {
  struct TGA {
    uint8_t id_len, color_map_type, image_type, color_map_data[5];
    uint16_t x_origin, y_origin, width, height;
    uint8_t bpp, image_descriptor;
  };
  static_assert(sizeof(TGA) == 18,
                "Members of struct TGA need to be packed with no padding.");
  auto header = reinterpret_cast<const TGA *>(tga_buf);
  int size = header->id_len + header->width * header->height * header->bpp / 8;
  return UnpackImage(tga_buf, size, mathfu::kOnes2f, flags, dimensions,
                     texture_format);
}

uint8_t *Texture::UnpackWebP(const void *webp_buf, size_t size,
                             const vec2 &scale, TextureFlags flags,
                             vec2i *dimensions, TextureFormat *texture_format) {
  WebPDecoderConfig config;
  memset(&config, 0, sizeof(WebPDecoderConfig));
  auto status = WebPGetFeatures(static_cast<const uint8_t *>(webp_buf), size,
                                &config.input);
  if (status != VP8_STATUS_OK) return nullptr;

  // Apply scaling.
  if (scale.x != 1.0f || scale.y != 1.0f) {
    config.options.use_scaling = true;
    config.options.scaled_width =
        static_cast<int>(config.input.width * scale.x);
    config.options.scaled_height =
        static_cast<int>(config.input.height * scale.y);
  }

  if (config.input.has_alpha) {
    if (flags & kTextureFlagsPremultiplyAlpha) {
      config.output.colorspace = MODE_rgbA;
    } else {
      config.output.colorspace = MODE_RGBA;
    }
  }
  status = WebPDecode(static_cast<const uint8_t *>(webp_buf), size, &config);
  if (status != VP8_STATUS_OK) return nullptr;

  *dimensions = vec2i(config.output.width, config.output.height);
  *texture_format = config.input.has_alpha != 0 ? kFormat8888 : kFormat888;
  return config.output.private_memory;  // Allocated with malloc by webp.
}

uint8_t *Texture::UnpackASTC(const void *astc_buf, size_t size,
                             TextureFlags flags, vec2i *dimensions,
                             TextureFormat *texture_format) {
  if (flags & kTextureFlagsPremultiplyAlpha) {
    LogError(kApplication, "Premultipled alpha not supported for ASTC");
  }
  auto &header = *reinterpret_cast<const ASTCHeader *>(astc_buf);
  static const uint8_t magic[] = {0x13, 0xab, 0xa1, 0x5c};
  if (memcmp(header.magic, magic, sizeof(magic))) return nullptr;

  auto xsize =
      header.xsize[0] | (header.xsize[1] << 8) | (header.xsize[2] << 16);
  auto ysize =
      header.ysize[0] | (header.ysize[1] << 8) | (header.ysize[2] << 16);
  auto zsize =
      header.zsize[0] | (header.zsize[1] << 8) | (header.zsize[2] << 16);

  // TODO(wvo): Our pipeline currently doesn't support 3D textures.
  if (zsize != 1) return nullptr;

  *dimensions = vec2i(xsize, ysize);
  *texture_format = kFormatASTC;

  // TODO(wvo): This in theory doesn't need to be copied, but it keeps the API
  // uniform, and should not affect load times.
  // We use malloc to ensure that all unpacked texture formats can be freed
  // in the same way (see also other Unpack* functions).
  auto buf = reinterpret_cast<uint8_t *>(malloc(size));
  memcpy(buf, astc_buf, size);
  return buf;
}

uint8_t *Texture::UnpackPKM(const void *file_buf, size_t size,
                            TextureFlags flags, vec2i *dimensions,
                            TextureFormat *texture_format) {
  if (flags & kTextureFlagsPremultiplyAlpha) {
    LogError(kApplication, "Premultipled alpha not supported for PKM");
  }
  auto &header = *reinterpret_cast<const PKMHeader *>(file_buf);
  if (strncmp(header.magic, "PKM ", 4) && strncmp(header.version, "10", 2))
    return nullptr;

  auto xsize = (header.width[0] << 8) | header.width[1];  // Big endian!
  auto ysize = (header.height[0] << 8) | header.height[1];
  *dimensions = vec2i(xsize, ysize);
  *texture_format = kFormatPKM;

  // TODO(wvo): This in theory doesn't need to be copied, but it keeps the API
  // uniform, and should not affect load times.
  // We use malloc to ensure that all unpacked texture formats can be freed
  // in the same way (see also other Unpack* functions).
  auto buf = reinterpret_cast<uint8_t *>(malloc(size));
  memcpy(buf, file_buf, size);
  return buf;
}

uint8_t *Texture::UnpackKTX(const void *file_buf, size_t size,
                            TextureFlags flags, vec2i *dimensions,
                            TextureFormat *texture_format) {
  if (flags & kTextureFlagsPremultiplyAlpha) {
    LogError(kApplication, "Premultipled alpha not supported for KTX");
  }
  auto &header = *reinterpret_cast<const KTXHeader *>(file_buf);
  auto magic = "\xABKTX 11\xBB\r\n\x1A\n";
  auto v = memcmp(header.id, magic, sizeof(header.id));
  // Note: a single Nx6N face and six NxN faces are both valid cubemaps
  bool valid_face_count =
      (flags & kTextureFlagsIsCubeMap)
          ? (header.faces == 6 && header.width == header.height) ||
                (header.faces == 1 && header.width * 6 == header.height)
          : (header.faces == 1);
  if (v != 0 || header.endian != 0x04030201 || header.depth != 0 ||
      !valid_face_count) {
    return nullptr;
  }

  *dimensions = vec2i(header.width, header.height);
  *texture_format = kFormatKTX;

  // TODO(wvo): This in theory doesn't need to be copied, but it keeps the API
  // uniform, and should not affect load times.
  // We use malloc to ensure that all unpacked texture formats can be freed
  // in the same way (see also other Unpack* functions).
  auto buf = reinterpret_cast<uint8_t *>(malloc(size));
  memcpy(buf, file_buf, size);
  return buf;
}

uint8_t *Texture::UnpackImage(const void *img_buf, size_t size,
                              const vec2 &scale, TextureFlags flags,
                              vec2i *dimensions,
                              TextureFormat *texture_format) {
  uint8_t *image = nullptr;
  int width = 0;
  int height = 0;

  int32_t channels;
  // STB has it's own format detection code inside stbi_load_from_memory.
  image = stbi_load_from_memory(static_cast<stbi_uc const *>(img_buf),
                                static_cast<int>(size), &width, &height,
                                &channels, 0);

  if (image && (scale.x != 1.0f || scale.y != 1.0f)) {
    // Scale the image.
    int32_t new_width = static_cast<int32_t>(width * scale.x);
    int32_t new_height = static_cast<int32_t>(height * scale.y);
    uint8_t *new_image =
        static_cast<uint8_t *>(malloc(new_width * new_height * channels));
    stbir_resize_uint8(image, width, height, 0, new_image, new_width,
                       new_height, 0, channels);
    stbi_image_free(image);
    image = new_image;
    width = new_width;
    height = new_height;
  }

  *dimensions = vec2i(width, height);
  if (channels == 4) {
    if (flags & kTextureFlagsPremultiplyAlpha) {
      MultiplyRgbByAlpha(image, width, height);
    }

    *texture_format = kFormat8888;
  } else if (channels == 3) {
    *texture_format = kFormat888;
  } else if (channels == 1) {
    *texture_format = kFormatLuminance;
  } else {
    assert(0);
  }
  return image;
}

uint8_t *Texture::LoadAndUnpackTexture(const char *filename, const vec2 &scale,
                                       TextureFlags flags, vec2i *dimensions,
                                       TextureFormat *texture_format) {
  std::string ext;
  std::string basename = filename;
  size_t ext_pos = basename.find_last_of(".");
  if (ext_pos != std::string::npos) {
    ext = basename.substr(ext_pos + 1);
    basename = basename.substr(0, ext_pos);
  }

  std::string file;

  // Try to load ASTC, but default to WebP if not available or not supported.
  if (ext == "astc") {
    if (RendererBase::Get()->SupportsTextureFormat(kFormatASTC) &&
        LoadFile(filename, &file)) {
      auto buf = UnpackASTC(file.c_str(), file.length(), flags, dimensions,
                            texture_format);
      if (!buf) LogError(kApplication, "ASTC format problem: %s", filename);
      return buf;
    } else {
      ext = "webp";
    }
  }

  // Try to load PKM, but default to WebP if not available or not supported.
  if (ext == "pkm") {
    if (RendererBase::Get()->SupportsTextureFormat(kFormatPKM) &&
        LoadFile(filename, &file)) {
      auto buf = UnpackPKM(file.c_str(), file.length(), flags, dimensions,
                           texture_format);
      if (!buf) LogError(kApplication, "PKM format problem: %s", filename);
      return buf;
    } else {
      ext = "webp";
    }
  }

  // Try to load KTX, but default to WebP if not available or not supported.
  if (ext == "ktx") {
    if (RendererBase::Get()->SupportsTextureFormat(kFormatKTX) &&
        LoadFile(filename, &file)) {
      auto buf = UnpackKTX(file.c_str(), file.length(), flags, dimensions,
                           texture_format);
      if (!buf) LogError(kApplication, "KTX format problem: %s", filename);
      return buf;
    } else {
      ext = "webp";
    }
  }

  std::string altfilename = basename;
  if (ext.length()) altfilename += "." + ext;

  if (!LoadFile(altfilename.c_str(), &file)) {
    LogError(kApplication, "Couldn\'t load: %s", filename);
    return nullptr;
  }

  if (ext == "tga" || ext == "png" || ext == "jpg") {
    auto buf = UnpackImage(file.c_str(), file.length(), scale, flags,
                           dimensions, texture_format);
    if (!buf) LogError(kApplication, "Image format problem: %s", filename);
    return buf;
  } else if (ext == "webp" || HasWebpHeader(file)) {
    auto buf = UnpackWebP(file.c_str(), file.length(), scale, flags, dimensions,
                          texture_format);
    if (!buf) LogError(kApplication, "WebP format problem: %s", filename);
    return buf;
  } else {
    LogError(kApplication, "Can\'t figure out file type from extension: %s",
             filename);
    return nullptr;
  }
}

TextureAtlas *TextureAtlas::LoadTextureAtlas(const char *filename,
                                             TextureFormat format,
                                             TextureFlags flags,
                                             const TextureLoaderFn &tlf) {
  std::string flatbuf;
  if (LoadFile(filename, &flatbuf)) {
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t *>(flatbuf.c_str()), flatbuf.length());
    assert(atlasdef::VerifyTextureAtlasBuffer(verifier));
    auto atlasdef = atlasdef::GetTextureAtlas(flatbuf.c_str());
    Texture *atlas_texture =
        tlf(atlasdef->texture_filename()->c_str(), format, flags);
    auto atlas = new TextureAtlas();
    atlas->set_atlas_texture(atlas_texture);
    for (size_t i = 0; i < atlasdef->entries()->Length(); ++i) {
      flatbuffers::uoffset_t index = static_cast<flatbuffers::uoffset_t>(i);
      atlas->index_map().insert(std::make_pair(
          atlasdef->entries()->Get(index)->name()->str(), index));
      vec2 size = LoadVec2(atlasdef->entries()->Get(index)->size());
      vec2 location = LoadVec2(atlasdef->entries()->Get(index)->location());
      atlas->subtexture_bounds().push_back(
          vec4(location.x, location.y, size.x, size.y));
    }
    return atlas;
  }
  RendererBase::Get()->set_last_error(std::string("Couldn\'t load: ") +
                                      filename);
  return nullptr;
}

}  // namespace fplbase
