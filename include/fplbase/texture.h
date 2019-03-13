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
#include "fplbase/handles.h"
#include "mathfu/constants.h"
#include "mathfu/glsl_mappings.h"

namespace fplbase {

class Renderer;
struct TextureImpl;

/// @file
/// @addtogroup fplbase_texture
/// @{

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
  kFormatLuminanceAlpha,
  kFormatCount    // Must be at end.
};

/// Flags affecting loading and sampler modes for a texture
enum TextureFlags {
  /// Default behavior.
  kTextureFlagsNone = 0,
  /// If not set, use repeating texcoords.
  kTextureFlagsClampToEdge = 1 << 0,
  /// Uses (or generates) mipmaps.
  kTextureFlagsUseMipMaps = 1 << 1,
  /// Data represents a 1x6 cubemap.
  kTextureFlagsIsCubeMap = 1 << 2,
  /// Load texture asynchronously.
  kTextureFlagsLoadAsync = 1 << 3,
  /// Premultiply by alpha on load.
  /// Not supported for ASTC, PKM, or KTX images.
  kTextureFlagsPremultiplyAlpha = 1 << 4,
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

/// @class Texture
/// @brief Abstraction for a texture object loaded on the GPU.
///
/// Contains functions for loading, marshalling, and disposal of textures.
class Texture : public AsyncAsset {
 public:
  /// @brief Constructor for a Texture.
  explicit Texture(const char *filename = nullptr,
                   TextureFormat format = kFormatAuto,
                   TextureFlags flags = kTextureFlagsUseMipMaps);

  /// @brief Destructor for a Texture.
  /// @note Calls `Delete()`.
  virtual ~Texture();

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
  virtual bool Finalize();

  /// @brief Whether this object loaded and finalized correctly. Call after
  /// Finalize has been called (by AssetManager::TryFinalize).
  bool IsValid() { return ValidTextureHandle(id_); }

  /// @brief Set the active Texture and binds `id_` to `GL_TEXTURE_2D`.
  /// @param[in] unit Specifies which texture unit to make active.
  /// @param[in] renderer Pointer to the Renderer
  /// @note Modifies global OpenGL state.
  void Set(size_t unit, Renderer *renderer);
  /// @overload void Set(size_t unit)
  void Set(size_t unit);

  /// @brief Set the active Texture and binds `id_` to `GL_TEXTURE_2D`.
  /// @param[in] unit Specifies which texture unit to make active.
  /// @param[in] renderer Pointer to the Renderer
  /// @note Modifies global OpenGL state.
  void Set(size_t unit, Renderer *renderer) const;
  /// @overload void Set(size_t unit) const
  void Set(size_t unit) const;

  /// @brief Delete the Texture stored in `id_`, and reset `id_` to `0`.
  void Delete();

  /// @brief Update (part of) the current texture with new pixel data.
  /// For now, must always update at least entire row.
  /// @param[in] unit Specifies which texture unit to do the update with.
  /// @param[in] texture_format The format of `data`.
  /// @param[in] xoffset Lowest x-pixel coordinate to update.
  /// @param[in] yoffset Lowest y-pixel coordinate to update.
  /// @param[in] width Number of pixels along x-axis to update.
  /// @param[in] hegiht Number of pixels along y-axis to update.
  /// @param[in] data The Texture data in memory to load from.
  void UpdateTexture(size_t unit, TextureFormat format, int xoffset,
                     int yoffset, int width, int height, const void *data);

  /// @brief Unpacks a memory buffer containing a TGA format file.
  /// @note May only be uncompressed RGB or RGBA data, Y-flipped or not.
  /// @param[in] tga_buf The TGA image data.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the TGA
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns RGBA array of returned dimensions or `nullptr` if the
  /// format is not understood.
  /// @note You must `free()` the returned pointer when done.
  static uint8_t *UnpackTGA(const void *tga_buf, TextureFlags flags,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Unpacks a memory buffer containing a Webp format file.
  /// @param[in] webp_buf The WebP image data.
  /// @param[in] size The size of the memory block pointed to by `webp_buf`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[in] flags Texture flag, allowing premultiply
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackWebP(const void *webp_buf, size_t size,
                             const mathfu::vec2 &scale, TextureFlags flags,
                             mathfu::vec2i *dimensions,
                             TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an ASTC format (.astc) file.
  /// @param[in] astc_buf The ASTC image data.
  /// @param[in] size The size of the memory block pointed to by `astc_buf`.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatASTC.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackASTC(const void *astc_buf, size_t size,
                             TextureFlags flags, mathfu::vec2i *dimensions,
                             TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an ETC2 format (.pkm) file.
  /// @param[in] file_buf the loaded file.
  /// @param[in] size The size of the memory block pointed to by `file_buf`.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatETC2.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackPKM(const void *file_buf, size_t size,
                            TextureFlags flags, mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Reads a memory buffer containing an KTX format (.ktx) file.
  /// @param[in] file_buf the loaded file.
  /// @param[in] size The size of the memory block pointed to by `file_buf`.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// kFormatETC2.
  /// @return Returns a buffer ready to be uploaded to GPU memory or `nullptr`,
  /// if the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackKTX(const void *file_buf, size_t size,
                            TextureFlags flags, mathfu::vec2i *dimensions,
                            TextureFormat *texture_format);

  /// @brief Unpacks a memory buffer containing a Png format file.
  /// @param[in] png_buf The Png image data.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format Pixel format of unpacked image.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackPng(const void *png_buf, size_t size,
                            const mathfu::vec2 &scale, TextureFlags flags,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format) {
    return UnpackImage(png_buf, size, scale, flags, dimensions, texture_format);
  }

