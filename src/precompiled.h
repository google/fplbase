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

#ifndef FPLBASE_PRECOMPILED_H
#define FPLBASE_PRECOMPILED_H

#include "fplbase/config.h"  // Must come first.

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <cstdint>

#include <functional>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <queue>
#include <algorithm>

#if defined(_WIN32)
#include <direct.h>  // for _chdir
#else                // !defined(_WIN32)
#include <unistd.h>  // for chdir
#endif               // !defined(_WIN32)

#include "flatbuffers/util.h"

#include "mathfu/matrix.h"
#include "mathfu/vector.h"
#include "mathfu/glsl_mappings.h"
#include "mathfu/quaternion.h"
#include "mathfu/constants.h"
#include "mathfu/utilities.h"

#ifdef FPLBASE_BACKEND_SDL
#include "SDL.h"
#include "SDL_log.h"
#endif

#include "fplbase/glplatform.h"

#if defined(_WIN32) && !defined(__clang__)
#pragma hdrstop
#endif  //  _WIN32

#endif  // FPLBASE_PRECOMPILED_H
