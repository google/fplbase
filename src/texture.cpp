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

#ifndef FPL_BASE_BACKEND_STDLIB
// Definitions to instantiate STB functions here.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif  // FPL_BASE_BACKEND_STDLIB

#include "fplbase/texture.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "mathfu/glsl_mappings.h"
#include "precompiled.h"
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

struct ASTCHeader {
  uint8_t magic[4];  // 13 ab a1 5c
  uint8_t blockdim_x;
  uint8_t blockdim_y;
  uint8_t blockdim_z;
  uint8_t xsize[3];
  uint8_t ysize[3];
  uint8_t zsize[3];
};

struct PKMHeader {
  char magic[4];          // "PKM "
  char version[2];        // "10"
  uint8_t data_type[2];   // 0 (ETC1_RGB_NO_MIPMAPS)
  uint8_t ext_width[2];   // rounded up to 4 size, big endian.
  uint8_t ext_height[2];  // rounded up to 4 size, big endian.
  uint8_t width[2];       // original, big endian.
  uint8_t height[2];      // original, big endian.
  // Data follows header, size = (ext_width / 4) * (ext_height / 4) * 8
};

struct KTXHeader {
  char id[12];  // "«KTX 11»\r\n\x1A\n"
  uint32_t endian;
  uint32_t type;
  uint32_t type_size;
  uint32_t format;
  uint32_t internal_format;
  uint32_t base_internal_format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_elements;
  uint32_t faces;
  uint32_t mip_levels;
  uint32_t keyvalue_data;
};

// Returns true if the file has a Resource Interchange File Format (RIFF) header
// whose first chunk has a WEBP FOURCC. This will not check chunks other than
// the first one. https://developers.google.com/speed/webp/docs/riff_container
static bool HasWebpHeader(const std::string &file) {
  return file.size() > 12 && file.substr(0, 4) == "RIFF" &&
         file.substr(8, 4) == "WEBP";
}

Texture::Texture(const char *filename, TextureFormat format, TextureFlags flags)
: AsyncAsset(filename ? filename : ""),
  id_(0),
  size_(mathfu::kZeros2i),
  original_size_(mathfu::kZeros2i),
  scale_(mathfu::kOnes2f),
  texture_format_(kFormat888),
  target_(flags & kTextureFlagsIsCubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D),
  desired_(format),
  flags_(flags) {}

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
  id_ = CreateTexture(data, size_, texture_format_, desired_, flags_);
}

void Texture::Finalize() {
  if (data_) {
    id_ = CreateTexture(data_, size_, texture_format_, desired_, flags_);
    free(const_cast<uint8_t *>(data_));
    data_ = nullptr;
  }
  CallFinalizeCallback();
}

void Texture::Set(size_t unit, RenderContext *) {
  GL_CALL(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit)));
  GL_CALL(glBindTexture(target_, id_));
}

void Texture::Set(size_t unit) { Set(unit, nullptr); }

void Texture::Set(size_t unit, RenderContext *) const {
  const_cast<Texture *>(this)->Set(unit);
}

void Texture::Set(size_t unit) const { const_cast<Texture *>(this)->Set(unit); }

void Texture::Delete() {
  if (id_) {
    GL_CALL(glDeleteTextures(1, &id_));
    id_ = 0;
  }
}

uint16_t *Texture::Convert8888To5551(const uint8_t *buffer, const vec2i &size) {
  auto buffer16 = new uint16_t[size.x() * size.y()];
  for (int i = 0; i < size.x() * size.y(); i++) {
    auto c = &buffer[i * 4];
    buffer16[i] = ((c[0] >> 3) << 11) | ((c[1] >> 3) << 6) |
                  ((c[2] >> 3) << 1) | ((c[3] >> 7) << 0);
  }
  return buffer16;
}

uint16_t *Texture::Convert888To565(const uint8_t *buffer, const vec2i &size) {
  auto buffer16 = new uint16_t[size.x() * size.y()];
  for (int i = 0; i < size.x() * size.y(); i++) {
    auto c = &buffer[i * 3];
    buffer16[i] = ((c[0] >> 3) << 11) | ((c[1] >> 2) << 5) | ((c[2] >> 3) << 0);
  }
  return buffer16;
}

void Texture::SetTextureId(TextureTarget target, TextureHandle id) {
  target_ = target;
  id_ = id;
}

