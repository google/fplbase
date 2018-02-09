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

#ifndef FPLBASE_MATERIAL_H
#define FPLBASE_MATERIAL_H

#include <assert.h>
#include <vector>

#include "fplbase/config.h"  // Must come first.

#include "fplbase/asset.h"
#include "fplbase/render_state.h"
#include "fplbase/texture.h"
#include "materials_generated.h"

namespace fplbase {

/// @file
/// @addtogroup fplbase_material
/// @{

class Renderer;
class Texture;

/// @brief Collections of textures used for rendering multi-texture models.
class Material : public Asset {
 public:
  /// @brief Default constructor for Material.
  Material() : blend_mode_(kBlendModeOff) {}

  /// @brief Set the renderer for this Material.
  /// @param[in] renderer The renderer to set for this Material.
  void Set(Renderer &renderer);

  /// @brief Get all Textures from this Material.
  /// @return Returns a `std::vector<Texture *>` reference to get all
  /// the Textures from this Material.
  std::vector<Texture *> &textures() { return textures_; }

  /// @brief Get all Textures from this Material.
  /// @return Returns a const `std::vector<Texture *>` reference to get all
  /// the Textures from this Material.
  const std::vector<Texture *> &textures() const { return textures_; }

  /// @brief Get the blend mode for this Material.
  /// @return Returns an `int` corresponding the the @ref fplbase_material
  /// "BlendMode" enum for this Material.
  int blend_mode() const { return blend_mode_; }

  /// @brief Set the blend mode.
  /// @param[in] blend_mode A @ref fplbase_material "BlendMode" enum
  /// corresponding to the blend mode to set for this texture.
  /// @warning Asserts if an invalid enum value for `blend_mode` is passed in.
  void set_blend_mode(BlendMode blend_mode) {
    assert(0 <= blend_mode && blend_mode < kBlendModeCount);
    blend_mode_ = blend_mode;
  }

  /// @brief Delete all Textures in this Material.
  void DeleteTextures();

  /// @brief Create a Material from the specified flatbuffer matdef::Material*.
  static Material *LoadFromMaterialDef(const matdef::Material *matdef,
                                       const TextureLoaderFn &tlf);

  /// @brief Load a .fplmat file, and all the textures referenced from it.
  /// Used by the more convenient AssetManager interface, but can be used
  /// without it.
  static Material *LoadFromMaterialDef(const char *filename,
                                       const TextureLoaderFn &tlf);

  /// @brief The filename that was the source of the material, if this
  /// material was loaded from a file.
  const std::string& filename() const { return filename_; }

  /// @brief Setter for The filename that is the source of the
  /// material.
  void set_filename(const std::string& filename) { filename_ = filename; }

 private:
  std::string filename_;
  std::vector<Texture *> textures_;
  BlendMode blend_mode_;
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_MATERIAL_H
