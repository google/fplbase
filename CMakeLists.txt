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
option(fplbase_build_samples "Build the fplbase sample executables."
       ${fplbase_standalone_mode})

# Option to only build flatc
option(fplbase_only_flatc "Only build FlatBuffers compiler." OFF)
option(fplbase_use_external_sdl2 "Use external SDL library." OFF)
set(external_sdl2_include_dir "" CACHE PATH "Absolute path to directory containing external SDL includes.")
set(external_sdl2_libraries "" CACHE PATH "List of semicolon separated absolute path to SDL libraries.")
option(fplbase_use_host_flatc "Use host machine flatc." OFF)

# Temporary files (like object files) created while compiling projects.
set(tmp_dir ${CMAKE_CURRENT_BINARY_DIR}/obj)

# Look in our local cmake directory for includes before checking other paths.
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Output executables in `bin`, and libraries (both static and module) in `obj`.
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${tmp_dir})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${tmp_dir})

# Directory which contains the source for 3rd party libraries.
if(NOT DEFINED third_party_root)
  get_filename_component(
    third_party_root "${CMAKE_CURRENT_LIST_DIR}/../../../../external"
    REALPATH)
endif()

# Directory which contains source for FPL libraries.
if(NOT DEFINED fpl_root)
  get_filename_component(
      fpl_root "${CMAKE_CURRENT_LIST_DIR}/../../libs" REALPATH)
endif()

# If the dependencies directory exists, assume this is the root directory for
# all libraries required by this project.
if(NOT DEFINED dependencies_root)
  set(dependencies_root "${CMAKE_CURRENT_LIST_DIR}/dependencies")
  if(EXISTS "${dependencies_root}")
    set(third_party_root "${dependencies_root}")
    set(fpl_root "${dependencies_root}")
  endif()
endif()

# Configurable locations of dependencies of this project.
set(dependencies_gtest_dir "${fpl_root}/googletest"
    CACHE PATH "Directory containing the GoogleTest library.")
set(dependencies_flatbuffers_dir "${fpl_root}/flatbuffers"
    CACHE PATH "Directory containing the Flatbuffers library.")
set(dependencies_flatbuffers_cmake_dir "${fpl_root}/flatbuffers/CMake"
    CACHE PATH "Directory containing the Flatbuffers library cmake helpers.")
set(dependencies_fplutil_dir "${fpl_root}/fplutil"
    CACHE PATH "Directory containing the fplutil library.")
set(dependencies_mathfu_dir "${fpl_root}/mathfu"
    CACHE PATH "Directory containing the MathFu library.")
set(dependencies_sdl_dir "${third_party_root}/sdl"
    CACHE PATH "Directory containing the SDL library.")
set(dependencies_webp_cmake_dir "${CMAKE_MODULE_PATH}/webp"
    CACHE PATH "Directory containing the Webp cmake project.")
set(dependencies_webp_distr_dir "${third_party_root}/webp"
    CACHE PATH "Directory containing the Webp distribution.")

# Detect clang
if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang" OR
   CMAKE_CXX_COMPILER_ID MATCHES ".*clang")
  set(CMAKE_COMPILER_IS_CLANGXX 1)
endif()

# Include MathFu in this project with test and benchmark builds disabled.
set(mathfu_build_benchmarks OFF CACHE BOOL "")
set(mathfu_build_tests OFF CACHE BOOL "")
add_subdirectory(${dependencies_mathfu_dir} ${tmp_dir}/mathfu)

