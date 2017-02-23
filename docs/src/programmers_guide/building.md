Build Targets    {#fplbase_guide_build_targets}
=============

[FPLBase][] compiles to a C++ library. It includes [CMake][] and
[Android][] build scripts.

The CMake build script can be used to generate standard
makefiles on [Linux][] and [OS X][], [Xcode][] projects on [OS X][] and [iOS][],
and [Visual Studio][] solutions on [Windows][].

The CMake build script also builds the samples, unless it is included from
another project, in which case it omits them.

For more detail, see:

   * [Building for Linux][]
   * [Building for OS X][]
   * [Building for Windows][]
   * [Building for iOS][]

The Android build scripts will compile for Android on Linux, OSX, and Windows.
See,

   * [Building for Android][]

<br>

  [Android]: http://www.android.com
  [Building for Android]: @ref fplbase_guide_android
  [Building for Linux]: @ref fplbase_guide_linux
  [Building for OS X]: @ref fplbase_guide_osx
  [Building for Windows]: @ref fplbase_guide_windows
  [Building for iOS]: @ref fplbase_guide_ios
  [CMake]: http://www.cmake.org/
  [Linux]: http://en.m.wikipedia.org/wiki/Linux
  [FPLBase]: @ref fplbase_overview
  [OS X]: http://www.apple.com/osx/
  [iOS]: http://www.apple.com/ios/
  [Visual Studio]: http://www.visualstudio.com/
  [Windows]: http://windows.microsoft.com/
  [Xcode]: http://developer.apple.com/xcode/
