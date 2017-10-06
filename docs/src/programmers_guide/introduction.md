Introduction    {#fplbase_guide_introduction}
============

# Introduction {#fplbase_introduction}

This document outlines the components that make up FPLBase.

# SDL {#fplbase_sdl}

We (optionally) use [SDL][] (Simple Directmedia Layer) as our lowest level
layer. SDL is an Open Source cross platform layer providing OpenGL context
creation, input, and other things a game needs to run on a platform,
without having to write any platform specific code. It has been in development
for a long time, and has very robust support for mobile (Android, iOS),
desktop (Windows, OS X, Linux), and even is available for the web through
asm.js.

SDL together with OpenGL(ES) and C++ provide an excellent basis for making great
cross platform games.

If you prefer to initialize your own OpenGL context, you can link to the
`fplbase_stdlib` library instead of the `fplbase` library (see CMakeLists.txt).
All the functionality of fplbase is still available: loading meshes, shaders,
and textures, and hiding the OpenGL details from your code.

# Components {#fplbase_components}

Directly on top of SDL sit two systems, the renderer (`renderer.h`)
and the input system (`input.h`).
On top of the renderer sits two more (optional) systems, the asset manager
(`asset_manager.h`) and the asynchronous loader (`async_loader.h`).

To represent resources created by the renderer (or asset manager) we have:
* `Shader` (`shader.h`)
* `Mesh` (`mesh.h`)
* `Texture` / `Material` (`material.h`)

The renderer also depends upon our [MathFu] library for all its vector and
matrix datatypes. The asset manager depends on our [FlatBuffers]
serialization library.

# Basic initialization and rendering {#fplbase_renderer}

The Renderer is the core of the engine, and is responsible
for creating the OpenGL context and OpenGL resources such as
shaders and textures.

The basic flow of using this lower level layer to create a fully functioning
program is as follows:

* Instantiate the `Renderer`, and call `Initialize` on it. This will get your
  OpenGL context set up, and a window/screen ready to draw on:
  ~~~{.cpp}
  Renderer renderer;
  renderer.Initialize(vec2i(960, 640), "Simple FPLBase example");
  ~~~
  The size argument is only really used for the desktop.
  If initialize fails (returns false), you can get a more informative error
  string by calling `renderer.last_error()`.
* Create the input system:
  ~~~{.cpp}
  InputSystem input;
  input.Initialize();
  ~~~
  This doesn't fail.
* Create resources. `CompileAndLinkShader` will turn two shader strings (GLSL
  vertex and pixel shader) into a `Shader` object:
  ~~~{.cpp}
  auto shader = renderer.CompileAndLinkShader(vertex_shader, fragment_shader);
  ~~~
  (for even more convenient methods that will load directly from file, see the
  material manager below).
  Shaders failing to compiler can generate complex errors, so be sure to
  check the contents of `last_error()` when `shader` is null.
  For more resource types, see below.
* Now you're ready to run your main loop. Call `AdvanceFrame` on the renderer
  to swap buffers and do general initialisation of the frame, likely followed
  by `ClearFrameBuffer`.
  ~~~{.cpp}
  while (!input.exit_requested()) {
    renderer.AdvanceFrame(input.minimized(), input.Time());
    input.AdvanceFrame(&renderer.window_size());
    renderer.ClearFrameBuffer(background_color);
    // Render stuff here.
  }
  ~~~
  We use the input system to see if an exit has been requested (close button,
  app shut down). `AdvanceFrame` for the input system advances the time, and
  collects new input events.
* Before rendering anything, call the renderer's `set_model_view_projection()`.
  Use our separate [MathFu] library to combine matrices depending on whether
  you're creating a 2D or 3D scene, e.g. `mathfu::mat4::Ortho` and
  `mathfu::mat4::Perspective`.
  ~~~{.cpp}
  renderer.set_model_view_projection(
    mathfu::mat4::Ortho(-1.0, 1.0, -1.5, 1.5, -1.0, 1.0));
  ~~~
  Creates a 2D coordinate space with (0,0) in the center and (3,2) in size.
* Now use the shader you've created by calling `SetShader` on the renderer.
  This will make it active, and also upload any renderer variables (such as
  `model_view_projection`), ready to be used by the shader.
  ~~~{.cpp}
  renderer.SetShader(shader);
  ~~~
* Finally, you can now render something! Lets use use convenient helper
  function that gets us a quad (or 2 sprite):
  ~~~{.cpp}
  RenderAAQuadAlongX(mathfu::vec3(-0.5f, -0.5f, 0.0f),
                     mathfu::vec3(0.5f, 0.5, 0.0f),
                     mathfu::vec2(0.0f, 0.0f),
                     mathfu::vec2(1.0f, 1.0f));
  ~~~
  For more complex meshes, see below.

Putting this all together, we get:

~~~{.cpp}
Renderer renderer;
renderer.Initialize(vec2i(960, 640), "Simple FPLBase example");

InputSystem input;
input.Initialize();

// A vertex shader that passes untransformed position thru.
auto vertex_shader =
  "attribute vec4 aPosition;\n"
  "void main() { gl_Position = aPosition; }\n";

// A fragment shader that outputs a green pixel.
auto fragment_shader =
  "void main() { gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0); }\n";

auto shader = renderer.CompileAndLinkShader(vertex_shader, fragment_shader);
assert(shader);

while (!input.exit_requested()) {
  renderer.AdvanceFrame(input.minimized(), input.Time());
  input.AdvanceFrame(&renderer.window_size());

  float color = (1.0f - cos(input.Time())) / 2.0f;
  renderer.ClearFrameBuffer(mathfu::vec4(color, 0.0f, color, 1.0f));

  renderer.set_model_view_projection(
    mathfu::mat4::Ortho(-1.0, 1.0, -1.5, 1.5, -1.0, 1.0));

  renderer.SetShader(shader);

  RenderAAQuadAlongX(mathfu::vec3(-0.5f, -0.5f, 0.0f),
                     mathfu::vec3(0.5f, 0.5, 0.0f),
                     mathfu::vec2(0.0f, 0.0f),
                     mathfu::vec2(1.0f, 1.0f));
}

renderer.ShutDown();
~~~

# Asset Manager {#fplbase_matman}

The renderer is deliberately a bare minimum system that takes care of creating
and using resources, but not *managing* them. The asset manager takes care
of loading from disk, and caching resources, but is deliberately seperate from
the renderer, such that it is easy to replace with something else should the
need arise.

Where the renderer reads from memory buffers, the material manager loads files.
To keep this cross platform, any resources should be in a folder called `assets`
under the project root. All paths you specify are relative to this folder.

Create one by passing the Renderer that will do the actual creation of
resources:
~~~{.cpp}
fpl::AssetManager asset_manager(renderer);
~~~

Once you've instantiated the `AssetManager`, calls like `LoadShader`
will conveniently construct a `Shader` from two files. It does this by
suffixing `.glslv` and `.glslf` to the filename you pass.

Similarly, `LoadTexture` will load a TGA or WebP file straight into a `Texture`:

~~~{.cpp}
auto shader = asset_manager.LoadShader("tex");
assert(shader);

auto tex = asset_manager.LoadTexture("tex.webp");
assert(tex);
~~~

All methods that start with `Load` have the property that if you ask to load a
file that has been loaded before, it just instantly return the previously
created resource, and only do any actual loading if not. As before,
`last_error()` will have a descriptive message if this fails.

Alternatively, there are `Find` versions of these methods that will return
`nullptr` if the resource wasn't previously loaded.

More high-level than loading individual textures is loading a `Material`,
which is a set of textures all meant to be used in the same draw call,
bundled with rendering flags such as the desired alpha blending mode etc.
You create an actual material file by writing a small .json file which specifies
the texture filenames and other properties, e.g.:

~~~{.json}
{
  texture_filenames: [ "diffuse.webp", "normal.webp" ],
  blendmode: OFF
}
~~~
(for more, see `schemas/material.fbs`). This JSON file can be converted
to a binary file by our [FlatBuffers][] serialization library, the result
of which can be passed to `LoadMaterial` that will load all referenced
textures and create a `Material` object (which can be attached to `Mesh`).

Meshes load similary using `LoadMesh`, to find out how to create these files,
see [MeshPipeline][].

A simple example, which could replace the shader loading in the above example,
and also loads a texture:

~~~{.cpp}
fpl::AssetManager asset_manager(renderer);

auto shader = asset_manager.LoadShader("tex");
assert(shader);

auto tex = asset_manager.LoadTexture("tex.webp");
assert(tex);

asset_manager.StartLoadingTextures();
while (!asset_manager.TryFinalize()) {
  // Can display loading screen here.
}
~~~

What was that while loop needed for?
See [Asynchronous Loader][fplbase_async_loader].

Note: for fast loading and efficient GPU usage, use of compressed ETC2 format
textures in a .ktx file is recommended. Loading will automatically fall-back
to .webp if the hardware does not support it, i.e systems that don't support
OpenGL 3.0. Alternatively, use ETC1 which is always supported.
Generate these with e.g:
~~~
etcpack mytexture.png . -c etc2 -f RGBA -mipmaps -ktx
~~~


# Asynchronous Loader {#fplbase_async_loader}

Loading resources can take a long time, and can be sped up by loading
heavy resources (such as textures) in parallel with the
rest of the game initialization. Even if speed is not the issue, just being
able to conveniently render a loading animation is nice to have.

`AsyncLoader` takes care of all that, hiding the gory details of threading
from you. It is fully integrated with the material manager, so using it is
relatively simple:

* Load all your resources as normal. You will receive `Texture` and
  `Material` objects, but these won't actually have any texture data
  associated with them yet, as textures haven't loaded yet.
* Once you've queued up all your resources, call `StartLoadingTextures`
  on the material manager to get the loading started.
* Now, enter your frame loop as normal. Call `TryFinalize` which will check
  if all textures have been loaded. If it returns false, you should display
  a loading screen, otherwise render the game as normal.
* Your loading screen might want to use textures also. Since the loader
  loads textures in the order they were requested, make sure you queue up
  your loading screen textures first. If the `Texture::id()` is non-zero,
  it can already be used.


# Instantiating resources with the renderer {#fplbase_renderer_resources}

We already saw how to load shaders directly from memory without using the
asset manager, we can do that with other resources too, if you prefer to
do your own asset management:

`LoadAndUnpackTexture` will take a file (currently WebP/TGA/KTX formats)
and turn it into a raw buffer which `CreateTexture` turns into an OpenGL
texture.

There's several ways to render or create meshes. We already saw
`RenderAAQuadAlongX` which is the simplest way to get geometry on screen.
A more general way is `RenderArray` which will render a mesh on the fly
from any indices and vertices:

~~~{.cpp}
const fpl::Attribute format[] = {
  fpl::Attribute::kPosition3f, fpl::Attribute::kEND
};

const unsigned short indices[] = { 0, 1, 2 };
const float vertices[] = {
  -.5f, -.5f, 0.0f,
  0.0f, 0.5f, 0.0f,
  0.5f, -.5f, 0.0f
};

fpl::RenderArray(fpl::Mesh::Primitive::kTriangles, 3, format,
                 sizeof(float) * 3, vertices, indices);
~~~

`format` specifies what kind of attributes our vertex data contains, here only
positions. Vertex data must always be interleaved.

Rendering on the fly is convenient, but for the best performance you should
always construct Mesh objects, which deal with uploading of vertex and index
data just once. Given the data in the above example, that would look like:
~~~{.cpp}
auto mesh = new Mesh(vertices, 3, sizeof(float) * 3, format);
mesh->AddIndices(indices, 3, material);
~~~
You may call AddIndices more than once to create multiple different surfaces
(with different textures) that make use of the same geometry.

You'll need a material which you can instantiate using the asset manager, or
manually.

Inside your rendering loop, now all you have to do is call:
~~~{.cpp}
renderer.Render(mesh);
~~~
This will render all surfaces contained, set all textures etc.

# Input System {#fplbase_input}

`InputSystem` deals with time, touch/mouse/keyboard/gamepad
input, and lifecycle events.

As we saw above, to initialize it call `Initialize`, and once
per frame call `AdvanceFrame` right after you called the same method on
the renderer. This collects new input events from the system to be reflected
in its internal state.

Call `Time` for seconds since game start, and `DeltaTime` for seconds since
the last frame. `Time` is updated only once per frame, to ensure that all
animations/simulations that happen during a frame stay in sync with rendering.
If instead you want time for profiling, call `RealTime` which recomputes the
current time each time it is called.

Now you can query the state of the inputs you're interested in. For example,
call `GetButton` with one of the constants from `input.h` which can either be
a keyboard key, or a pointer (meaning the mouse or touch inputs, depending
on platform) or a gamepad button, and you can see if it went down or up
this last frame, or wether it is currently down. The `Pointer` objects can
tell you more about their current position. For example:

~~~{.cpp}
if (input.GetButton(K_POINTER1).went_down()) {
  auto &pos = input.get_pointers()[0].mousepos;
  printf("finger/mouse hit at %d, %d\n", pos.x, pos.y);
}
~~~

  [SDL]: https://www.libsdl.org/
  [MathFu]: http://google.github.io/mathfu
  [FlatBuffers]: http://google.github.io/flatbuffers/
  [MeshPipeline]: @ref fplbase_guide_mesh_pipeline
