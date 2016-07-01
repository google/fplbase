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

#ifndef FPLBASE_TEXTURE_H
#define FPLBASE_TEXTURE_H

#include <vector>

#include "fplbase/config.h"  // Must come first.

#include "fplbase/async_loader.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

namespace fplbase {

/// @file
/// @addtogroup fplbase_texture
/// @{

class RenderContext;

enum TextureFormat {
  kFormatAuto = 0,  ///< @brief The default, picks based on loaded data.
  kFormat8888,
  kFormat888,
  kFormat5551,
  kFormat565,
  kFormatLuminance,
  kFormatASTC,
  kFormatPKM,
  kFormatKTX,
  kFormatNative,  ///< @brief Uses the same format as the source file.
  kFormatCount    // Must be at end.
};

enum TextureFlags {
  kTextureFlagsNone = 0,
  kTextureFlagsClampToEdge = 1 << 0,  // If not set, use repeating texcoords.
  kTextureFlagsUseMipMaps = 1 << 1,   // Uses (or generates) mipmaps.
  kTextureFlagsIsCubeMap = 1 << 2,    // Data represents a 1x6 cubemap.
  kTextureFlagsLoadAsync = 1 << 3,    // Load texture asynchronously.
};

inline TextureFlags operator|(TextureFlags a, TextureFlags b) {
  return static_cast<TextureFlags>(static_cast<int>(a) | static_cast<int>(b));
}

/// @brief determines if the format has an alpha component.
inline bool HasAlpha(TextureFormat format) {
  switch (format) {
    case kFormat8888:
    case kFormat5551:
    case kFormatASTC:
    case kFormatKTX:  // TODO(wvo): depends on the internal format.
      return true;
    default:
      return false;
  }
}

/// @brief determines if the format is already compressed in some way.
/// If image data is supplied in these formats, we use them as-is.
inline bool IsCompressed(TextureFormat format) {
  switch (format) {
    case kFormat5551:
    case kFormat565:
    case kFormatASTC:
    case kFormatPKM:
    case kFormatKTX:
      return true;
    default:
      return false;
  }
}

/// @brief This typedef is compatible with its OpenGL equivalent, but doesn't
/// require this header to depend on OpenGL.
typedef unsigned int TextureHandle;

/// @class Texture
/// @brief Abstraction for a texture object loaded on the GPU.
///
/// Contains functions for loading, marshalling, and disposal of textures.
class Texture : public AsyncAsset {
 public:
  /// @brief Constructor for a Texture.
  explicit Texture(const char *filename = nullptr,
                   TextureFormat format = kFormatAuto,
                   TextureFlags flags = kTextureFlagsUseMipMaps)
      : AsyncAsset(filename ? filename : ""),
        id_(0),
        size_(mathfu::kZeros2i),
        original_size_(mathfu::kZeros2i),
        scale_(mathfu::kOnes2f),
        texture_format_(kFormat888),
        desired_(format),
        flags_(flags) {}

  /// @brief Destructor for a Texture.
  /// @note Calls `Delete()`.
  virtual ~Texture() { Delete(); }

  /// @brief Loads and unpacks the Texture from `filename_` into `data_`. It
  /// also sets the original size, if it has not yet been set.
  virtual void Load();

  /// @brief Create a texture from data in memory.
  /// @param[in] data The Texture data in memory to load from.
  /// @param[in] size A const `mathfu::vec2i` reference to the original
  /// Texture size `x` and `y` components.
  /// @param[in] texture_format The format of `data`.
  virtual void LoadFromMemory(const uint8_t *data, const mathfu::vec2i &size,
                              TextureFormat texture_format);

  /// @brief Creates a Texture from `data_` and stores the handle in `id_`.
  virtual void Finalize();

  /// @brief Set the active Texture and binds `id_` to `GL_TEXTURE_2D`.
  /// @param[in] unit Specifies which texture unit to make active.
  /// @param[in] render_context Pointer to the RenderContext object
  /// @note Modifies global OpenGL state.
  void Set(size_t unit, RenderContext *render_context);
  /// @overload void Set(size_t unit)
  void Set(size_t unit);

