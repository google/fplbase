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

#include "precompiled.h"

#include "fplbase/texture.h"
#include "fplbase/texture_atlas.h"
#include "fplbase/renderer.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/utilities.h"
#include "mathfu/glsl_mappings.h"
#include "texture_headers.h"
#include "webp/decode.h"
#include "texture_atlas_generated.h"

using mathfu::vec2i;

namespace fplbase {

void Texture::Set(size_t unit, RenderContext *) {
  GL_CALL(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit)));
  GL_CALL(glBindTexture(target_, id_));
}

void Texture::Delete() {
  if (id_) {
    if (!is_external_) {
      GL_CALL(glDeleteTextures(1, &id_));
    }
    id_ = 0;
  }
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
    if (tex_size.x != tex_size.y) {
      LogError(kError, "CreateTexture: cubemap not in 1x6 format: (%d,%d)",
               size.x, size.y);
    }
  }

  if (!RendererBase::Get()->SupportsTextureNpot()) {
    // Npot textures are supported in ES 2.0 if you use GL_CLAMP_TO_EDGE and no
    // mipmaps. See Section 3.8.2 of ES2.0 spec:
    // https://www.khronos.org/registry/gles/specs/2.0/es_full_spec_2.0.25.pdf
    if (flags & kTextureFlagsUseMipMaps ||
        !(flags & kTextureFlagsClampToEdge)) {
      int area = tex_size.x * tex_size.y;
      if (area & (area - 1)) {
        LogError(kError, "CreateTexture: not power of two in size: (%d,%d)",
                 tex_size.x, tex_size.y);
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
                                       mip_size.x, mip_size.y, 0, buf_size,
                                       buf));
      } else {
        GL_CALL(glTexImage2D(tex_imagetype + i, mip_level, format, mip_size.x,
                             mip_size.y, 0, format, type, buf));
      }
      if (buf) buf += buf_size;
    }
  };

  int num_pixels = tex_size.x * tex_size.y;

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
      auto xblocks = (size.x + header.blockdim_x - 1) / header.blockdim_x;
      auto yblocks = (size.y + header.blockdim_y - 1) / header.blockdim_y;
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
        // Guard against extra mip levels in the ktx.
        if (cur_size.x == 0 && cur_size.y == 0) {
          LogError("KTX file has too many mips: %dx%d, %d mips", tex_size.x,
                   tex_size.y, header.mip_levels);
          break;
        }
        auto data_size = *(reinterpret_cast<const int32_t *>(data));
        data += sizeof(int32_t);
        // Keep loading mip data even if one of our calculated dimensions goes
        // to 0, but maintain a min size of 1.  This is needed to get non-square
        // mip chains to work using ETC2 (eg a 256x512 needs 10 mips defined).
        gl_tex_image(data, vec2i::Max(mathfu::kOnes2i, cur_size), i,
                     data_size / tex_num_faces, true);
        cur_size /= 2;
        data += data_size;
        // If the file has mips but the caller doesn't want them, stop here.
        if (!have_mips) break;
      }
      break;
    }
    default:
      assert(false);
  }

  if (generate_mips && buffer != nullptr) {
    // Work around for some Android devices to correctly generate miplevels.
    // NOTE:  If client creates a texture with buffer == nullptr (i.e. to
    // render into later), and wants mipmapping, and is on a phone requiring
    // this workaround, the client will need to do this preallocation
    // workaround themselves.
    auto min_dimension = static_cast<float>(std::min(tex_size.x, tex_size.y));
    auto levels = ceil(log(min_dimension) / log(2.0f));
    auto mip_size = tex_size / 2;
    for (auto i = 1; i < levels; ++i) {
      gl_tex_image(nullptr, mip_size, i, 0, false);
      mip_size /= 2;
    }

    GL_CALL(glGenerateMipmap(tex_type));
  } else if (have_mips && IsCompressed(texture_format)) {
#ifndef __ANDROID__
    // At least on Linux, there appears to be a bug with uploading pre-made
    // compressed mipmaps that makes the texture not show up if
    // glGenerateMipmap isn't called, even though glGenerateMipmap can't
    // generate any mipmaps for compressed textures.
    // Also, this call should generate GL_INVALID_OPERATION but it doesn't?
    // TODO(wvo): is this a driver bug, or what is the root cause of this?
    // On android/adreno, this goes generate a GL_INVALID_OPERATION and isn't
    // needed.
    GL_CALL(glGenerateMipmap(tex_type));
#endif
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

}  // namespace fplbase
