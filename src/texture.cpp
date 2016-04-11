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

#include "fplbase/texture.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "mathfu/glsl_mappings.h"
#include "precompiled.h"
#include "webp/decode.h"

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

void Texture::Load() {
  data_ =
      LoadAndUnpackTexture(filename_.c_str(), scale_, &size_, &texture_format_);
  SetOriginalSizeIfNotYetSet(size_);
}

void Texture::LoadFromMemory(const uint8_t *data, const vec2i &size,
                             TextureFormat texture_format) {
  size_ = size;
  SetOriginalSizeIfNotYetSet(size_);
  texture_format_ = texture_format;
  id_ = CreateTexture(data, size_, texture_format_, mipmaps_, desired_);
}

void Texture::Finalize() {
  if (data_) {
    id_ = CreateTexture(data_, size_, texture_format_, mipmaps_, desired_);
    free(const_cast<uint8_t *>(data_));
    data_ = nullptr;
  }
}

void Texture::Set(size_t unit) {
  GL_CALL(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit)));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, id_));
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

GLuint Texture::CreateTexture(const uint8_t *buffer, const vec2i &size,
                              TextureFormat texture_format, bool mipmaps,
                              TextureFormat desired) {
  int area = size.x() * size.y();
  if (area & (area - 1)) {
    LogError(kError, "CreateTexture: not power of two in size: (%d,%d)",
             size.x(), size.y());
    return 0;
  }

  // In some Android devices (particulary Galaxy Nexus), there is an issue
  // of glGenerateMipmap() with 16BPP texture format.
  // In that case, we are going to fallback to 888/8888 textures
  const bool use_16bpp = MipmapGeneration16bppSupported();

  // TODO(wvo): support default args for mipmap/wrap/trilinear
  GLuint texture_id;
  GL_CALL(glGenTextures(1, &texture_id));
  GL_CALL(glActiveTexture(GL_TEXTURE0));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, texture_id));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));

  auto format = GL_RGBA;
  auto type = GL_UNSIGNED_BYTE;
  if (desired == kFormatAuto) {
    desired = IsCompressed(texture_format)
                  ? texture_format
                  : HasAlpha(texture_format) ? kFormat5551 : kFormat565;
  }
  switch (desired) {
    case kFormat5551: {
      switch (texture_format) {
        case kFormat8888:
          if (use_16bpp) {
            auto buffer16 = Convert8888To5551(buffer, size);
            type = GL_UNSIGNED_SHORT_5_5_5_1;
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(),
                                 0, format, type, buffer16));
            delete[] buffer16;
          } else {
            // Fallback to 8888
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(),
                                 0, format, type, buffer));
          }
          break;
        case kFormat5551:
          // Nothing to do.
          break;
        default:
          // This conversion not supported yet.
          assert(false);
          break;
      }
      break;
    }
    case kFormat565: {
      switch (texture_format) {
        case kFormat888:
          format = GL_RGB;
          if (use_16bpp) {
            auto buffer16 = Convert888To565(buffer, size);
            type = GL_UNSIGNED_SHORT_5_6_5;
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(),
                                 0, format, type, buffer16));
            delete[] buffer16;
          } else {
            // Fallback to 888
            GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(),
                                 0, format, type, buffer));
          }
          break;
        case kFormat565:
          // Nothing to do.
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
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
      break;
    }
    case kFormat888: {
      assert(texture_format == kFormat888);
      format = GL_RGB;
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
      break;
    }
    case kFormatLuminance: {
      assert(texture_format == kFormatLuminance);
      format = GL_LUMINANCE;
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
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
      GL_CALL(glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, size.x(),
                                     size.y(), 0, data_size,
                                     buffer + sizeof(ASTCHeader)));
      break;
    }
    case kFormatPKM: {
      assert(texture_format == kFormatPKM);
      auto &header = *reinterpret_cast<const PKMHeader *>(buffer);
      auto ext_xsize = (header.ext_width[0] << 8) | header.ext_width[1];
      auto ext_ysize = (header.ext_height[0] << 8) | header.ext_height[1];
      auto data_size = (ext_xsize / 4) * (ext_ysize / 4) * 8;
      format = GL_COMPRESSED_RGB8_ETC2;
      GL_CALL(glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, size.x(),
                                     size.y(), 0, data_size,
                                     buffer + sizeof(PKMHeader)));
      break;
    }
    case kFormatKTX: {
      assert(texture_format == kFormatKTX);
      auto &header = *reinterpret_cast<const KTXHeader *>(buffer);
      format = header.internal_format;
      auto data = buffer + sizeof(KTXHeader);
      auto cur_size = size;
      auto offset = 0;
      for (uint32_t i = 0; i < header.mip_levels; i++) {
        auto data_size = *(reinterpret_cast<const int32_t *>(data + offset));
        offset += sizeof(int32_t);
        GL_CALL(glCompressedTexImage2D(GL_TEXTURE_2D, i, format, cur_size.x(),
                                       cur_size.y(), 0, data_size,
                                       data + offset));
        cur_size /= 2;
        offset += data_size;
      }
      break;
    }
    default:
      assert(false);
  }

  if (mipmaps) {
    // Work around for some Android devices to correctly generate miplevels.
    auto min_dimension = static_cast<float>(std::min(size.x(), size.y()));
    auto levels = ceil(log(min_dimension) / log(2.0f));
    auto width = size.x() / 2;
    auto height = size.y() / 2;
    for (auto i = 1; i < levels; ++i) {
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, i, format, width, height, 0, format,
                           type, nullptr));
      width /= 2;
      height /= 2;
    }

    GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
  }
  return texture_id;
}

