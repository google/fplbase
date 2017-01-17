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

project(fplbase-samples)

function(fplbase_sample target)
  set(srcs "${target}/${target}.cpp")
  set(assets_dir "${CMAKE_CURRENT_SOURCE_DIR}/${target}/assets")

  # Add OpenGL target.
  set(target_gl "sample_${target}_gl")

  if(IOS)
    file(GLOB assets "${assets_dir}/*")
    set_source_files_properties(${assets}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    add_executable(${target_gl} ${srcs} ${assets})

    set_target_properties(${target_gl}
        PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "${target_gl}")
    set_target_properties(${target_gl}
        PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "${target_gl}")
  else()
    add_executable(${target_gl} ${srcs})

    if(EXISTS ${assets_dir})
      add_custom_command(
          TARGET ${target_gl} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${assets_dir}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")
    endif()
  endif()

  add_dependencies(${target_gl} fplbase)
  mathfu_configure_flags(${target_gl})
  target_link_libraries(${target_gl} fplbase)
endfunction()

fplbase_sample("triangle")
fplbase_sample("texture")
fplbase_sample("mesh")

