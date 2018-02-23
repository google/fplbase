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

#include "fplbase/file_utilities.h"
#include "fplbase/logging.h"

namespace fplbase {

bool FileExistsRaw(const char *filename) {
#if defined(__ANDROID__)
  if (!GetAAssetManager()) {
    LogError(kError,
             "Need to call SetAssetManager() once before calling FileExists()");
    assert(false);
  }
  AAsset *asset =
      AAssetManager_open(GetAAssetManager(), filename, AASSET_MODE_STREAMING);
  if (!asset) {
    return false;
  }
  AAsset_close(asset);
  return true;
#else
  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    return false;
  }
  fclose(fd);
  return true;
#endif
}

bool LoadFileRaw(const char *filename, std::string *dest) {
#if defined(__ANDROID__)
  if (!GetAAssetManager()) {
    LogError(kError,
             "Need to call SetAssetManager() once before calling LoadFile()");
    assert(false);
  }
  AAsset *asset =
      AAssetManager_open(GetAAssetManager(), filename, AASSET_MODE_STREAMING);
  if (!asset) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  off_t len = AAsset_getLength(asset);
  dest->assign(len, 0);
  int rlen = AAsset_read(asset, &(*dest)[0], len);
  AAsset_close(asset);
  return len == rlen && len > 0;
#else
  FILE *fd = fopen(filename, "rb");
  if (fd == NULL) {
    LogError(kError, "LoadFile fail on %s", filename);
    return false;
  }
  if (fseek(fd, 0, SEEK_END)) {
    fclose(fd);
    return false;
  }
  size_t len = ftell(fd);
  fseek(fd, 0, SEEK_SET);
  dest->assign(len, 0);
  size_t rlen = fread(&(*dest)[0], 1, len, fd);
  fclose(fd);
  return len == rlen && len > 0;
#endif
}

bool SaveFile(const char *filename, const void *data, size_t size) {
#if defined(__ANDROID__)
  (void)filename;
  (void)data;
  (void)size;
  LogError(kError, "SaveFile unimplemented on STDLIB on ANDROID.");
  return false;
#else
  FILE *fd = fopen(filename, "wb");
  if (fd == NULL) {
    LogError(kError, "SaveFile fail on %s", filename);
    return false;
  }
  size_t wlen = fwrite(data, 1, size, fd);
  fclose(fd);
  return size == wlen && size > 0;
#endif
}

}  // namespace fplbase
