// Copyright 2017 Google Inc. All rights reserved.
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

#ifndef FPLBASE_INTERNAL_HANDLE_CONVERSIONS_H
#define FPLBASE_INTERNAL_HANDLE_CONVERSIONS_H

#include "fplbase/handles.h"

namespace fplbase {

union HandleUnion {
  HandleUnion() { handle.handle = 0; }
  explicit HandleUnion(internal::OpaqueHandle handle) : handle(handle) {}
  explicit HandleUnion(unsigned int gl_param) {
    handle.handle = 0; // Clear all the memory first.
    gl = gl_param;
  }
  explicit HandleUnion(uint64_t vk) : vk(vk) {}

  // Opaque handle used in external API.
  internal::OpaqueHandle handle;

  // OpenGL handles, unsigned and signed.
  unsigned int gl;
  int gl_int;

  // Vulkan handles.
  uint64_t vk;
  int32_t vk32;
};

// Call from OpenGL code to convert between GL and external API handles.
inline TextureHandle TextureHandleFromGl(unsigned int gl) {
  return HandleUnion(gl).handle;
}

inline TextureTarget TextureTargetFromGl(unsigned int gl) {
  return HandleUnion(gl).handle;
}

inline ShaderHandle ShaderHandleFromGl(unsigned int gl) {
  return HandleUnion(gl).handle;
}

inline UniformHandle UniformHandleFromGl(int gl_int) {
  HandleUnion u;
  u.gl_int = gl_int;
  return u.handle;
}

inline BufferHandle BufferHandleFromGl(unsigned int gl) {
  return HandleUnion(gl).handle;
}

inline unsigned int GlTextureHandle(TextureHandle handle) {
  return HandleUnion(handle).gl;
}

inline unsigned int GlTextureTarget(TextureTarget handle) {
  return HandleUnion(handle).gl;
}

inline unsigned int GlShaderHandle(ShaderHandle handle) {
  return HandleUnion(handle).gl;
}

inline int GlUniformHandle(UniformHandle handle) {
  HandleUnion u;
  u.handle = handle;
  return u.gl_int;
}

inline unsigned int GlBufferHandle(BufferHandle handle) {
  return HandleUnion(handle).gl;
}

// Call from Vulkan code to convert between Vulkan and external API handles.
inline TextureHandle TextureHandleFromVk(uint64_t vk) {
  return HandleUnion(vk).handle;
}

inline TextureTarget TextureTargetFromVk(uint64_t vk) {
  return HandleUnion(vk).handle;
}

inline ShaderHandle ShaderHandleFromVk(uint64_t vk) {
  return HandleUnion(vk).handle;
}

inline UniformHandle UniformHandleFromVk(int32_t vk32) {
  HandleUnion u;
  u.vk32 = vk32;
  return u.handle;
}

inline BufferHandle BufferHandleFromVk(uint64_t vk) {
  return HandleUnion(vk).handle;
}

inline DeviceMemoryHandle DeviceMemoryHandleFromVk(uint64_t vk) {
  return HandleUnion(vk).handle;
}

inline uint64_t VkTextureHandle(TextureHandle handle) {
  return HandleUnion(handle).vk;
}

inline uint64_t VkTextureTarget(TextureTarget handle) {
  return HandleUnion(handle).vk;
}

inline uint64_t VkShaderHandle(ShaderHandle handle) {
  return HandleUnion(handle).vk;
}

inline int32_t VkUniformHandle(UniformHandle handle) {
  HandleUnion u;
  u.handle = handle;
  return u.vk32;
}

inline uint64_t VkBufferHandle(BufferHandle handle) {
  return HandleUnion(handle).vk;
}

inline uint64_t VkDeviceMemoryHandle(DeviceMemoryHandle handle) {
  return HandleUnion(handle).vk;
}

}  // namespace fplbase

#endif  // FPLBASE_INTERNAL_HANDLE_CONVERSIONS_H