  /// @brief Set the active Texture and binds `id_` to `GL_TEXTURE_2D`.
  /// @param[in] unit Specifies which texture unit to make active.
  /// @param[in] render_context Pointer to the RenderContext object
  /// @note Modifies global OpenGL state.
  void Set(size_t unit, RenderContext *render_context) const;
  /// @overload void Set(size_t unit) const
  void Set(size_t unit) const;

  /// @brief Delete the Texture stored in `id_`, and reset `id_` to `0`.
  void Delete();

  /// @brief Create a texture from a memory buffer containing `xsize` * `ysize`
  /// RGBA pixels.
  /// @param[in] buffer The data to create the Texture from.
  /// @param[in] size A const `mathfu::vec2i` reference to the original
  /// Texture size `x` and `y` components.
  /// @param[in] texture_format The format of `buffer`.
  /// @param[in] mipmaps If `true`, use the work around for some Android devices
  /// to correctly generate miplevels. Defaults to `true`.
  /// @param[in] desired The desired TextureFormat. Defaults to `kFormatAuto`.
  /// @param[in] wrapping The desired TextureWrapping. Defaults to 'kRepeat'.
  /// @return Returns the Texture handle. Otherwise, it returns `0`, if not a
  /// power of two in size.
  static TextureHandle CreateTexture(
      const uint8_t *buffer, const mathfu::vec2i &size,
      TextureFormat texture_format, TextureFormat desired = kFormatAuto,
      TextureFlags flags = kTextureFlagsUseMipMaps);

  /// @brief Update (part of) the current texture with new pixel data.
  /// For now, must always update at least entire rows.
  static void UpdateTexture(TextureFormat format, int xoffset, int yoffset,
                            int width, int height, const void *data);

  /// @brief Unpacks a memory buffer containing a TGA format file.
  /// @note May only be uncompressed RGB or RGBA data, Y-flipped or not.
  /// @param[in] tga_buf The TGA image data.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the TGA
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns RGBA array of returned dimensions or `nullptr` if the
  /// format is not understood.
  /// @note You must `free()` the returned pointer when done.
  static uint8_t *UnpackTGA(const void *tga_buf, mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Unpacks a memory buffer containing a Webp format file.
  /// @param[in] webp_buf The WebP image data.
  /// @param[in] size The size of the memory block pointed to by `webp_buf`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackWebP(const void *webp_buf, size_t size,
                             const mathfu::vec2 &scale,
                             mathfu::vec2i *dimensions,
                             TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an ASTC format (.astc) file.
  /// @param[in] astc_buf The ASTC image data.
  /// @param[in] size The size of the memory block pointed to by `astc_buf`.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatASTC.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackASTC(const void *astc_buf, size_t size,
                             mathfu::vec2i *dimensions,
                             TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an ETC2 format (.pkm) file.
  /// @param[in] file_buf the loaded file.
  /// @param[in] size The size of the memory block pointed to by `file_buf`.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatETC2.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackPKM(const void *file_buf, size_t size,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an KTX format (.ktx) file.
  /// @param[in] file_buf the loaded file.
  /// @param[in] size The size of the memory block pointed to by `file_buf`.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatETC2.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackKTX(const void *file_buf, size_t size,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Unpacks a memory buffer containing a Png format file.
  /// @param[in] png_buf The Png image data.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] has_alpha A `bool` pointer that captures whether the Png
  /// image has an alpha.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackPng(const void *png_buf, size_t size,
                            const mathfu::vec2 &scale,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format) {
    return UnpackImage(png_buf, size, scale, dimensions, texture_format);
  }

