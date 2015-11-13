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
#include "fplbase/texture.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"
#include "mathfu/glsl_mappings.h"
#include "webp/decode.h"

namespace fpl {

using mathfu::vec2i;

void Texture::Load() {
  data_ = LoadAndUnpackTexture(filename_.c_str(), scale_, &size_, &has_alpha_);
  SetOriginalSizeIfNotYetSet(size_);
}

void Texture::LoadFromMemory(const uint8_t *data, const vec2i &size,
                             bool has_alpha) {
  size_ = size;
  SetOriginalSizeIfNotYetSet(size_);
  has_alpha_ = has_alpha;
  id_ = CreateTexture(data, size_, has_alpha_, mipmaps_, desired_);
}

void Texture::Finalize() {
  if (data_) {
    id_ = CreateTexture(data_, size_, has_alpha_, mipmaps_, desired_);
    free(data_);
    data_ = nullptr;
  }
}

void Texture::Set(size_t unit) {
  GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));
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
                              bool has_alpha, bool mipmaps,
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
  const bool use_16bpp = fpl::MipmapGeneration16bppSupported();

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
  if (desired == kFormatAuto) desired = has_alpha ? kFormat5551 : kFormat565;
  switch (desired) {
    case kFormat5551: {
      assert(has_alpha);
      if (use_16bpp) {
        auto buffer16 = Convert8888To5551(buffer, size);
        type = GL_UNSIGNED_SHORT_5_5_5_1;
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                             format, type, buffer16));
        delete[] buffer16;
      } else {
        // Fallback to 8888
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                             format, type, buffer));
      }
      break;
    }
    case kFormat565: {
      assert(!has_alpha);
      format = GL_RGB;
      if (use_16bpp) {
        auto buffer16 = Convert888To565(buffer, size);
        type = GL_UNSIGNED_SHORT_5_6_5;
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                             format, type, buffer16));
        delete[] buffer16;
      } else {
        // Fallback to 888
        GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                             format, type, buffer));
      }
      break;
    }
    case kFormat8888: {
      assert(has_alpha);
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
      break;
    }
    case kFormat888: {
      assert(!has_alpha);
      format = GL_RGB;
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
      break;
    }
    case kFormatLuminance: {
      assert(!has_alpha);
      format = GL_LUMINANCE;
      GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, format, size.x(), size.y(), 0,
                           format, type, buffer));
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
                            bool *has_alpha) {
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
  *has_alpha = header->bpp == 32;
  *dimensions = vec2i(header->width, header->height);
  return dest;
}

uint8_t *Texture::UnpackWebP(const void *webp_buf, size_t size,
                             const vec2 &scale, vec2i *dimensions,
                             bool *has_alpha) {
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
  *has_alpha = config.input.has_alpha != 0;
  return config.output.private_memory;
}

uint8_t *Texture::LoadAndUnpackTexture(const char *filename, const vec2 &scale,
                                       vec2i *dimensions, bool *has_alpha) {
  std::string file;
  if (!LoadFile(filename, &file)) {
    LogError(kApplication, "Couldn\'t load: %s", filename);
    return nullptr;
  }

  std::string ext = filename;
  size_t ext_pos = ext.find_last_of(".");
  if (ext_pos != std::string::npos) ext = ext.substr(ext_pos + 1);
  if (ext == "tga") {
    auto buf = UnpackTGA(file.c_str(), dimensions, has_alpha);
    if (!buf) {
      LogError(kApplication, "TGA format problem: %s", filename);
    }
    return buf;
  } else if (ext == "webp") {
    auto buf =
        UnpackWebP(file.c_str(), file.length(), scale, dimensions, has_alpha);
    if (!buf) {
      LogError(kApplication, "WebP format problem: %s", filename);
    }
    return buf;
  } else {
    LogError(kApplication, "Can\'t figure out file type from extension: %s",
             filename);
    return nullptr;
  }
}

}  // namespace fpl
