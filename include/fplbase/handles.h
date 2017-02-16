// Copyright 2016 Google Inc. All rights reserved.
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

#ifndef FPLBASE_HANDLES_H
#define FPLBASE_HANDLES_H

#include <cstdint>

namespace fplbase {
namespace internal {

// Internally-defined. Holds handle from OpenGL, Vulkan, etc.
struct OpaqueHandle {
  uint64_t handle;
  bool operator==(OpaqueHandle rhs) const { return handle == rhs.handle; }
  bool operator!=(OpaqueHandle rhs) const { return !operator==(rhs); }
};

}  // namespace internal

/// @brief Backend agnostic handles to various resources.
///
/// These typedefs are capible of holding OpenGL and Vulkan equivalents.
/// We abstract the handles so that we can present a uniform interface,
/// regardless of which backend is used.
///
/// This is also required for when we allow OpenGL or Vulkan to be selected
/// at runtime, instead of compile time.
typedef internal::OpaqueHandle TextureHandle;
typedef internal::OpaqueHandle TextureTarget;
typedef internal::OpaqueHandle ShaderHandle;
typedef internal::OpaqueHandle UniformHandle;
typedef internal::OpaqueHandle BufferHandle;
typedef internal::OpaqueHandle DeviceMemoryHandle;

// The following functions are backend agnostic.
// Call from any code to get an invalid handle from the current backend.
TextureHandle InvalidTextureHandle();
TextureTarget InvalidTextureTarget();
ShaderHandle InvalidShaderHandle();
UniformHandle InvalidUniformHandle();
BufferHandle InvalidBufferHandle();
DeviceMemoryHandle InvalidDeviceMemoryHandle();

// The following functions are backend agnostic.
// Call from any code to check the validity of a target.
bool ValidTextureHandle(TextureHandle handle);
bool ValidTextureTarget(TextureTarget target);
bool ValidShaderHandle(ShaderHandle handle);
bool ValidUniformHandle(UniformHandle handle);
bool ValidBufferHandle(BufferHandle handle);
bool ValidDeviceMemoryHandle(DeviceMemoryHandle handle);

}  // namespace fplbase

#endif  // FPLBASE_HANDLES_H