GLuint Texture::CreateTexture(const uint8_t *buffer, const vec2i &size,
                              TextureFormat texture_format,
                              TextureFormat desired, TextureFlags flags) {
  GLenum tex_type = GL_TEXTURE_2D;
  GLenum tex_imagetype = GL_TEXTURE_2D;
  int tex_num_faces = 1;
  auto tex_size = size;

  if (flags & kTextureFlagsIsCubeMap) {
    tex_type = GL_TEXTURE_CUBE_MAP;
    tex_imagetype = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    tex_num_faces = 6;
    tex_size = size / vec2i(1, tex_num_faces);
    if (tex_size.x() != tex_size.y()) {
      LogError(kError, "CreateTexture: cubemap not in 1x6 format: (%d,%d)",
               size.x(), size.y());
    }
  }

  if (!Renderer::Get()->SupportsTextureNpot()) {
    // Npot textures are supported in ES 2.0 if you use GL_CLAMP_TO_EDGE and no
    // mipmaps. See Section 3.8.2 of ES2.0 spec:
    // https://www.khronos.org/registry/gles/specs/2.0/es_full_spec_2.0.25.pdf
    if (flags & kTextureFlagsUseMipMaps ||
        !(flags & kTextureFlagsClampToEdge)) {
      int area = tex_size.x() * tex_size.y();
      if (area & (area - 1)) {
        LogError(kError, "CreateTexture: not power of two in size: (%d,%d)",
                 tex_size.x(), tex_size.y());
        return 0;
      }
    }
  }

  bool generate_mips = (flags & kTextureFlagsUseMipMaps) != 0;
  bool have_mips = generate_mips;

  if (generate_mips && IsCompressed(texture_format)) {
    if (texture_format == kFormatKTX) {
      const auto &header = *reinterpret_cast<const KTXHeader *>(buffer);
      have_mips = (header.mip_levels > 1);
    } else {
      have_mips = false;
    }

    if (!have_mips) {
      LogError(kError, "Can't generate mipmaps for compressed textures");
    }
    generate_mips = false;
  }

  // In some Android devices (particulary Galaxy Nexus), there is an issue
  // of glGenerateMipmap() with 16BPP texture format.
  // In that case, we are going to fallback to 888/8888 textures
  const bool use_16bpp = MipmapGeneration16bppSupported();
  const GLint wrap_mode =
      flags & kTextureFlagsClampToEdge ? GL_CLAMP_TO_EDGE : GL_REPEAT;

  // TODO(wvo): support default args for mipmap/wrap/trilinear
  GLuint texture_id;
  GL_CALL(glGenTextures(1, &texture_id));
  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(tex_type, texture_id));
  GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, wrap_mode));
  GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, wrap_mode));
  if (flags & kTextureFlagsIsCubeMap) {
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_R, wrap_mode));
  }
  GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER,
                          have_mips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  auto format = GL_RGBA;
  auto type = GL_UNSIGNED_BYTE;
  if (desired == kFormatAuto) {
    desired = IsCompressed(texture_format)
                  ? texture_format
                  : HasAlpha(texture_format) ? kFormat5551 : kFormat565;
  } else if (desired == kFormatNative) {
    desired = texture_format;
  }

  auto gl_tex_image = [&](const uint8_t *buf, const vec2i &mip_size,
                          int mip_level, int buf_size, bool compressed) {
    for (int i = 0; i < tex_num_faces; i++) {
      if (compressed) {
        GL_CALL(glCompressedTexImage2D(tex_imagetype + i, mip_level, format,
                                       mip_size.x(), mip_size.y(), 0, buf_size,
                                       buf));
      } else {
        GL_CALL(glTexImage2D(tex_imagetype + i, mip_level, format, mip_size.x(),
                             mip_size.y(), 0, format, type, buf));
      }
      if (buf) buf += buf_size;
    }
  };

  int num_pixels = tex_size.x() * tex_size.y();

  switch (desired) {
    case kFormat5551: {
      switch (texture_format) {
        case kFormat8888:
          if (use_16bpp) {
            auto buffer16 = Convert8888To5551(buffer, size);
            type = GL_UNSIGNED_SHORT_5_5_5_1;
            gl_tex_image(reinterpret_cast<const uint8_t *>(buffer16), tex_size,
                         0, num_pixels * 2, false);
            delete[] buffer16;
          } else {
            // Fallback to 8888
            gl_tex_image(buffer, tex_size, 0, num_pixels * 4, false);
          }
          break;
        case kFormat5551:
          // Nothing coversion.
          gl_tex_image(buffer, tex_size, 0, num_pixels * 2, false);
          break;
        default:
          // This conversion not supported yet.
          assert(false);
          break;
      }
      break;
    }
    case kFormat565: {
      format = GL_RGB;
      switch (texture_format) {
        case kFormat888:
          if (use_16bpp) {
            auto buffer16 = Convert888To565(buffer, size);
            type = GL_UNSIGNED_SHORT_5_6_5;
            gl_tex_image(reinterpret_cast<const uint8_t *>(buffer16), tex_size,
                         0, num_pixels * 2, false);
            delete[] buffer16;
          } else {
            // Fallback to 888
            gl_tex_image(buffer, tex_size, 0, num_pixels * 3, false);
          }
          break;
        case kFormat565:
          // No conversion.
          type = GL_UNSIGNED_SHORT_5_6_5;
          gl_tex_image(buffer, tex_size, 0, num_pixels * 2, false);
          break;
        default:
          // This conversion not supported yet.
          assert(false);
          break;
      }
      break;
    }
    case kFormat8888: {
      assert(texture_format == kFormat8888);
      gl_tex_image(buffer, tex_size, 0, num_pixels * 4, false);
      break;
    }
    case kFormat888: {
      assert(texture_format == kFormat888);
      format = GL_RGB;
      gl_tex_image(buffer, tex_size, 0, num_pixels * 3, false);
      break;
    }
    case kFormatLuminance: {
      assert(texture_format == kFormatLuminance);
      format = GL_LUMINANCE;
      gl_tex_image(buffer, tex_size, 0, num_pixels, false);
      break;
    }
    case kFormatASTC: {
      assert(texture_format == kFormatASTC);
      auto &header = *reinterpret_cast<const ASTCHeader *>(buffer);
      auto xblocks = (size.x() + header.blockdim_x - 1) / header.blockdim_x;
      auto yblocks = (size.y() + header.blockdim_y - 1) / header.blockdim_y;
      auto zblocks = (1 + header.blockdim_z - 1) / header.blockdim_z;
      auto data_size = xblocks * yblocks * zblocks << 4;
      // Convert the block dimensions into the correct GL constant.
      switch (header.blockdim_x) {
        case 4:
          assert(header.blockdim_y == 4);
          format = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
          break;
        case 5:
          if (header.blockdim_y == 4) {
            format = GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
          } else {
            assert(header.blockdim_y == 5);
            format = GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
          }
          break;
        case 6:
          if (header.blockdim_y == 5) {
            format = GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
          } else {
            assert(header.blockdim_y == 6);
            format = GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
          }
          break;
        case 8:
          if (header.blockdim_y == 5) {
            format = GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
          } else if (header.blockdim_y == 6) {
            format = GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
          } else {
            assert(header.blockdim_y == 8);
            format = GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
          }
          break;
        case 10:
          if (header.blockdim_y == 5) {
            format = GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
          } else if (header.blockdim_y == 6) {
            format = GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
          } else if (header.blockdim_y == 8) {
            format = GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
          } else {
            assert(header.blockdim_y == 10);
            format = GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
          }
          break;
        case 12:
          if (header.blockdim_y == 10) {
            format = GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
          } else {
            assert(header.blockdim_y == 12);
            format = GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
          }
          break;
        default:
          assert(false);
      }
      // TODO(wvo): cubemaps in ASTC may not work for block sizes that straddle
      // the face boundaries.
      gl_tex_image(buffer + sizeof(ASTCHeader), tex_size, 0,
                   data_size / tex_num_faces, true);
      break;
    }
    case kFormatPKM: {
      assert(texture_format == kFormatPKM);
      auto &header = *reinterpret_cast<const PKMHeader *>(buffer);
      auto ext_xsize = (header.ext_width[0] << 8) | header.ext_width[1];
      auto ext_ysize = (header.ext_height[0] << 8) | header.ext_height[1];
      auto data_size = (ext_xsize / 4) * (ext_ysize / 4) * 8;
      format = GL_COMPRESSED_RGB8_ETC2;
      gl_tex_image(buffer + sizeof(PKMHeader), tex_size, 0,
                   data_size / tex_num_faces, true);
      break;
    }
    case kFormatKTX: {
      assert(texture_format == kFormatKTX);
      auto &header = *reinterpret_cast<const KTXHeader *>(buffer);
      format = header.internal_format;
      auto data = buffer + sizeof(KTXHeader);
      auto cur_size = tex_size;
      for (uint32_t i = 0; i < header.mip_levels; i++) {
        auto data_size = *(reinterpret_cast<const int32_t *>(data));
        data += sizeof(int32_t);
        // Keep loading mip data even if one of our calculated dimensions goes
        // to 0, but maintain a min size of 1.  This is needed to get non-square
        // mip chains to work using ETC2 (eg a 256x512 needs 10 mips defined).
        gl_tex_image(data, vec2i::Max(mathfu::kOnes2i, cur_size), i,
            data_size / tex_num_faces, true);
        cur_size /= 2;
        data += data_size;
        // Guard against extra mip levels in the ktx.
        if (!cur_size.x() && !cur_size.y()) {
          LogError("KTX file has too many mips: %dx%d, %d mips",
                   tex_size.x(), tex_size.y(), header.mip_levels);
          break;
        }
        // If the file has mips but the caller doesn't want them, stop here.
        if (!have_mips) break;
      }
      break;
    }
    default:
      assert(false);
  }

  if (generate_mips) {
    // Work around for some Android devices to correctly generate miplevels.
    auto min_dimension =
        static_cast<float>(std::min(tex_size.x(), tex_size.y()));
    auto levels = ceil(log(min_dimension) / log(2.0f));
    auto mip_size = tex_size / 2;
    for (auto i = 1; i < levels; ++i) {
      gl_tex_image(nullptr, mip_size, i, 0, false);
      mip_size /= 2;
    }

    GL_CALL(glGenerateMipmap(tex_type));
  } else if (have_mips && IsCompressed(texture_format)) {
    // At least on Linux, there appears to be a bug with uploading pre-made
    // compressed mipmaps that makes the texture not show up if
    // glGenerateMipmap isn't called, even though glGenerateMipmap can't
    // generate any mipmaps for compressed textures.
    // Also, this call should generate GL_INVALID_OPERATION but it doesn't?
    // TODO(wvo): is this a driver bug, or what is the root cause of this?
    GL_CALL(glGenerateMipmap(tex_type));
  }
  return texture_id;
}

