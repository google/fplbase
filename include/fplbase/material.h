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

namespace fplbase {

/// @file
/// @addtogroup fplbase_material
/// @{

class Renderer;
class Texture;

/// @brief Specifies the blending mode used by the blend function.
enum BlendMode {
  kBlendModeOff = 0, /**< No blending is done. The alpha channel is ignored. */
  kBlendModeTest, /**< Used to provide a function via `glAlphaFunc` instead of
                       `glBlendFunc`. This is not supported directly, meaning
                       the `glAlphaFunc` must be set manually by the user. This
                       will simply configure the renderer to use that
                       function.*/
  kBlendModeAlpha, /**< Normal alpha channel blending. */
  kBlendModeAdd, /**< Additive blending, where the resulting color is the sum of
                      the two colors. */
  kBlendModeAddAlpha, /**< Additive blending, where the resulting color is the
                           sum of the two colors, and the image is affected
                           by the alpha. (Note: The background is not affected
                           by the image's alpha.) */
  kBlendModeMultiply, /**< Multiplicative blending, where the resulting color is
                           the product of the image color and the background
                           color. */
  kBlendModeCount  /** Used at the end of the enum as sentinel value. */
};

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

 private:
  std::vector<Texture *> textures_;
  BlendMode blend_mode_;
};

/// @}
}  // namespace fplbase

#endif  // FPLBASE_MATERIAL_H
