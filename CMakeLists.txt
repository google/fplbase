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
if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "-std=c++11")
endif()

set(fplbase_standalone_mode OFF)
if("${CMAKE_CURRENT_LIST_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
  set(fplbase_standalone_mode ON)
  message(STATUS "FPLBase standalone: building library and samples")
else()
  message(STATUS "FPLBase library: not building samples")
endif()

set(fbx_sdk_msg "The FBX SDK must be installed and the FBX_SDK environment variable must be set to the SDK directory.")
option(fplbase_build_library "Build the fplbase runtime library." ON)
option(fplbase_build_mesh_pipeline
       "Build the mesh_pipeline binary (converts from FBX to FlatBuffers). ${fbx_sdk_msg}"
       OFF)
option(fplbase_build_shader_pipeline
       "Build the shader_pipeline binary (packages GLSL in FlatBuffers)."
       OFF)
option(fplbase_build_samples "Build the fplbase sample executables."
       ${fplbase_standalone_mode})

# Option to enable / disable the test build.
option(fplbase_build_tests "Build tests for this project." ${fplbase_standalone_mode})

# Option to only build flatc
option(fplbase_only_flatc "Only build FlatBuffers compiler." OFF)

# Option to enable debug markers
option(fplbase_debug_markers "Enable OpenGL debug markers." OFF)

# We're on iOS if the system root is set to "iphoneos" or some variant.
if("${CMAKE_OSX_SYSROOT}" MATCHES "iphoneos")
  set(IOS TRUE CACHE BOOL "Target platform is iOS.")
endif()

# Temporary files (like object files) created while compiling projects.
set(tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/obj)

# Look in our local cmake directory for includes before checking other paths.
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Output executables in `bin`, and libraries (both static and module) in `obj`.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${tmp_dir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${tmp_dir})

# Call fplutil to get locations of dependencies and set common build settings.
include("cmake/find_fplutil.cmake")
include("${fplutil_dir}/buildutil/cmake_common.txt")
set_common_build_variables()

set(dependencies_webp_cmake_dir "${CMAKE_MODULE_PATH}/webp"
    CACHE PATH "Directory containing the Webp cmake project.")

if(IOS)
  # iOS builds flatc and SDL externally. Disable building them in the xcodeproj.
  set(fplbase_external_flatc_build_dir "${tmp_dir}/flatbuffers_external")
  set(fplbase_external_flatc "${fplbase_external_flatc_build_dir}/bin/flatc")
  set(FLATBUFFERS_FLATC_EXECUTABLE ${fplbase_external_flatc} CACHE STRING "")
  set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "")

  # Directory and target names for external flatc and SDL.
  set(fplbase_external_sdl_build_dir "${tmp_dir}/sdl_external")
  set(fplbase_external_sdl_lib_simulator_dir
      "${fplbase_external_sdl_build_dir}/lib/iphonesimulator")
  set(fplbase_external_sdl_lib_ios_dir
      "${fplbase_external_sdl_build_dir}/lib/iphoneos")
  set(fplbase_external_sdl_lib_simulator
      "${fplbase_external_sdl_lib_simulator_dir}/libSDL2.a")
  set(fplbase_external_sdl_lib_ios
      "${fplbase_external_sdl_lib_ios_dir}/libSDL2.a")
  set(fplbase_external_sdl_xcode_proj
      "${dependencies_sdl_dir}/Xcode-iOS/SDL/SDL.xcodeproj")
  set(fplbase_use_external_sdl ON)
endif()

# Find or Include FlatBuffers in this project.
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
if(NOT TARGET flatc)
  add_subdirectory("${dependencies_flatbuffers_dir}" ${tmp_dir}/flatbuffers)
endif()
if(fplbase_only_flatc)
  return()
endif()

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

