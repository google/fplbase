Building for Windows    {#fplbase_guide_windows}
====================

# Version Requirements

Following are the minimum required versions of tools and libraries you
need to build [FPLBase][] for Windows:

   * [Windows][]: 7
   * [Visual Studio][]: 2012
   * [CMake][]: 2.8.12.1

# Prerequisites

Prior to building, install the following:

   * [Visual Studio][]
   * [CMake][] from [cmake.org](http://cmake.org).

# Building with Visual Studio

Generate the [Visual Studio][] project using [CMake][]:

   * Open a command line window.
   * Go to the [FPLBase][] project directory.
   * Use [CMake][] to generate the [Visual Studio][] project.

~~~{.sh}
    cd fplbase
    cmake -G "Visual Studio 11 2012" .
~~~

Open the [FPLBase][] solution in [Visual Studio][].

   * Double-click on `fplbase/FPLBase.sln` to open the solution.
   * Select "Build-->Build Solution" from the menu.

<br>

  [CMake]: http://www.cmake.org
  [FPLBase]: @ref fplbase_overview
  [Visual Studio]: http://www.visualstudio.com/
  [Windows]: http://windows.microsoft.com/

