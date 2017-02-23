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

#ifndef FPLBASE_TEXTURE_HEADERS_H
#define FPLBASE_TEXTURE_HEADERS_H

#include <cstdint>

namespace fplbase {

struct ASTCHeader {
  uint8_t magic[4];  // 13 ab a1 5c
  uint8_t blockdim_x;
  uint8_t blockdim_y;
  uint8_t blockdim_z;
  uint8_t xsize[3];
  uint8_t ysize[3];
  uint8_t zsize[3];
};

struct PKMHeader {
  char magic[4];          // "PKM "
  char version[2];        // "10"
  uint8_t data_type[2];   // 0 (ETC1_RGB_NO_MIPMAPS)
  uint8_t ext_width[2];   // rounded up to 4 size, big endian.
  uint8_t ext_height[2];  // rounded up to 4 size, big endian.
  uint8_t width[2];       // original, big endian.
  uint8_t height[2];      // original, big endian.
  // Data follows header, size = (ext_width / 4) * (ext_height / 4) * 8
};

struct KTXHeader {
  char id[12];  // "«KTX 11»\r\n\x1A\n"
  uint32_t endian;
  uint32_t type;
  uint32_t type_size;
  uint32_t format;
  uint32_t internal_format;
  uint32_t base_internal_format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_elements;
  uint32_t faces;
  uint32_t mip_levels;
  uint32_t keyvalue_data;
};

}  // namespace fplbase

#endif  // FPLBASE_TEXTURE_HEADERS_H