  /// @brief Unpacks a memory buffer containing a Jpeg format file.
  /// @param[in] jpg_buf The Jpeg image data.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] texture_format Pixel format of unpacked image.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackJpg(const void *jpg_buf, size_t size,
                            const mathfu::vec2 &scale, TextureFlags flags,
                            mathfu::vec2i *dimensions,
                            TextureFormat *texture_format) {
    return UnpackImage(jpg_buf, size, scale, flags, dimensions, texture_format);
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
  /// @param[in] flags Texture flag, allowing premultiply (only webp now)
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the Texture
  /// width and height.
  /// @param[out] texture_format The format of the returned buffer, always
  /// either 888 or 8888.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *LoadAndUnpackTexture(const char *filename,
                                       const mathfu::vec2 &scale,
                                       TextureFlags flags,
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

  /// @brief Create a texture from a memory buffer.
  /// @param[in] buffer The data to create the Texture from.
  /// @param[in] size The dimensions of the image in the data buffer.
  /// @param[in] texture_format The format of `buffer`.
  /// @param[in] desired The desired TextureFormat.
  /// @param[in] flags Options for the texture.
  /// @return Returns the Texture handle if successfully created, 0 otherwise.
  static TextureHandle CreateTexture(const uint8_t *buffer,
                                     const mathfu::vec2i &size,
                                     TextureFormat texture_format,
                                     TextureFormat desired, TextureFlags flags);

  /// @brief Set texture target and id directly for textures that have been
  /// created outside of this class.  The creator is responsible for deleting
  /// the texture id.
  /// @param[in] target Texture target to use when binding texture to context.
  /// @param[in] id Texture handle ID.
  void SetTextureId(TextureTarget target, TextureHandle id);

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
  /// @return Returns a const `mathfu::vec2i` reference to the original size of
  /// the Texture.
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
    if (original_size_.x == 0 && original_size_.y == 0) {
      original_size_ = size;
    }
  }

  // For internal use only.
  TextureImpl *impl() { return impl_; }

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  // Backend-specific create and destroy calls. These just call new and delete
  // on the platform-specific MeshImpl structs.
  static TextureImpl *CreateTextureImpl();
  static void DestroyTextureImpl(TextureImpl *impl);

  /// @brief Create a texture from a memory buffer containing `xsize` * `ysize`
  /// RGBA pixels.
  /// @param[in] buffer The data to create the Texture from.
  /// @param[in] size A const `mathfu::vec2i` reference to the original
  /// Texture size `x` and `y` components.
  /// @param[in] texture_format The format of `buffer`.
  /// @param[in] desired The desired TextureFormat.
  /// @param[in] flags Options for the texture.
  /// @return Returns the Texture handle. Otherwise, it returns `0`, if not a
  /// power of two in size.
  static TextureHandle CreateTexture(
      const uint8_t *buffer, const mathfu::vec2i &size,
      TextureFormat texture_format, TextureFormat desired,
      TextureFlags flags, TextureImpl *impl);

  /// @brief Unpacks a memory buffer containing a PNG/JPEG/TGA format file.
  /// @param[in] img_buf The PNG/JPEG/TGA image data including an image header.
  /// @param[in] size The size of the memory block pointed to by `data`.
  /// @param[in] scale A scale value must be a power of two to have correct
  /// Texture sizes.
  /// @param[in] flags Texture flag, allowing premultiply
  /// @param[out] dimensions A `mathfu::vec2i` pointer the captures the image
  /// width and height.
  /// @param[out] has_alpha A `bool` pointer that captures whether the Png
  /// image has an alpha.
  /// @return Returns a RGBA array of the returned dimensions or `nullptr`, if
  /// the format is not understood.
  /// @note You must `free()` on the returned pointer when done.
  static uint8_t *UnpackImage(const void *img_buf, size_t size,
                              const mathfu::vec2 &scale, TextureFlags flags,
                              mathfu::vec2i *dimensions,
                              TextureFormat *texture_format);

  /// @brief Backend specific conversion of flags to TextureTarget.
  static TextureTarget TextureTargetFromFlags(TextureFlags flags);

  TextureImpl *impl_;
  TextureHandle id_;
  mathfu::vec2i size_;
  mathfu::vec2i original_size_;
  mathfu::vec2 scale_;
  TextureFormat texture_format_;
  TextureTarget target_;
  TextureFormat desired_;
  TextureFlags flags_;
  bool is_external_;
};

/// @brief used by some functions to allow the texture loading mechanism to
/// be specified by the caller.
typedef std::function<Texture *(const char *filename, TextureFormat format,
                                TextureFlags flags)>
    TextureLoaderFn;

/// @}
}  // namespace fplbase

#endif  // FPLBASE_TEXTURE_H
