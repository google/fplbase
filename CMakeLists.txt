# Copyright 2014 Google Inc. All rights reserved.
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

project(fplbase)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# Temporary files (like object files) created while compiling projects.
set(tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/obj)

# Directory which contains the source for 3rd party libraries.
get_filename_component(
    third_party_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../../external" REALPATH)
# Directory which contains source for FPL libraries.
get_filename_component(
    fpl_root "${CMAKE_CURRENT_SOURCE_DIR}/../../libs" REALPATH)

# If the dependencies directory exists, assume this is the root directory for
# all libraries required by this project.
set(fplbase_dependencies_root "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
if(EXISTS "${dependencies_root}")
  set(third_party_root "${fplbase_dependencies_root}")
  set(fpl_root "${fplbase_dependencies_root}")
endif()

# Configurable locations of dependencies of this project.
set(fplbase_dependencies_gtest_dir "${fpl_root}/googletest"
    CACHE PATH "Directory containing the GoogleTest library.")
set(fplbase_dependencies_flatbuffers_dir "${fpl_root}/flatbuffers"
    CACHE PATH "Directory containing the Flatbuffers library.")
set(fplbase_dependencies_fplutil_dir "${fpl_root}/fplutil"
    CACHE PATH "Directory containing the fplutil library.")
set(fplbase_dependencies_mathfu_dir "${fpl_root}/mathfu"
    CACHE PATH "Directory containing the MathFu library.")
set(fplbase_dependencies_sdl_dir "${third_party_root}/sdl"
    CACHE PATH "Directory containing the SDL library.")
set(fplbase_dependencies_webp_cmake_dir "${CMAKE_MODULE_PATH}/webp"
    CACHE PATH "Directory containing the Webp cmake project.")
set(fplbase_dependencies_webp_distr_dir "${third_party_root}/webp"
    CACHE PATH "Directory containing the Webp distribution.")

# Apparently the webp cmake file needs this to be cached before it can run?
set(dependencies_webp_distr_dir "${third_party_root}/webp"
    CACHE PATH "Directory containing the Webp distribution.")
set(dependencies_mathfu_dir "${fpl_root}/mathfu"
    CACHE PATH "Directory containing the MathFu library.")
set(dependencies_flatbuffers_dir "${fpl_root}/flatbuffers"
    CACHE PATH "Directory containing the Flatbuffers library.")

# Option to only build flatc
option(fplbase_only_flatc "Only build FlatBuffers compiler." OFF)

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
#add_subdirectory(${fplbase_dependencies_mathfu_dir} ${tmp_dir}/mathfu)
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)


# Include SDL in this project while working around SDL requirement of out of
# tree builds.
set(SDL_SHARED OFF CACHE BOOL "Build a shared version of the library")
# (hack) We redefine install to stop SDL's CMakeLists.txt from referring to SDL2 lib
function(install)
endfunction(install)
# For some reason SDL defaults to these not being defined, and then defines their own,
# causing linker errors.
add_definitions(-DHAVE_LIBC)
add_definitions(-DHAVE_STDIO_H)
set(BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.)
if(APPLE)
  set(SDL_AUDIO_DRIVER_COREAUDIO 1)
endif(APPLE)

if(NOT fplbase_only_flatc AND NOT TARGET SDL2-static)
  add_subdirectory(${fplbase_dependencies_sdl_dir} ${tmp_dir}/sdl)
endif()

set(CMAKE_BINARY_DIR ${BIN_DIR})

# Include Webp in this project, and force build in release mode for speed.
set(original_build_type ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
if(NOT fplbase_only_flatc AND NOT TARGET webp)
  add_subdirectory("${fplbase_dependencies_webp_cmake_dir}" ${tmp_dir}/webp)
endif()
set(CMAKE_BUILD_TYPE ${original_build_type})

# Include FlatBuffers in this project.
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
if(NOT TARGET flatc)
  add_subdirectory("${dependencies_flatbuffers_dir}" ${tmp_dir}/flatbuffers)
endif()

# Generate source files for all FlatBuffers schema files under the src
# directory.
set(FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}${CMAKE_FILES_DIRECTORY}/include)
set(FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR} PARENT_SCOPE)
file(GLOB_RECURSE FLATBUFFERS_SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.fbs)

