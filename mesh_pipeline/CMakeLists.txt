# Copyright 2016 Google Inc. All rights reserved.
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

project(mesh_pipeline)

# We check-in the mesh_pipeline executible, so we need to output it to
# a platform-specific location.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_SYSTEM_NAME})

# Configurable locations of dependencies of this project.
if(NOT DEFINED fpl_root)
  get_filename_component(fpl_root "${CMAKE_CURRENT_LIST_DIR}/../../libs"
       REALPATH)
  if(NOT EXISTS "${fpl_root}")
    set(fpl_root "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
  endif ()
endif()

# Include functions fbx_compile_options() and fbx_configure_target()
include(${fpl_root}/fplutil/fbx_common/cmake_fbx.txt)

# We use file_util.h from fplutil.
set(fplutil_build_tests OFF CACHE BOOL "")
add_subdirectory("${dependencies_fplutil_dir}/libfplutil"
                 ${tmp_dir}/fplutil)

# Add the common FBX library.
add_subdirectory(${dependencies_fplutil_dir}/fbx_common ${tmp_dir}/fbx_common)

# Source files for the pipeline.
set(fplbase_mesh_pipeline_SRCS mesh_pipeline.cpp mesh_pipeline_main.cpp)

# Set compile options for FBX programs.
fbx_compile_options()

# Create the executable for mesh_pipeline.
add_executable(mesh_pipeline ${fplbase_mesh_pipeline_SRCS})

# Set further options for FBX programs.
fbx_configure_target(mesh_pipeline)
target_link_libraries(mesh_pipeline fplutil)

# Additional flags for the target.
mathfu_configure_flags(mesh_pipeline)

