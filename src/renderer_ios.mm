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

#import "fplbase/internal/renderer_ios.h"

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR

#import <OpenGLES/EAGL.h>

static_assert(
    kEAGLRenderingAPIOpenGLES1 == 1 &&
    kEAGLRenderingAPIOpenGLES2 == 2 &&
    kEAGLRenderingAPIOpenGLES3 == 3,
    "EAGL API versions don't match.");

namespace fplbase {

int IosGetContextClientVersion() {
  EAGLContext* context = [EAGLContext currentContext];
  assert(context);
  return static_cast<int>(context.API);
}

}  // namespace fplbase

#endif  // TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
