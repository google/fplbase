Mesh Pipeline   {#fplbase_guide_mesh_pipeline}
=============

The `mesh_pipeline` converts [FBX files] into [FlatBuffer] files that can
be loaded with `AssetManager::LoadMesh()`.

Most modeling software, such as [Maya] and [Blender], supports export to FBX,
so the `mesh_pipeline` is an effective way to bring mesh data from your
authoring tools into your runtime.

    Usage: mesh_pipeline [-b ASSET_BASE_DIR] [-r ASSET_REL_DIR]
                         [-e TEXTURE_EXTENSION] [-f TEXTURE_FORMATS]
                         [-m BLEND_MODE] [-a AXES] [-h] [-c] [-v|-d|-i]
                         FBX_FILE

    Pipeline to convert FBX mesh data into FlatBuffer mesh data.
    We output a .fplmesh file and (potentially several) .fplmat files,
    one for each material. The files have the same base name as
    FBX_FILE, with a number appended to the .fplmat files if required.
    The .fplmesh file references the .fplmat files.
    The .fplmat files reference the textures.

    Options:
      -b, --base-dir ASSET_BASE_DIR
      -r, --relative-dir ASSET_REL_DIR
                    The .fplmesh file and the .fplmat files are output
                    to the ASSET_BASE_DIR/ASSET_REL_DIR directory.
                    ASSET_BASE_DIR is the working directory of your app,
                    from which all files are loaded. The .fplmesh file
                    references the .fplmat file relative to
                    ASSET_BASE_DIR, that is, by prefixing ASSET_REL_DIR.
                    If ASSET_BASE_DIR is unspecified, use current
                    directory. If ASSET_REL_DIR is unspecified, output
                    and reference files from ASSET_BASE_DIR.
      -e, --texture-extension TEXTURE_EXTENSION
                    material files use this extension for texture files.
                    Useful if your textures are externally converted
                    to a different file format.
                    If unspecified, uses original file extension.
      -f, --texture-formats TEXTURE_FORMATS
                    comma-separated list of formats for each output
                    texture. For example, if a mesh has two textures
                    then `AUTO,F_888` will ensure the second texture's
                    material has 8-bits of RGB precision.
                    Default is AUTO.
                    Valid possibilities:
                               AUTO
                               F_8888
                               F_888
                               F_5551
                               F_565
      -m, --blend-mode BLEND_MODE
                    rendering blend mode for the generated materials.
                    If texture format has an alpha channel, defaults to
                    ALPHA. Otherwise, defaults to OFF.
                    Valid possibilities:
                               OFF
                               TEST
                               ALPHA
                               ADD
                               ADDALPHA
                               MULTIPLY
      -a, --axes AXES
                    coordinate system of exported file, in format
                        (up-axis)(front-axis)(left-axis)
                    where,
                        'up' = [x|y|z]
                        'front' = [+x|-x|+y|-y|+z|-z], is the axis
                          pointing out of the front of the mesh.
                          For example, the vector pointing out of a
                          character's belly button.
                        'left' = [+x|-x|+y|-y|+z|-z], is the axis
                          pointing out the left of the mesh.
                          For example, the vector from the character's
                          neck to his left shoulder.
                    For example, 'z+y+x' is z-axis up, positive y-axis
                    out of a character's belly button, positive x-axis
                    out of a character's left side.
                    If unspecified, use file's coordinate system.
      -h, --hierarchy
                    output vertices relative to local pivot of each
                    sub-mesh. The transforms from parent pivot to local
                    pivot are also output.
                    This option is necessary for animated meshes.
      -c, --center  ensure world origin is inside geometry bounding box
                    by adding a translation if required.
      -v, --verbose output all informative messages
      -d, --details output important informative messages
      -i, --info    output more than details, less than verbose

# Texture Files

[FBX files] reference [texture files], but the mapping is not well defined.
Most often the texture files are referenced by absolute path, which creates
a problem when the set of files is moved to another directory or checked into a
[version control system].

Also, it's common for texture files to be processed externally. They may
have a new [file extension], their names may have a different [casing],
or they may have been renamed to have the same base file name as the `FBX` file.

The `mesh_pipeline` therefore searches for texture files in the following order
(see `FbxMeshParser::FindSourceTextureFileName()`):
- `FBX directory` + `texture relative path` + `texture name`
- `FBX directory` + `texture name`
- `FBX directory` + `FBX base name` + `texture extension`
- `FBX directory` + texture and FBX base names in various casings +
  various image extensions
- `texture name in FBX file`

where "various casings" include,
- original base name
- base name in [snake_case]
- base name in [CamelCase]

and "various image extensions" include "jpg", "jpeg", "png", "webp", and "tga".

If a texture file is not found, `mesh_pipeline` outputs a warning and lists
all of the file names that it tried in its search.

# Bone Assignment

The `mesh_pipeline` traverses the FBX's scene graph in [depth-first order].
A node is output if it is the ancestor of a mesh node--that is, if it has
vertex data somewhere under it.

When `--hierarchy` is specified, every output node is assigned a bone,
and vertex positions are output relative to that bone's transform.

Note that, although the FlatBuffer format supports skinning information with
four influences, `mesh_pipeline` currently only outputs skinning
information with a single influence.
That is, vertices are attached 100% to a single bone.
This limitation will be removed in a later release of `mesh_pipeline`.

The bone heirarchy output by `mesh_pipeline` matches the bone hierarchy
output by `anim_pipeline`, in [Motive].
The `anim_pipeline` animations can therefore be applied to the `mesh_pipeline`
meshes.

# Pre-built Binaries  {#fplbase_guide_mesh_pipeline_prebuilts}

Pre-built binaries for the `mesh_pipeline` are distributed in the `bin`
directory.
- Windows 7: bin/Windows/Release/mesh_pipeline.exe
- Mac OS X (10.10.5 Yosemite): bin/Darwin/mesh_pipeline
- Linux (Ubuntu 64-bit): bin/Linux/mesh_pipeline

To build the `mesh_pipeline` from source, please see [building mesh_pipeline].


  [FBX files]: https://en.wikipedia.org/wiki/Spline_(mathematics)
  [Maya]: http://www.autodesk.com/products/maya/overview
  [Blender]: https://www.blender.org/
  [FlatBuffer]: http://google.github.io/flatbuffers/
  [texture files]: https://en.wikipedia.org/wiki/Texture_mapping
  [depth-first order]: https://en.wikipedia.org/wiki/Depth-first_search
  [version control system]: https://en.wikipedia.org/wiki/Version_control
  [casing]: https://en.wikipedia.org/wiki/Letter_case
  [file extension]: https://en.wikipedia.org/wiki/Filename_extension
  [snake_case]: https://en.wikipedia.org/wiki/Snake_case
  [CamelCase]: https://en.wikipedia.org/wiki/CamelCase
  [Motive]: http://google.github.io/motive/
  [building mesh_pipeline]: @ref fplbase_guide_building_mesh_pipeline