  /// @brief Unpacks a memory buffer containing a Jpeg format file.
  /// @param[in] jpg_buf The Jpeg image data.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] has_alpha A `bool` pointer that captures whether the Png
  /// image has an alpha.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackJpg(const void *jpg_buf, size_t size,
                            const mathfu::vec2 &scale,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format) {
    return UnpackImage(jpg_buf, size, scale, dimensions, texture_format);
  }

  /// @brief Loads the file in filename, and then unpacks the file format
  /// (supports TGA, WebP, KTX, PKM, ASTC).
  /// @note KTX/PKM/ASTC will automatically fall-back on WebP if the file is not
  /// present or not supported by the GPU.
  /// @note `last_error()` contains more information if `nullptr` is returned.
  ///  You must `free()` the returned pointer when done.
  /// @param[in] filename A C-string corresponding to the name of the file
  /// containing the Texture.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the Texture
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *LoadAndUnpackTexture(const char *filename,
                                       const mathfu::vec2 &scale,
                                       mathfu::vec2i *dimensions,
                                       TextureFormat *texture_format);

  /// @brief Utility function to convert 32bit RGBA (8-bits each) to 16bit RGB
  /// in hex 5551 format.
  /// @note You must `delete[]` the return value afterwards.
  static uint16_t *Convert8888To5551(const uint8_t *buffer,
                                     const mathfu::vec2i &size);
  /// @brief Utility function to convert 24bit RGB (8-bits each) to 16bit RGB in
  /// hex 565 format.
  /// @note You must `delete[]` the return value afterwards.
  static uint16_t *Convert888To565(const uint8_t *buffer,
                                   const mathfu::vec2i &size);

  /// @brief Get the Texture handle ID.
  /// @return Returns the TextureHandle ID.
  const TextureHandle &id() const { return id_; }

  /// @brief Get the Texture size.
  /// @return Returns a const `mathfu::vec2i` reference to the size of the
  /// Texture.
  const mathfu::vec2i &size() const { return size_; }

  /// @brief Get the Texture scale.
  /// @return Returns a const `mathfu::vec2` reference to the scale of the
  /// Texture.
  const mathfu::vec2 &scale() const { return scale_; }

  /// @brief Set the Texture scale.
  /// @param[in] scale A const `mathfu::vec2` reference containing the
  /// `x` and `y` scale components to set for the Texture.
  void set_scale(const mathfu::vec2 &scale) { scale_ = scale; }

  /// @brief returns the texture flags.
  TextureFlags flags() const { return flags_; }

  /// @brief Get the original size of the Texture.
  /// @return Returns a const `mathfu::vec2` reference to the scale of the
  /// Texture.
  const mathfu::vec2i &original_size() const { return original_size_; }

  /// @brief Set the original size of the Texture.
  /// @param[in] size A `mathfu::vec2i` containing the original Texture
  /// `x` and `y` sizes.
  void set_original_size(const mathfu::vec2i &size) { original_size_ = size; }

  /// @brief Get the Texture format.
  /// @return Returns the texture format.
  TextureFormat format() const { return texture_format_; }

  /// @brief If the original size has not yet been set, then set it.
  /// @param[in] size A `mathfu::vec2i` containing the original Texture
  /// `x` and `y` sizes.
  void SetOriginalSizeIfNotYetSet(const mathfu::vec2i &size) {
    if (original_size_.x() == 0 && original_size_.y() == 0) {
      original_size_ = size;
    }
  }

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  /// @brief Unpacks a memory buffer containing a PNG/JPEG/TGA format file.
  /// @param[in] img_buf The PNG/JPEG/TGA image data including an image header.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] has_alpha A `bool` pointer that captures whether the Png
  /// image has an alpha.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackImage(const void *img_buf, size_t size,
                              const mathfu::vec2 &scale,
                              mathfu::vec2i *dimensions,
                              TextureFormat *texture_format);

  TextureHandle id_;
  mathfu::vec2i size_;
  mathfu::vec2i original_size_;
  mathfu::vec2 scale_;
  TextureFormat texture_format_;
  TextureFormat desired_;
  TextureFlags flags_;
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_TEXTURE_H