# Generate rules to build the set of output files from the set of input
# schema files.
foreach(flatbuffers_schema ${FLATBUFFERS_SCHEMAS})
  get_filename_component(filename ${flatbuffers_schema} NAME_WE)
  set(flatbuffers_include
      ${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR}/${filename}_generated.h)
  add_custom_command(
    OUTPUT ${flatbuffers_include}
    COMMAND flatc --gen-includes -o ${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR} -c ${flatbuffers_schema}
    DEPENDS flatc ${flatbuffers_schema})
  list(APPEND FLATBUFFERS_GENERATED_INCLUDES ${flatbuffers_include})
endforeach()
add_custom_target(fplbase_generated_includes DEPENDS ${FLATBUFFERS_GENERATED_INCLUDES})

# Build rule that uses make to build the assets.
get_property(flatc_location TARGET flatc PROPERTY LOCATION)
add_custom_target(fplbase_assets
  COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_assets.py
                 --flatc ${flatc_location} --output ${CMAKE_BINARY_DIR}/fplbase_assets
  DEPENDS flatc ${CWEBP_TARGET})


if(APPLE)
  find_library(OPENGL_LIBRARIES NAMES GL GLU)
  SET(CMAKE_CXX_LINK_FLAGS "-framework OpenGL")
endif()


set(fplbase_SRCS
  include/async_loader.h
  include/fpl_common.h
  include/glplatform.h
  include/input.h
  include/material.h
  include/material_manager.h
  include/mesh.h
  include/renderer.h
  include/renderer_android.h
  include/shader.h
  include/utilities.h
  schemas
  src/async_loader.cpp
  src/input.cpp
  src/main.cpp
  src/material.cpp
  src/material_manager.cpp
  src/mesh.cpp
  src/precompiled.cpp
  src/precompiled.h
  src/renderer.cpp
  src/shader.cpp
  src/utilities.cpp)


# Includes for this project.
include_directories(src)
include_directories(include)
if(WIN32)
  include_directories(external/include/windows)
endif()
include_directories(${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR})
include_directories(${dependencies_flatbuffers_dir}/include)
include_directories(${fplbase_dependencies_fplutil_dir}/libfplutil/include)
include_directories(${fplbase_dependencies_webp_distr_dir}/include)
include_directories(${fplbase_dependencies_webp_distr_dir}/src)

# SDL includes.
include_directories(${SDL2_BINARY_DIR}/include ${SDL2_SOURCE_DIR}/include)

# Detect clang
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()

# Compiler flags.
set(C_FLAGS_WARNINGS "")
if(MSVC)
  set(C_FLAGS_WARNINGS "/W4 /WX")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR
       CMAKE_COMPILER_IS_CLANGXX)
  add_definitions(-g)
  set(CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -std=c++0x -Wall -pedantic -Werror -Wextra -Wno-unused-function")
  # -Wno-unused-function is because of webp encode.h
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS_WARNINGS}")

set (SDL_LIBRARIES SDL2-static)
if(WIN32)
  add_definitions(-D_USE_MATH_DEFINES)
  set(OPENGL_LIBRARIES opengl32)
  set(SDL_LIBRARIES SDL2main ${SDL_LIBRARIES})
  link_directories("$ENV{DXSDK_DIR}/Lib/$ENV{PROCESSOR_ARCHITECTURE}")
endif()


add_library (fplbase ${fplbase_SRCS})

# Additional flags for the target.
mathfu_configure_flags(fplbase)
# Dependencies for the executable target.
add_dependencies(fplbase fplbase_generated_includes fplbase_assets)
target_link_libraries(fplbase
  ${SDL_LIBRARIES}
  ${OPENGL_LIBRARIES}
  webp)
