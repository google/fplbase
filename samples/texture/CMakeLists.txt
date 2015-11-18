# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
cmake_minimum_required(VERSION 2.8.12)

project(fplbase-texture)

if(IOS)
  file(GLOB ASSETS "assets/*")
  set_source_files_properties(${ASSETS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  add_executable(fplbase-texture texture.cpp ${ASSETS})
  set_target_properties(fplbase-texture
    PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "fplbase-texture")
  set_target_properties(fplbase-texture
    PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "fplbase-texture")
else()
  add_executable(fplbase-texture texture.cpp)
  file(COPY assets DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

add_dependencies(fplbase-texture fplbase)
mathfu_configure_flags(fplbase-texture)
target_link_libraries(fplbase-texture fplbase)
