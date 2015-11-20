Building for iOS    {#fplbase_guide_ios}
================

# Version Requirements

Following are the minimum required versions of tools and libraries you
need to build [FPLBase][] for [OS X]:

   * [OS X][]: Mavericks 10.9.1.
   * [iOS SDK][]: 8.3
   * [Xcode][]: 6.3.2
   * [CMake][]: 3.4

# Prerequisites

   * Install [Xcode][].
   * Install [CMake][].

# Known limitations

- When generated with CMake, Xcode projects are tied to a specific
  platform and architecture.
- SDL2 is not built using CMake but with the upstream provided
  xcodeproj.
- Resources are global to all samples.

# Building with Xcode

1. Build `flatc` for the host architecture and add it to your `PATH`:
~~~{.sh}
    cd $FLATC_SOURCE_DIRECTORY
    cmake -GXcode . && xcodebuild
    mkdir -p ~/bin && cp flatc ~/bin
    export PATH=$PATH:~/bin
~~~

2. Build `libSDL2` for the Simulator architecture using the upstream
   provided `xcodeproj`
~~~{.sh}
    cd $SDL2_SOURCE_DIRECTORY
    xcodebuild ARCHS="x86_64 i386" -project Xcode-iOS/SDL/SDL.xcodeproj -sdk iphonesimulator8.3 build
~~~

3. Generate the [Xcode][] project needs to be generated using [CMake][]:
~~~{.sh}
    cd fplbase
    cmake -Dfplbase_use_external_sdl2=ON -Dexternal_sdl2_libraries=$SDL2_SOURCE_DIRECTORY/Xcode-iOS/SDL/build/Release-iphonesimulator/libSDL2.a -Dexternal_sdl2_include_dir=$SDL2_SOURCE_DIRECTORY/include -Dfplbase_use_host_flatc=ON  -DCMAKE_TOOLCHAIN_FILE=cmake/ios.toolchain.xcode.cmake -GXcode .
~~~

4. Then the project can be opened in [Xcode][] and built:

   * Double-click on `fplbase/fplbase.xcodeproj` to open the project in
     [Xcode][].
   * Select "Product-->Build" from the menu.

5. The demo can be run from within [Xcode][]:

   * Select one of the sample `Scheme`, for example
     "fplbase-triangle-->iPhone Simulator", from the combo box to the right of the
     "Run" button.
   * Click the "Run" button.

<br>

  [CMake]: http://www.cmake.org
  [FPLBase]: @ref fplbase_overview
  [OS X]: http://www.apple.com/osx/
  [iOS]: http://www.apple.com/ios/
  [iOS SDK]: https://developer.apple.com/ios/
  [Xcode]: http://developer.apple.com/xcode/