void Texture::UpdateTexture(TextureFormat format, int xoffset, int yoffset,
                            int width, int height, const void *data) {
  // In OpenGL ES2.0, width and pitch of the src buffer needs to match. So
  // that we are updating entire row at once.
  // TODO(wvo): Optimize glTexSubImage2D call in ES3.0 capable platform.
  switch (format) {
    case kFormatLuminance:
      GL_CALL(glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height,
                              GL_LUMINANCE, GL_UNSIGNED_BYTE, data));
      break;
    default:
      assert(false);  // TODO(wvo): not implemented.
  }
}

uint8_t *Texture::UnpackTGA(const void *tga_buf, vec2i *dimensions,
                            TextureFormat *texture_format) {
  struct TGA {
    uint8_t id_len, color_map_type, image_type, color_map_data[5];
    uint16_t x_origin, y_origin, width, height;
    uint8_t bpp, image_descriptor;
  };
  static_assert(sizeof(TGA) == 18,
                "Members of struct TGA need to be packed with no padding.");
  int little_endian = 1;
  if (!*reinterpret_cast<char *>(&little_endian)) {
    return nullptr;  // TODO(wvo): Endian swap the shorts instead.
  }
  auto header = reinterpret_cast<const TGA *>(tga_buf);
  if (header->color_map_type != 0  // No color map.
      || header->image_type != 2   // RGB or RGBA only.
      || (header->bpp != 32 && header->bpp != 24))
    return nullptr;
  auto pixels = reinterpret_cast<const unsigned char *>(header + 1);
  pixels += header->id_len;
  int size = header->width * header->height;
  // We use malloc to ensure that all unpacked texture formats can be freed
  // in the same way (see also other Unpack* functions).
  auto dest = reinterpret_cast<uint8_t *>(malloc(size * header->bpp / 8));
  int start_y, end_y, y_direction;
  if (header->image_descriptor & 0x20) {  // y is not flipped.
    start_y = 0;
    end_y = header->height;
    y_direction = 1;
  } else {  // y is flipped.
    start_y = header->height - 1;
    end_y = -1;
    y_direction = -1;
  }
  for (int y = start_y; y != end_y; y += y_direction) {
    for (int x = 0; x < header->width; x++) {
      auto p = dest + (y * header->width + x) * header->bpp / 8;
      p[2] = *pixels++;  // BGR -> RGB
      p[1] = *pixels++;
      p[0] = *pixels++;
      if (header->bpp == 32) p[3] = *pixels++;
    }
  }
  *texture_format = header->bpp == 32 ? kFormat8888 : kFormat888;
  *dimensions = vec2i(header->width, header->height);
  return dest;
}

uint8_t *Texture::UnpackWebP(const void *webp_buf, size_t size,
                             const vec2 &scale, vec2i *dimensions,
                             TextureFormat *texture_format) {
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
    config.output.colorspace = MODE_RGBA;
  }
  status = WebPDecode(static_cast<const uint8_t *>(webp_buf), size, &config);
  if (status != VP8_STATUS_OK) return nullptr;

  *dimensions = vec2i(config.output.width, config.output.height);
  *texture_format = config.input.has_alpha != 0 ? kFormat8888 : kFormat888;
  return config.output.private_memory;  // Allocated with malloc by webp.
}

uint8_t *Texture::UnpackASTC(const void *astc_buf, size_t size,
                             vec2i *dimensions, TextureFormat *texture_format) {
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
                            vec2i *dimensions, TextureFormat *texture_format) {
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
                            vec2i *dimensions, TextureFormat *texture_format) {
  auto &header = *reinterpret_cast<const KTXHeader *>(file_buf);
  auto magic = "\xABKTX 11\xBB\r\n\x1A\n";
  auto v = memcmp(header.id, magic, sizeof(header.id));
  if (v != 0 ||
      header.endian != 0x04030201 ||
      header.depth != 0 ||
      header.faces != 1 ||
      header.keyvalue_data != 0)
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

uint8_t *Texture::LoadAndUnpackTexture(const char *filename, const vec2 &scale,
                                       vec2i *dimensions,
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
      auto buf =
          UnpackASTC(file.c_str(), file.length(), dimensions, texture_format);
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
      auto buf =
          UnpackPKM(file.c_str(), file.length(), dimensions, texture_format);
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
      auto buf =
          UnpackKTX(file.c_str(), file.length(), dimensions, texture_format);
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

  if (ext == "tga") {
    auto buf = UnpackTGA(file.c_str(), dimensions, texture_format);
    if (!buf) LogError(kApplication, "TGA format problem: %s", filename);
    return buf;
  } else if (ext == "webp") {
    auto buf = UnpackWebP(file.c_str(), file.length(), scale, dimensions,
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