if(NOT fplbase_use_external_sdl)
  # Include SDL in this project while working around SDL requirement of out of
  # tree builds.
  set(SDL_SHARED OFF CACHE BOOL "Build a shared version of the library")
  # (hack) We redefine install to stop SDL's CMakeLists.txt from referring to
  # SDL2 lib
  function(install)
  endfunction(install)
  # For some reason SDL defaults to these not being defined, and then defines
  # their own,
  # causing linker errors.
  add_definitions(-DHAVE_LIBC)
  add_definitions(-DHAVE_STDIO_H)
  set(BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})
  set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/.)
  if(APPLE)
    set(SDL_AUDIO_DRIVER_COREAUDIO 1)
  endif(APPLE)

  if(NOT TARGET SDL2-static)
    set_compiler_flags_for_external_libraries()
    # SDL appends EXTRA_CFLAGS to its CMAKE_CFLAGS
    list(APPEND EXTRA_CFLAGS ${CMAKE_C_FLAGS})

    # Fix bug in SDL CMakeLists.txt that prevents SDL from being built
    # out-of-tree if the calling project is being built in-tree.
    # SDL has to be built out-of-tree, but it checks the global variables
    # (e.g. CMAKE_SOURCE_DIR) instead of the current variables
    # (e.g. CMAKE_CURRENT_SOURCE_DIR), so its check is too strict.
    # We hack the SDL CMakeLists.txt file until it is properly fixed upstream.
    file(COPY cmake/sdl/CMakeLists.txt DESTINATION ${dependencies_sdl_dir})
    set(FORCE_STATIC_VCRT ON CACHE BOOL "")
    add_subdirectory(${dependencies_sdl_dir} ${tmp_dir}/sdl)
    restore_compiler_flags()
  endif()
  set(CMAKE_BINARY_DIR ${BIN_DIR})
endif()

