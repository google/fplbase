Building for iOS    {#fplbase_guide_ios}
================

<!-- Include the JavaScript to template in iOS SDK specific code snippets. -->
\htmlonly
<script src="iossdkversions.js"></script>
\endhtmlonly

# Version Requirements

Following are the minimum tested versions of tools and libraries you
need to build [FPLBase][] for [iOS][]:

   * [OS X][]: Mavericks 10.9.1.
   * [iOS SDK][]: 8.3
   * [Xcode][]: 6.3.2
   * [CMake][]: 3.4

# Prerequisites

   * Install [Xcode][].
   * Install [CMake][].

# Building with Xcode

Firstly, the [Xcode][] project needs to be generated using [CMake][]:

   * Open a command line window.
   * Go to the [FPLBase][] project directory.
   * Use [CMake][] to generate the [Xcode][] project.

~~~{.sh}
    cd fplbase
    cmake -DCMAKE_TOOLCHAIN_FILE=cmake/ios.cmake -GXcode .
~~~

Then the project can be opened in [Xcode][] and built:

   * Double-click on `fplbase/FPLBase.xcodeproj` to open the project in
     [Xcode][].
   * Select "Product-->Build" from the menu.

The samples and tests can be run from within [Xcode][]:

   * Select an application `Scheme`, for example
     "fplbase-texture-->iOS Simulators/iPhone 6s Plus",
     from the combo box to the right of the "Run" button.
   * Click the "Run" button.

<br>

  [CMake]: http://www.cmake.org
  [FPLBase]: @ref fplbase_overview
  [OS X]: http://www.apple.com/osx/
  [iOS]: http://www.apple.com/ios/
  [iOS SDK]: https://developer.apple.com/ios/
  [SDL]: https://www.libsdl.org/
  [Xcode]: http://developer.apple.com/xcode/
