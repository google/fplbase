Building mesh_pipeline    {#fplbase_guide_building_mesh_pipeline}
======================

[mesh_pipeline] converts meshes made with modeling tools such as [Maya] and
[Blender] into a fast runtime format.

[fplbase] includes [prebuilt mesh_pipeline binaries] for [Windows], [OS X], and
[Linux].

If you'd like to make modifications to [mesh_pipeline] you can also
build it from source by following the instructions below.

# Prerequisites

Prior to building, please install the prerequisites below.

   * Install the fplbase [prerequisites for Windows], [prerequisites for OSX],
     or [prerequisites for Linux].
   * Then install the [FBX SDK] version 2015.1 or newer, at any location.
   * Set the `FBX_SDK` environment variable to the root directory of
     the FBX SDK.

# Building

   * Open a command line window.
   * Navigate to the [fplbase] project directory.
   * Generate [Makefiles] from the [CMake] project, and ensure the
     `fplbase_build_mesh_pipeline` option is enabled.
   * Build the binary in the `bin` directory by:
     * executing `make`, on Linux (or OSX command line)
     * opening the Visual Studio solution and building, on Windows
     * opening the Xcode project and building, on OSX

For example, on Linux:

~~~{.sh}
    cd fplbase
    cmake -G'Unix Makefiles' -Dfplbase_build_mesh_pipeline=ON .
    make
~~~

<br>


  [mesh_pipeline]: @ref fplbase_guide_mesh_pipeline
  [Maya]: http://www.autodesk.com/products/maya/overview
  [Blender]: https://www.blender.org/
  [prebuilt mesh_pipeline binaries]: @ref fplbase_guide_mesh_pipeline_prebuilts
  [Windows]: http://windows.microsoft.com/
  [OS X]: http://www.apple.com/osx/
  [Linux]: http://en.m.wikipedia.org/wiki/Linux
  [prerequisites for Windows]: @ref fplbase_guide_windows
  [prerequisites for OSX]: @ref fplbase_guide_osx
  [prerequisites for Linux]: @ref fplbase_guide_linux
  [FBX SDK]: http://usa.autodesk.com/adsk/servlet/pc/item?siteID=123112&id=10775847
  [Makefiles]: http://www.gnu.org/software/make/
  [CMake]: http://www.cmake.org/
  [fplbase]: @ref fplbase_overview
