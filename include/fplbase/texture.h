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

enum TextureFormat {
  kFormatAuto = 0,  // The default, picks based on loaded data.
  kFormat8888,
  kFormat888,
  kFormat5551,
  kFormat565,
  kFormatLuminance,
  kFormatCount  // Must be at end.
};

// This typedef is compatible with its OpenGL equivalent, but doesn't require
// this header to depend on OpenGL.
typedef unsigned int TextureHandle;

/// @class Texture
/// @brief Abstraction for a texture object loaded on the GPU.
///
/// Contains functions for loading, marshalling, and disposal of textures.
class Texture : public AsyncAsset {
 public:
  explicit Texture(const char *filename = nullptr,
                   TextureFormat format = kFormatAuto, bool mipmaps = true)
      : AsyncAsset(filename ? filename : ""),
        id_(0),
        size_(mathfu::kZeros2i),
        original_size_(mathfu::kZeros2i),
        scale_(mathfu::kOnes2f),
        has_alpha_(false),
        mipmaps_(mipmaps),
        desired_(format) {}
  virtual ~Texture() { Delete(); }

  virtual void Load();
  virtual void LoadFromMemory(const uint8_t *data, const mathfu::vec2i &size,
                              bool has_alpha);
  virtual void Finalize();

  void Set(size_t unit);
  // Const version of Set() API. Note that the API modifies global OpenGL state.
  void Set(size_t unit) const;

  void Delete();

  // Create a texture from a memory buffer containing xsize * ysize RGBA pixels.
  // Return 0 if not a power of two in size.
  static TextureHandle CreateTexture(const uint8_t *buffer,
                                     const mathfu::vec2i &size, bool has_alpha,
                                     bool mipmaps = true,
                                     TextureFormat desired = kFormatAuto);

  // Update (part of) the current texture with new pixel data.
  // For now, must always update at least entire rows.
  static void UpdateTexture(TextureFormat format, int xoffset, int yoffset,
                            int width, int height, const void *data);

  // Unpacks a memory buffer containing a TGA format file.
  // May only be uncompressed RGB or RGBA data, Y-flipped or not.
  // Returns RGBA array of returned dimensions or nullptr if the
  // format is not understood.
  // You must free() the returned pointer when done.
  static uint8_t *UnpackTGA(const void *tga_buf, mathfu::vec2i *dimensions,
                            bool *has_alpha);

  // Unpacks a memory buffer containing a Webp format file.
  // Returns RGBA array of the returned dimensions or nullptr if the format
  // is not understood.
  // You must free() the returned pointer when done.
  // Can apply scaling with scale parameter. A scale value have to be in power
  // of two to have correct texture sizes.
  static uint8_t *UnpackWebP(const void *webp_buf, size_t size,
                             const mathfu::vec2 &scale,
                             mathfu::vec2i *dimensions, bool *has_alpha);

  // Loads the file in filename, and then unpacks the file format (supports
  // TGA and WebP).
  // last_error() contains more information if nullptr is returned.
  // You must free() the returned pointer when done.
  // Can apply scaling with scale parameter.
  static uint8_t *LoadAndUnpackTexture(const char *filename,
                                       const mathfu::vec2 &scale,
                                       mathfu::vec2i *dimensions,
                                       bool *has_alpha);

  // Utility functions to convert 32bit RGBA to 16bit.
  // You must delete[] the return value afterwards.
  static uint16_t *Convert8888To5551(const uint8_t *buffer,
                                     const mathfu::vec2i &size);
  static uint16_t *Convert888To565(const uint8_t *buffer,
                                   const mathfu::vec2i &size);

  const TextureHandle &id() const { return id_; }
  const mathfu::vec2i &size() const { return size_; }

  const mathfu::vec2 &scale() const { return scale_; }
  void set_scale(const mathfu::vec2 &scale) { scale_ = scale; }

  const mathfu::vec2i &original_size() const { return original_size_; }
  void set_original_size(const mathfu::vec2i &size) { original_size_ = size; }
  void SetOriginalSizeIfNotYetSet(const mathfu::vec2i &size) {
    if (original_size_.x() == 0 && original_size_.y() == 0) {
      original_size_ = size;
    }
  }

  MATHFU_DEFINE_CLASS_SIMD_AWARE_NEW_DELETE

 private:
  TextureHandle id_;
  mathfu::vec2i size_;
  mathfu::vec2i original_size_;
  mathfu::vec2 scale_;
  bool has_alpha_;
  bool mipmaps_;
  TextureFormat desired_;
};

}  // namespace fplbase

#endif  // FPLBASE_TEXTURE_H