if(NOT fplbase_use_external_sdl2)
  # External code should be compiled with these compiler options.
  # Call this function before add_subdirectory([external project]), and then
  # call restore_comiler_flags() after.
  function(set_compiler_flags_for_external_libraries)
    # Save current compiler flags so that we can restore them in
    # restore_compiler_flags()
    set(FPLBASE_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    set(FPLBASE_SAVED_C_FLAGS "${CMAKE_C_FLAGS}" PARENT_SCOPE)

    # Suppress all warnings.
    if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR
       CMAKE_COMPILER_IS_CLANGXX)
      set(CMAKE_CXX_FLAGS "-std=c++0x -w -Wno-deprecated-declarations" PARENT_SCOPE)
      set(CMAKE_C_FLAGS "-w" PARENT_SCOPE)
    elseif(MSVC)
      set(CMAKE_CXX_FLAGS "/W4" PARENT_SCOPE)
      set(CMAKE_C_FLAGS "" PARENT_SCOPE)
    endif()
  endfunction()

  # Restore the compiler flags to the values they had before
  # set_compiler_flags_for_external_libraries() was called.
  function(restore_compiler_flags)
    set(CMAKE_CXX_FLAGS "${FPLBASE_SAVED_CXX_FLAGS}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS "${FPLBASE_SAVED_C_FLAGS}" PARENT_SCOPE)
  endfunction()

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

  if(NOT fplbase_only_flatc AND NOT TARGET SDL2-static)
    set_compiler_flags_for_external_libraries()
    add_subdirectory(${dependencies_sdl_dir} ${tmp_dir}/sdl)
    restore_compiler_flags()
  endif()
  set(CMAKE_BINARY_DIR ${BIN_DIR})
endif()

# Include Webp in this project, and force build in release mode for speed.
set(original_build_type ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
if(NOT fplbase_only_flatc AND NOT TARGET webp)
  add_subdirectory("${dependencies_webp_cmake_dir}" ${tmp_dir}/webp)
endif()
set(CMAKE_BUILD_TYPE ${original_build_type})

# Find or Include FlatBuffers in this project.
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")

if(fplbase_use_host_flatc AND NOT fplbase_only_flatc)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
      "${dependencies_flatbuffers_cmake_dir}")
  find_package(FlatBuffers REQUIRED)
endif()

if(NOT TARGET flatc AND NOT FLATBUFFERS_FLATC_EXECUTABLE)
  add_subdirectory("${dependencies_flatbuffers_dir}" ${tmp_dir}/flatbuffers)
endif()

# Generate source files for all FlatBuffers schema files under the src
# directory.
set(FPLBASE_FLATBUFFERS_GENERATED_INCLUDES_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}${CMAKE_FILES_DIRECTORY}/include)
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

if(APPLE)
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
  include/fplbase/asset_manager.h
  include/fplbase/async_loader.h
  include/fplbase/fpl_common.h
  include/fplbase/glplatform.h
  include/fplbase/input.h
  include/fplbase/keyboard_keycodes.h
  include/fplbase/material.h
  include/fplbase/mesh.h
  include/fplbase/renderer.h
  include/fplbase/renderer_android.h
  include/fplbase/render_target.h
  include/fplbase/shader.h
  include/fplbase/texture.h
  include/fplbase/utilities.h
  include/fplbase/version.h
  schemas
  src/asset_manager.cpp
  src/material.cpp
  src/mesh.cpp
  src/precompiled.cpp
  src/precompiled.h
  src/renderer.cpp
  src/render_target.cpp
  src/shader.cpp
  src/texture.cpp
  src/utilities.cpp
  src/version.cpp)

set(fplbase_SRCS
  ${fplbase_common_SRCS}
  src/async_loader.cpp
  src/input.cpp
  src/main.cpp)

set(fplbase_stdlib_SRCS
  ${fplbase_common_SRCS}
  src/async_loader_stdlib.cpp)

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
if (NOT fplbase_use_external_sdl2)
  include_directories(${SDL2_BINARY_DIR}/include ${SDL2_SOURCE_DIR}/include)
else()
  include_directories(${external_sdl2_include_dir})
endif()

# Compiler flags.
set(C_FLAGS_WARNINGS "")
if(MSVC)
  # C4127: conditional expression is constant
  set(C_FLAGS_WARNINGS "/W4 /wd4127")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR
       CMAKE_COMPILER_IS_CLANGXX)
  add_definitions(-g)
  set(CMAKE_CXX_FLAGS "-std=c++0x -pedantic -Wno-unused-function")
  # -Wno-unused-function is because of webp encode.h
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS_WARNINGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS_WARNINGS}")

if (NOT fplbase_use_external_sdl2)
  set (SDL_LIBRARIES SDL2-static)
  if(WIN32)
    add_definitions(-D_USE_MATH_DEFINES)
    set(OPENGL_LIBRARIES opengl32)
    set(SDL_LIBRARIES SDL2main ${SDL_LIBRARIES})
    link_directories("$ENV{DXSDK_DIR}/Lib/$ENV{PROCESSOR_ARCHITECTURE}")
  endif()
else()
  set(SDL_LIBRARIES ${external_sdl2_libraries})
endif()

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
# Unforunately, there's no easy way to get this compiler capability from
# cmake, at the moment, it seems.
if(NOT MSVC OR MSVC_VERSION GREATER 1600)
  set(compiler_supports_threading 1)
endif()