void Texture::UpdateTexture(TextureFormat format, int xoffset, int yoffset,
                            int width, int height, const void *data) {
  // In OpenGL ES2.0, width and pitch of the src buffer needs to match. So
  // that we are updating entire row at once.
  // TODO(wvo): Optimize glTexSubImage2D call in ES3.0 capable platform.
  auto texture_format = GL_RGBA;
  auto pixel_format = GL_UNSIGNED_BYTE;
  switch (format) {
    case kFormatLuminance:
      texture_format = GL_LUMINANCE;
      break;
    case kFormat888:
      texture_format = GL_RGB;
      break;
    case kFormat5551:
      pixel_format = GL_UNSIGNED_SHORT_5_5_5_1;
      break;
    case kFormat565:
      pixel_format = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case kFormat8888:
      break;
    default:
      assert(false);  // TODO(wvo): not implemented.
  }
  GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height,
                          texture_format, pixel_format, data));
}

uint8_t *Texture::UnpackTGA(const void *tga_buf, TextureFlags flags,
                            vec2i *dimensions, TextureFormat *texture_format) {
  if (flags & kTextureFlagsPremultiplyAlpha) {
    LogError(kApplication, "Premultipled alpha not supported for TGA\n");
  }
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
  if (scale.x() != 1.0f || scale.y() != 1.0f) {
    config.options.use_scaling = true;
    config.options.scaled_width =
        static_cast<int>(config.input.width * scale.x());
    config.options.scaled_height =
        static_cast<int>(config.input.height * scale.y());
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
  if (v != 0 || header.endian != 0x04030201 || header.depth != 0 ||
      header.faces != 1 || header.keyvalue_data != 0)
    return nullptr;

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
  if (flags & kTextureFlagsPremultiplyAlpha) {
    LogError(kApplication, "Premultipled alpha not supported for this image");
  }
  uint8_t *image = nullptr;
  int width = 0;
  int height = 0;

  int32_t channels;
  // STB has it's own format detection code inside stbi_load_from_memory.
  image = stbi_load_from_memory(static_cast<stbi_uc const *>(img_buf),
                                static_cast<int>(size), &width, &height,
                                &channels, 0);

  if (image && (scale.x() != 1.0f || scale.y() != 1.0f)) {
    // Scale the image.
    int32_t new_width = static_cast<int32_t>(width * scale.x());
    int32_t new_height = static_cast<int32_t>(height * scale.y());
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
    if (Renderer::Get()->SupportsTextureFormat(kFormatASTC) &&
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
    if (Renderer::Get()->SupportsTextureFormat(kFormatPKM) &&
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
    if (Renderer::Get()->SupportsTextureFormat(kFormatKTX) &&
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

}  // namespace fplbase
