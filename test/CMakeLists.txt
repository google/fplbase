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

project(fplbase_test)

# TODO: This should be replaced with a call to `set_common_build_variables` in
# fplutil/buildutil/cmake_common.txt. Currently that script is hard coded to
# only work from within top level directory.
if(NOT DEFINED third_party_root)
  get_filename_component(third_party_root
    "${fpl_root}/../../../external" REALPATH)
endif()

# If the dependencies directory exists, assume this is the root directory
# for all libraries required by this project.
if(NOT DEFINED dependencies_root)
  set(dependencies_root "${fpl_root}/fplbase/dependencies")
  if(EXISTS "$dependencies_root}")
    set(third_party_root "${dependencies_root}")
  endif()
endif()

#TODO: Remove this once the `dependencies_gtest_dir` is updated in the other
# libraries to use the new `external/googletest` path.
set(dependencies_gtest_dir ${third_party_root}/googletest)

# gtest seems to prefer the non-DLL runtime on Windows, which conflicts with
# everything else.
option(
  gtest_force_shared_crt
  "Use shared (DLL) run-time lib even when Google Test is built as static lib."
  ON)

# Import gtest if it's not already present.
if(NOT TARGET gtest)
	add_subdirectory(${dependencies_gtest_dir} ${tmp_dir}/googletest)
endif()

# Set some variables describing file locations.
set(GUNIT_INCDIR "${dependencies_gtest_dir}/googletest/include")
set(GTEST_LIBDIR "${dependencies_gtest_dir}/googletest")

# This is the directory into which the executables are built.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

include_directories(${GUNIT_INCDIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}
                    ${fpl_root/fplbase}
                    ${fpl_root}/mathfu/include)

# Include helper functions and macros used by Google Test.
include(${GTEST_LIBDIR}/cmake/internal_utils.cmake)
config_compiler_and_linker()
string(REPLACE "-W4" "-W3" cxx_default "${cxx_default}")
string(REPLACE "-Wshadow" "" cxx_default "${cxx_default}")
string(REPLACE "-Wextra" "" cxx_default "${cxx_default}")

# Common libraries for tests.
if(NOT MSVC)
  find_package(Threads)
endif()

set(COMMON_LIBS "gtest;fplbase;${CMAKE_THREAD_LIBS_INIT}")

set(fplbase_common_SRCS
  ../include/fplbase/asset.h
  ../include/fplbase/asset_manager.h
  ../include/fplbase/async_loader.h
  ../include/fplbase/fpl_common.h
  ../include/fplbase/glplatform.h
  ../include/fplbase/input.h
  ../include/fplbase/keyboard_keycodes.h
  ../include/fplbase/material.h
  ../include/fplbase/mesh.h
  ../include/fplbase/preprocessor.h
  ../include/fplbase/renderer.h
  ../include/fplbase/renderer_android.h
  ../include/fplbase/render_target.h
  ../include/fplbase/shader.h
  ../include/fplbase/texture.h
  ../include/fplbase/texture_atlas.h
  ../include/fplbase/utilities.h
  ../include/fplbase/version.h
  ../schemas
  ../src/input.cpp
  ../src/asset_manager.cpp
  ../src/material.cpp
  ../src/mesh.cpp
  ../src/precompiled.h
  ../src/preprocessor.cpp
  ../src/renderer.cpp
  ../src/render_target.cpp
  ../src/shader.cpp
  ../src/texture.cpp
  ../src/utilities.cpp
  ../src/version.cpp)

set(fplbase_SRCS
  ${fplbase_common_SRCS}
  ../src/async_loader.cpp
  ../src/input_sdl.cpp
  ../src/main.cpp)

# PUT ADDITIONAL UNIT TEST BINARIES BELOW!
# The commands should be of the form:
#
# test_executable(<test-name>)
#
# Where <test-name> is the name of the output test executable and the basename
# of the source file for the test.  For example, test_executable(CallbackTests)
# generates an executable called CallbackTests which is the result of compiling
# CallbackTests.cpp along with any files specified by the SUPPORT_SRCS variable.

function(test_executable name)
  cxx_executable_with_flags(${name}_test "${cxx_default}" "${COMMON_LIBS}"
      ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/${name}_test.cpp ${fplbase_SRCS}
      ${ARGN})
  mathfu_configure_flags(${name}_test)
endfunction()

test_executable(preprocessor)