if(fplbase_build_library)
  add_library(fplbase ${fplbase_SRCS})
  fplbase_common_config(fplbase)
  target_link_libraries(fplbase ${SDL_LIBRARIES})

  if(compiler_supports_threading)
    add_library(fplbase_stdlib ${fplbase_stdlib_SRCS})
    fplbase_common_config(fplbase_stdlib)
    target_compile_definitions(fplbase_stdlib PRIVATE -DFPL_BASE_BACKEND_STDLIB)
  endif()
endif()

if(fplbase_build_mesh_pipeline)
  # Autodesk's FBX SDK must be installed to build the pipeline.
  set(fbx_sdk_dir $ENV{FBX_SDK})
  if("${fbx_sdk_dir}" STREQUAL "")
    message(FATAL_ERROR ${fbx_sdk_msg})
  endif()

  # We check-in the mesh_pipeline executible, so we need to output it to
  # a platform-specific location.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${CMAKE_SYSTEM_NAME})

  # Search for the FBX library.
  # Visual Studio 64 bit option can be specified with
  # `cmake -G "Visual Studio 11 Win64"`
  if(CMAKE_CL_64)
    set(fbs_sdk_subdir "x64")
  else()
    set(fbs_sdk_subdir "x86")
  endif()

  # Prefer the static library '.a' over the sharedlibrary '.so'.
  find_library(fbx_sdk_lib_release
    NAMES libfbxsdk.a fbxsdk libfbxsdk-md.lib
    PATHS ${fbx_sdk_dir}/lib/gcc4/x64/release
          ${fbx_sdk_dir}/lib/clang/release
          ${fbx_sdk_dir}/lib/vs2008/${fbs_sdk_subdir}/release
          ${fbx_sdk_dir}/lib/vs2010/${fbs_sdk_subdir}/release
          ${fbx_sdk_dir}/lib/vs2012/${fbs_sdk_subdir}/release
          ${fbx_sdk_dir}/lib/vs2013/${fbs_sdk_subdir}/release)

  find_library(fbx_sdk_lib_debug
    NAMES libfbxsdk.a fbxsdk libfbxsdk-md.lib
    PATHS ${fbx_sdk_dir}/lib/gcc4/x64/debug
          ${fbx_sdk_dir}/lib/clang/debug
          ${fbx_sdk_dir}/lib/vs2008/${fbs_sdk_subdir}/debug
          ${fbx_sdk_dir}/lib/vs2010/${fbs_sdk_subdir}/debug
          ${fbx_sdk_dir}/lib/vs2012/${fbs_sdk_subdir}/debug
          ${fbx_sdk_dir}/lib/vs2013/${fbs_sdk_subdir}/debug)

  set(fbx_sdk_lib debug ${fbx_sdk_lib_debug} optimized ${fbx_sdk_lib_release})

  # Linux and Mac require these libs too.
  if(NOT MSVC)
    set(fbx_sdk_extra_libs pthread dl)
  endif()

  # Suppress deprecation of fopen.
  if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  endif()

  # We use file_util.h from fplutil.
  set(fplutil_build_tests OFF)
  add_subdirectory("${dependencies_fplutil_dir}/libfplutil"
                   ${tmp_dir}/fplutil)

  # Source files for the pipeline.
  set(fplbase_mesh_pipeline_SRCS mesh_pipeline/mesh_pipeline.cpp)

  # Add the location of #include <fbxsdk.h>.
  include_directories(${fbx_sdk_dir}/include)

  # These warnings come from the FBX SDK, and they can't be suppressed
  # by pragmas around #include <fbxsdk.h>, so suppress them here.
  if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS
        "-std=c++0x -Wno-literal-suffix -Wno-comment ${C_FLAGS_WARNINGS}")
  endif()

  # Needed by the FBX SDK when we call some file manipulation functions.
  if(APPLE)
    set(CMAKE_CXX_LINK_FLAGS "-framework Carbon")
  endif()

  # Statically link the C runtime .so files to make binary more portable.
  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(CMAKE_CXX_LINK_FLAGS "-static")
  endif()

  # Create the executable for mesh_pipeline.
  add_executable(mesh_pipeline ${fplbase_mesh_pipeline_SRCS})

  # Link the executable with the FBX SDK and the static libraries that the
  # FBX SDK needs.
  target_link_libraries(mesh_pipeline
                        ${fbx_sdk_lib}
                        ${fbx_sdk_extra_libs}
                        fplutil)

  # Additional flags for the target.
  fplbase_common_config(mesh_pipeline)
endif()

if(fplbase_build_samples)
  add_subdirectory(samples)
endif()
