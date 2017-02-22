FPLBase    {#fplbase_index}
======

# Overview    {#fplbase_overview}

[FPLBase][] is the lowest level game library we use at [FPL][] taking care of
input, rendering, and resource loading (shaders, textures, meshes etc.).
It also offers useful functionality for dealing with Android input devices and
HMDs, and threaded resource loading.

[FPLBase][] is not meant to be an "engine", in that it doesn't dictate anything
about how your game is structured. It is meant to shorten the distance between
an empty project and "drawing stuff on screen", by providing the typical
minimal functionality you don't get if you work on top of raw OpenGL.

It is also not meant to be a platform abstraction library (like SDL, GLFW,
Glut etc). We (optionally) use [SDL][] underneath for that purpose, and we
provide a version of the library (`fplbase_stdlib`) that assumes you set up
your own Open GL context.

Other FPL libraries can be used on top of [FPLBase][], for example [FlatUI][]
can provide font rendering and game UIs.

[FPLBase] is available as open source from
[GitHub](http://github.com/google/fplbase) under the Apache license, v2
(see LICENSE.txt).

# Why use FPLBase?

   * You want to write a cross-platform C++ game or other graphical thing,
     and you'd prefer something simple to start with instead of buying in to
     an "engine".
   * You'd prefer to not have to deal with writing yet another shader / texture
     / mesh loader etc.
   * You want to be using some of our other libraries with it.

# Getting started.

Read [Building][] on how to build for your system, and [Introduction][] on how
to use the API.

# Supported Platforms and Dependencies.

[FPLBase][] has been tested on the following platforms:

   * [Android][]
   * [Linux][] (x86_64)
   * [OS X][]
   * [iOS][]
   * [Windows][]

This library is entirely written in portable C++11 and depends on the
following libraries (included in the download / submodules):

   * [OpenGL][]. Our code is written such that it works well with both ES 2 on
     mobile, and desktop OpenGL.
   * [SDL][] as a cross-platform layer (optional, use `fplbase_stdlib` if you
     initialize your own OpenGL context).
   * [MathFu][] for vector / matrix math.
   * [FlatBuffers][] for serialization.
   * [WebP][] for image loading.
   * [FPLUtil][] (optional, can be used to build).
   * [googletest][] (only in unit tests).

# Download

Download using git or from the
[releases page](http://github.com/google/fplbase/releases).

**Important**: FPLBase uses submodules to reference other components it depends
upon so download the source using:

~~~{.sh}
    git clone --recursive https://github.com/google/fplbase.git
~~~

# Feedback and Reporting Bugs

   * Discuss FPLBase with other developers and users on the
     [FPLBase Google Group][].
   * File issues on the [FPLBase Issues Tracker][].
   * Post your questions to [stackoverflow.com][] with a mention of **fplbase**.

  [Android]: http://www.android.com
  [Build Configurations]: @ref fplbase_build_config
  [Linux]: http://en.m.wikipedia.org/wiki/Linux
  [FPLBase Google Group]: http://groups.google.com/group/fplbaselib
  [FPLBase Issues Tracker]: http://github.com/google/fplbase/issues
  [FPLBase]: @ref fplbase_overview
  [Introduction]: @ref fplbase_introduction
  [Building]: @ref fplbase_guide_build_targets
  [OS X]: http://www.apple.com/osx/
  [iOS]: http://www.apple.com/ios/
  [Windows]: http://windows.microsoft.com/
  [stackoverflow.com]: http://stackoverflow.com/search?q=fplbase
  [FreeType]: http://www.freetype.org/
  [MathFu]: https://google.github.io/mathfu/
  [FlatBuffers]: https://google.github.io/flatbuffers/
  [FPLUtil]: https://google.github.io/fplutil/
  [googletest]: https://code.google.com/p/googletest/
  [FlatUI]: https://google.github.io/flatui/
  [FPL]: https://developers.google.com/games/#Tools
  [SDL]: https://www.libsdl.org/
  [OpenGL]: https://www.opengl.org/
  [WebP]: https://developers.google.com/speed/webp/?hl=en
  [googletest]: https://code.google.com/p/googletest/
