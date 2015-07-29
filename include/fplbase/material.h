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

#include <vector>

#include "fplbase/config.h" // Must come first.

#include "fplbase/async_loader.h"
#include "fplbase/shader.h"
#include "mathfu/glsl_mappings.h"
#include "mathfu/constants.h"

namespace fpl {

class Renderer;

enum BlendMode {
  kBlendModeOff,
  kBlendModeTest,
  kBlendModeAlpha,

  kBlendModeCount  // Must be at end.
};

enum TextureFormat {
  kFormatAuto = 0,  // The default, picks based on loaded data.
  kFormat8888,
  kFormat888,
  kFormat5551,
  kFormat565,
  kFormatLuminance,
};

// This typedef is compatible with its OpenGL equivalent, but doesn't require
// this header to depend on OpenGL.
typedef unsigned int TextureHandle;

class Texture : public AsyncResource {
 public:
  Texture(Renderer &renderer, const std::string &filename)
      : AsyncResource(filename),
        renderer_(&renderer),
        id_(0),
        size_(mathfu::kZeros2i),
        uv_(mathfu::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
        has_alpha_(false),
        desired_(kFormatAuto) {}
  Texture(Renderer &renderer)
      : AsyncResource(""),
        renderer_(&renderer),
        id_(0),
        size_(mathfu::kZeros2i),
        uv_(mathfu::vec4(0.0f, 0.0f, 1.0f, 1.0f)),
        has_alpha_(false),
        desired_(kFormatAuto) {}
  ~Texture() { Delete(); }

  virtual void Load();
  virtual void LoadFromMemory(const uint8_t *data, const mathfu::vec2i size,
                              const TextureFormat format, const bool has_alpha);
  virtual void Finalize();

  void Set(size_t unit);
  // Const version of Set() API. Note that the API modifies global OpenGL state.
  void Set(size_t unit) const;

  void Delete();

  const TextureHandle &id() const { return id_; }
  mathfu::vec2i size() { return size_; }
  const mathfu::vec2i size() const { return size_; }

  const mathfu::vec4 &uv() const { return uv_; }
  void set_uv(const vec4 &uv) { uv_ = uv; }

  void set_desired_format(TextureFormat format) { desired_ = format; }

 private:
  Renderer *renderer_;

  TextureHandle id_;
  mathfu::vec2i size_;
  mathfu::vec4 uv_;
  bool has_alpha_;
  TextureFormat desired_;
};

class Material {
 public:
  Material() : blend_mode_(kBlendModeOff) {}

  void Set(Renderer &renderer);

  std::vector<Texture *> &textures() { return textures_; }
  const std::vector<Texture *> &textures() const { return textures_; }
  int blend_mode() const { return blend_mode_; }
  void set_blend_mode(BlendMode blend_mode) {
    assert(0 <= blend_mode && blend_mode < kBlendModeCount);
    blend_mode_ = blend_mode;
  }

  void DeleteTextures();

 private:
  std::vector<Texture *> textures_;
  BlendMode blend_mode_;
};

}  // namespace fpl

#endif  // FPLBASE_MATERIAL_H
