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

#ifndef FPL_COMMON_H
#define FPL_COMMON_H

namespace fplbase {

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
//
// For disallowing only assign or copy, write the code directly, but declare
// the intend in a comment, for example:
// void operator=(const TypeName&);  // DISALLOW_ASSIGN
// Note, that most uses of DISALLOW_ASSIGN and DISALLOW_COPY are broken
// semantically, one should either use disallow both or neither. Try to
// avoid these in new code.
#define FPL_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                   \
  void operator=(const TypeName&)

// Return the number of elements in an array 'a', as type `size_t`.
// If 'a' is not an array, generates an error by dividing by zero.
#define FPL_ARRAYSIZE(a)        \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

// Clang generates warnings if the fallthrough attribute is not specified.
#if defined(__clang__) && defined(__has_warning)
#if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#define FPL_FALLTHROUGH_INTENDED [[clang::fallthrough]];  // NOLINT
#endif
#endif
#if !defined(FPL_FALLTHROUGH_INTENDED)
#define FPL_FALLTHROUGH_INTENDED
#endif

}  // namespace fplbase

#endif  // FPL_COMMON_H
