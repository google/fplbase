Building for Linux    {#fplbase_guide_linux}
==================

# Version Requirements

Following are the minimum required versions of tools and libraries you
need to build [FPLBase][] for Linux:

   * [autoconf][]: 2.69
   * [automake][]: 1.141
   * [CMake][]: 2.8.12.1
   * [cwebp][]: 0.4.0 (available from the [WebP Precompiled Utilities][] page)
   * [libtool][]: 2.4.2
   * [OpenGL][]: libglapi-mesa 8.0.4
   * [Python][]: 2.7.6
   * [Ragel][]: 6.9

# Prerequisites

Prior to building, install the following components using the [Linux][]
distribution's package manager:

   * [autoconf][], [automake][], and [libtool][]
   * [cmake][] (You can also manually install from [cmake.org][].)
   * [cwebp][] (`webp`)
   * [OpenGL][] (`libglapi-mesa`)
   * [Python][]
   * [Ragel][]

For example, on [Ubuntu][]:

~~~{.sh}
    sudo apt-get install autoconf automake cmake libglapi-mesa libglu1-mesa-dev libtool  python ragel webp
~~~

# Building

   * Open a command line window.
   * Go to the [FPLBase][] project directory.
   * Generate [Makefiles][] from the [CMake][] project. <br/>
   * Execute `make` to build the library and unit tests.

For example:

~~~{.sh}
    cd fplbase
    cmake -G'Unix Makefiles' .
    make
~~~

To perform a debug build:

~~~{.sh}
    cd fplbase
    cmake -G'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug .
    make
~~~

Build targets can be configured using options exposed in
`fplbase/CMakeLists.txt` by using [CMake]'s `-D` option.
Build configuration set using the `-D` option is sticky across subsequent
builds.

For example, if a build is performed using:

~~~{.sh}
    cd fplbase
    cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug .
    make
~~~

to switch to a release build CMAKE_BUILD_TYPE must be explicitly specified:

~~~{.sh}
    cd fplbase
    cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release .
    make
~~~

<br>

  [autoconf]: http://www.gnu.org/software/autoconf/
  [automake]: http://www.gnu.org/software/automake/
  [CMake]: http://www.cmake.org/
  [cmake.org]: http://www.cmake.org/
  [cwebp]: https://developers.google.com/speed/webp/docs/cwebp
  [libtool]: http://www.gnu.org/software/libtool/
  [Linux]: http://en.wikipedia.org/wiki/Linux
  [FPLBase]: @ref fplbase_overview
  [Makefiles]: http://www.gnu.org/software/make/
  [OpenGL]: http://www.mesa3d.org/
  [Python]: http://www.python.org/download/releases/2.7/
  [Ragel]: http://www.colm.net/open-source/ragel/
  [Ubuntu]: http://www.ubuntu.com
  [WebP Precompiled Utilities]: https://developers.google.com/speed/webp/docs/precompiled