# Include Webp in this project, and force build in release mode for speed.
set(original_build_type ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
if(NOT TARGET webp)
  set_compiler_flags_for_external_libraries()
  add_subdirectory("${dependencies_webp_cmake_dir}" ${tmp_dir}/webp)
  restore_compiler_flags()
endif()
set(CMAKE_BUILD_TYPE ${original_build_type})

if(fplbase_debug_markers)
  add_definitions(-DFPLBASE_ENABLE_DEBUG_MARKERS)
endif()

# Generate source files for all FlatBuffers schema files under the src
# directory.
set(FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/include)
file(GLOB_RECURSE FPLBASE_FLATBUFFERS_SCHEMAS
                  ${CMAKE_CURRENT_LIST_DIR}/schemas/*.fbs)

# Generate rules to build the set of output files from the set of input
# schema files.
build_flatbuffers("${FPLBASE_FLATBUFFERS_SCHEMAS}"
                  ""
                  fplbase_generated_includes
                  ""
                  ${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR}
                  ""
                  "")

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR using_clangxx)
  set(OPENGL_LIBRARIES "GL")

elseif(APPLE)
  if(IOS)
    set(OPENGL_LIBRARIES "-framework OpenGLES")
    set(SYSTEM_LIBRARIES "-framework AudioToolbox"
                         "-framework CoreGraphics"
                         "-framework QuartzCore"
                         "-framework UIKit"
                         "-framework Foundation")
  else()
    find_library(OPENGL_LIBRARIES NAMES GL GLU)
    SET(OPENGL_LIBRARIES "-framework OpenGL")
  endif()
endif()

set(fplbase_common_SRCS
  include/fplbase/asset.h
  include/fplbase/asset_manager.h
  include/fplbase/async_loader.h
  include/fplbase/debug_markers.h
  include/fplbase/environment.h
  include/fplbase/file_utilities.h
  include/fplbase/fpl_common.h
  include/fplbase/glplatform.h
  include/fplbase/gpu_debug.h
  include/fplbase/handles.h
  include/fplbase/input.h
  include/fplbase/internal/type_conversions_gl.h
  include/fplbase/internal/detailed_render_state.h
  include/fplbase/keyboard_keycodes.h
  include/fplbase/logging.h
  include/fplbase/material.h
  include/fplbase/mesh.h
  include/fplbase/preprocessor.h
  include/fplbase/renderer.h
  include/fplbase/renderer_android.h
  include/fplbase/render_state.h
  include/fplbase/render_target.h
  include/fplbase/render_utils.h
  include/fplbase/shader.h
  include/fplbase/texture.h
  include/fplbase/texture_atlas.h
  include/fplbase/utilities.h
  include/fplbase/version.h
  include/fplbase/viewport.h
  schemas
  src/asset_manager.cpp
  src/file_utilities.cpp
  src/gpu_debug_gl.cpp
  src/input.cpp
  src/logging.cpp
  src/material.cpp
  src/mesh_common.cpp
  src/mesh_gl.cpp
  src/mesh_impl_gl.h
  src/precompiled.h
  src/preprocessor.cpp
  src/renderer_common.cpp
  src/renderer_gl.cpp
  src/render_target_common.cpp
  src/render_target_gl.cpp
  src/render_utils_gl.cpp
  src/shader_common.cpp
  src/shader_gl.cpp
  src/texture_common.cpp
  src/texture_gl.cpp
  src/texture_headers.h
  src/type_conversions_gl.cpp
  src/utilities.cpp
  src/version.cpp)

set(fplbase_SRCS
  ${fplbase_common_SRCS}
  src/async_loader_sdl.cpp
  src/environment_sdl.cpp
  src/file_utilities_sdl.cpp
  src/input_sdl.cpp
  src/logging_sdl.cpp
  src/main_sdl.cpp
  src/utilities_sdl.cpp)

set(fplbase_stdlib_SRCS
  ${fplbase_common_SRCS}
  src/async_loader_stdlib.cpp
  src/environment_stdlib.cpp
  src/file_utilities_stdlib.cpp
  src/input_stdlib.cpp
  src/logging_stdlib.cpp
  src/renderer_ios.mm
  src/utilities_stdlib.cpp)

set(fplbase_stb_defines
    STB_IMAGE_IMPLEMENTATION
    STB_IMAGE_RESIZE_IMPLEMENTATION)

# Includes for this project.
include_directories(src)
include_directories(include)
if(WIN32)
  include_directories(${CMAKE_CURRENT_LIST_DIR}/external/include)
endif()
include_directories(${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR})
include_directories(${dependencies_flatbuffers_dir}/include)
include_directories(${dependencies_fplutil_dir}/libfplutil/include)
include_directories(${dependencies_webp_distr_dir}/include)
include_directories(${dependencies_webp_distr_dir}/src)

# SDL includes.
if (fplbase_use_external_sdl)
  include_directories(${dependencies_sdl_dir}/include)
  set(SDL_LIBRARIES
      "${fplbase_external_sdl_lib_simulator};${fplbase_external_sdl_lib_ios}")
else()
  include_directories(${SDL2_BINARY_DIR}/include ${SDL2_SOURCE_DIR}/include)
  set(SDL_LIBRARIES SDL2-static)
  if(WIN32)
    add_definitions(-D_USE_MATH_DEFINES)
    set(OPENGL_LIBRARIES opengl32)
    set(SDL_LIBRARIES SDL2main ${SDL_LIBRARIES})

    if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
      set(fplbase_processor_arch "x64")
    else()
      set(fplbase_processor_arch "x86")
    endif()
    link_directories("$ENV{DXSDK_DIR}/Lib/${fplbase_processor_arch}")
  endif()
endif()

# STB includes.
include_directories(${dependencies_stb_dir})

# Configure the specified target with the common dependencies and flags to
# build fplbase.
function(fplbase_common_config target)
  # Additional flags for the target.
  mathfu_configure_flags(${target})

  # Dependencies for the executable target.
  add_dependencies(${target} fplbase_generated_includes)
  target_link_libraries(${target} ${OPENGL_LIBRARIES} ${SYSTEM_LIBRARIES} webp)
endfunction()

# The stdlib backend requires <mutex> and <thread> headers that were only
# added in C++11, and are not supported by Visual Studio 2010.
# Additionally, newer versions require exceptions to be enabled with
# _HAS_EXCEPTIONS, which we do not want to force on users since it can cause
# code bloat and slowdown.
# Unforunately, there's no easy way to get this compiler capability from
# cmake, at the moment, it seems.
if(NOT MSVC)
  set(compiler_supports_threading 1)
endif()

if(fplbase_build_library)
  add_library(fplbase ${fplbase_SRCS})
  fplbase_common_config(fplbase)
  target_link_libraries(fplbase ${SDL_LIBRARIES})
  target_compile_definitions(fplbase PRIVATE ${fplbase_stb_defines})

  if(compiler_supports_threading)
    add_library(fplbase_stdlib ${fplbase_stdlib_SRCS})
    fplbase_common_config(fplbase_stdlib)
    target_compile_definitions(fplbase_stdlib PRIVATE -DFPLBASE_BACKEND_STDLIB)
    target_compile_definitions(fplbase_stdlib PRIVATE ${fplbase_stb_defines})
  endif()

  # Setup iOS custom build target.
  if(IOS)
    # These commands are executed from within Xcode, which has its own set
    # of environment variables. We use "env -i" to clear the Xcode environment
    # variables, and manually keep the PATH to regular bash path.
    # If we didn't do this, we'd end up building flatc for iOS instead of OSX,
    # since the default Xcode environment variables target iOS when you're in
    # an iOS project.
    set(fplbase_command_line_path "$ENV{PATH}")

    # Build flatc for OSX.
    # Note that the WORKING_DIRECTORY option doesn't create the directory so
    # we need to manually use `mkdir` and `cd` below.
    # The -B and -H options are required because there may already be a cmake
    # cached in the fplbase directory, so `cmake [fplbase]` will trigger a
    # recreation of the existing cmake project, instead of creating a new one
    # for flatc.
    add_custom_command(
      OUTPUT ${fplbase_external_flatc}
      COMMAND (mkdir -p ${fplbase_external_flatc_build_dir} &&
               cd ${fplbase_external_flatc_build_dir} &&
               env -i PATH=${fplbase_command_line_path} cmake
                     -Dfplbase_only_flatc=ON
                     -B${fplbase_external_flatc_build_dir}
                     -H${CMAKE_CURRENT_LIST_DIR} &&
               env -i PATH=${fplbase_command_line_path} make)
      COMMENT "Building flatc (the FlatBuffer schema compiler)")

    # Build SDL libraries for the iOS simulator.
    # SDL does not support iOS in its cmake files, so we have to build it
    # manually from its Xcode project.
    add_custom_command(
      OUTPUT ${fplbase_external_sdl_lib_simulator}
      COMMAND
          (mkdir -p ${fplbase_external_sdl_build_dir} &&
           cd ${fplbase_external_sdl_build_dir} &&
           env -i PATH=${fplbase_command_line_path}  xcodebuild
             -project ${fplbase_external_sdl_xcode_proj}
             -alltargets
             -sdk iphonesimulator
             ARCHS='i386 x86_64'
             CONFIGURATION_BUILD_DIR=${fplbase_external_sdl_lib_simulator_dir}
             OBJROOT=${fplbase_external_sdl_build_dir}/obj
             SYMROOT=${fplbase_external_sdl_build_dir}/sym)
      COMMENT "Building SDL for the iOS simulator")

    # Build SDL libraries for iOS devices.
    add_custom_command(
      OUTPUT ${fplbase_external_sdl_lib_ios}
      COMMAND
          (mkdir -p ${fplbase_external_sdl_build_dir} &&
           cd ${fplbase_external_sdl_build_dir} &&
           env -i PATH=${fplbase_command_line_path} xcodebuild
             -project ${fplbase_external_sdl_xcode_proj}
             -alltargets
             -sdk iphoneos
             ARCHS='arm64 armv7 armv7s'
             CONFIGURATION_BUILD_DIR=${fplbase_external_sdl_lib_ios_dir}
             OBJROOT=${fplbase_external_sdl_build_dir}/obj
             SYMROOT=${fplbase_external_sdl_build_dir}/sym)
      COMMENT "Building SDL for iOS devices")

    # Add a target so that we can run the custom commands before the code build.
    add_custom_target(
      fplbase_ios_prebuilds
      DEPENDS ${fplbase_external_flatc}
              ${fplbase_external_sdl_lib_simulator}
              ${fplbase_external_sdl_lib_ios})

    add_dependencies(fplbase fplbase_ios_prebuilds)
    add_dependencies(fplbase_stdlib fplbase_ios_prebuilds)
    add_dependencies(fplbase_generated_includes fplbase_ios_prebuilds)
  endif()  # IOS
endif()

if(fplbase_build_shader_pipeline)
  set(fplbase_shader_pipeline_SRCS shader_pipeline/shader_pipeline.cpp
                                   shader_pipeline/shader_pipeline_main.cpp)
  include_directories(include)
  include_directories(${FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR})
  include_directories(${dependencies_flatbuffers_dir}/include)
  include_directories(${dependencies_mathfu_dir}/include)
  add_executable(shader_pipeline ${fplbase_shader_pipeline_SRCS})
  target_link_libraries(shader_pipeline fplbase_stdlib)
  fplbase_common_config(shader_pipeline)
endif()

if(fplbase_build_samples)
  add_subdirectory(samples)
endif()

if(fplbase_build_tests)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()

if(fplbase_build_mesh_pipeline)
  add_subdirectory(mesh_pipeline)
endif()

