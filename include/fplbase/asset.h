// Copyright 2015 Google Inc. All rights reserved.
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

#ifndef FPLBASE_ASSET_H
#define FPLBASE_ASSET_H

#include <assert.h>

namespace fplbase {

class AssetManager;

/// @class Asset
/// @brief Base class of all assets that _may_ be managed by Assetmanager.
class Asset {
 public:
  Asset() : refcount_(1) {}
  virtual ~Asset() {}

  /// @brief indicate there is an additional owner of this asset.
  /// By default, when you call any of the UnLoad*() functions in the
  /// AssetManager, that will directly delete the asset since they all start
  /// out with a single reference count. Call this function to indicate
  /// multiple owners will call Unload*() independently, and only have the
  /// asset deleted by the last one.
  void IncreaseRefCount() { refcount_++; }

 private:
  // This is private, since only the AssetManager can delete assets.
  friend class AssetManager;
  int DecreaseRefCount() {
    assert(refcount_ > 0);
    return --refcount_;
  };

  int refcount_;
};

}  // namespace fplbase

#endif  // FPLBASE_ASSET_H
