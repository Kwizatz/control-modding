# Control Modding

This is a repo with code/scripts/tools and documentation on modding Remedy's Control Video Game.
This is **not** in any way, shape, or form an official project and all data found here was compiled from other unofficial sources or by trial and error.

# Current Status

Right now there is only source for an executable 'multitool' with a single tool, binfbx file dumping.

To build the executable use CMake, and from command line run the tool as:

- control-tool binfbx &lt;binfbx file&gt;

The binfbx files can be extracted with [control-unpack](https://github.com/profMagija/control-unpack) from the ep100-000-generic package file, character models are found at objects\characters\intermediate.

# Remedy's binfbx file format specification.

This document tries to describe the binary structure of Remedy's Control video game 3d mesh format with extension .binfbx.

This research is based on the previous on a Blender imported work by user 'volfin' at xentax forums:
https://forum.xentax.com/viewtopic.php?f=33&t=21047&start=45#p156665
Most credit goes to them.

While the extension of the format suggests a relation with the known FBX file format the similarity seems start there and end at how strings are stored within the file.

## Strings

The string format used is the same as FBX, a uint32_t character count followed with that many characters. The terminal character '\0' may or may not be included, if it *is* included it is always counted.

Examples:
  6 joint1
  6 joint\0
  7 joint2\0

Always trust the count to find the next field in the file.

This format is the same for all variable length arrays, a uint32_t followed with that many objects of whatever type.

## File Structure

Field names are not in any way official and were selected to best represent the information they contain.

## General arrangement

| Type       |  Name               |  Comment                                                                       |
|------------|:-------------------:|--------------------------------------------------------------------------------|
| uint32_t   | Magick              | Always 0x2e                                                                    |
| uint32_t   | AttributeBufferSize | Vertex Attribute Buffer size in number of bytes                                |
| uint32_t   | VertexBufferSize    | Vertex Buffer size in bytes in number of bytes                                 |
| uint32_t   | IndexCount          | Number of indices stored into the index buffer                                 |
| uint32_t   | IndexSize           | Number of bytes per index. 2 seems to be the norm                              |
| uint8_t[]  | AttributeBuffer     | Byte array of size AttributeBufferSize                                         |
| uint8_t[]  | VertexBuffer        | Byte array of size VertexBufferSize. Can be read as an array of float vec3     |
| uint8_t[]  | IndexBuffer         | Byte array of size IndexCount * IndexSize                                      |
| See below  | Binding Skeleton    | While Optional if not available you'll find a uint32_t of 0x00000000 here      |
| float[3]   | UpVector            | Usually  x = 0, y = 0, z = 1                                                   |
| float[]    | Unknown             | Variable array of floats, seen 0 and 3, need to inspect more files             |
| float[11]  | Unknown             | Fixed size array of 11 floats                                                  |
| uint32_t   | LoDCount            | Number of Level of Detail meshes                                               |
| See below  | Uniform Data        | A combination of uniform metadata and actual values to set in uniform bindings |
|            |                     | To Be Continued                                                                |

## Binding Skeleton

The skeleton is optional and consists of an array of *joints* (uint32_t count followed by that many joints), if the skeleton is not available the count will just be zero.
The original importer mentions that this can be instead a list of "breakable parts", but that does not make much sence given that no diferentiation is given, most likely "breakable parts" are just a collection of unparented joints.

### Array of Joints

| Type       |  Name               |  Comment                                                                   |
|------------|:-------------------:|----------------------------------------------------------------------------|
| uint32_t   | JointCount          | Number of stored Joints, including Zero                                    |
| joint[]    | Joints              | Array of JointCount Joints                                                 |

### Joint structure

| Type       |  Name               |  Comment                                                                   |
|------------|:-------------------:|----------------------------------------------------------------------------|
| float[16]  | JointMatrix         | Binding matrix as a 4x4 float array                                        |
| uint32_t   | ParentIndex         | Parent Index of this joint, 0xffffffff if joint has no parent              |

## Uniform Data

Uniform Data is provided as an array of "UniformStruct"

| Type       |  Name               |  Comment                                                                   |
|------------|:-------------------:|----------------------------------------------------------------------------|
| uint32_t   | UniformStructCount  | Number of Uniform Structures, layouts, sets, whatever the shaders use      |
| See Below  | UniformStruct[]     | Uniform data and metadata                                                  |

### Uniform Struct

| Type       |  Name               |  Comment                                                                                                                                                           |
|------------|:-------------------:|--------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| uint32_t[3]| Unknown             | Fixed size array of 3 uint32_t's first value is 7 on files seen, the other 2 may not be uint32_s but they don't make sense as floats                               |
| string     | Name                | Name of 'Material', for example "breakable_glass_glass" or "jesse_civ_jeans" may be just a local identifier                                                        |
| string     | MaterialDefinition  | Name of the Material definition used, for example "standardmaterial" json files with these names can be found at "data\metadata\materials" with a .matdef extension|
| string     | MaterialSource      | Path to the material source, usually starts with "sourcedata\materials", probably metadata or development only as the file is not in generic or pc .rmdp files     |
| See Below  | UniformVariables    | Array of uniform variable data, likely to be compiled into a Uniform Buffer or independently bound to shader uniform variables                                     |

### Uniform Variable Array

For some reason Volfin made the comment on the Blender importer that the count seems to be almost always 16, and thats what I found to be the case, still this is a variable array so:

| Type       |  Name                |  Comment                                                                   |
|------------|:--------------------:|----------------------------------------------------------------------------|
| uint32_t   | UniformVariableCount | Number of Uniform Variables                                                |
| See Below  | UniformVariables     | Variable size array of Uniform Variables                                   |

### Uniform Variable

| Type       |  Name               |  Comment                                                                                                                                                                                           |
|------------|:-------------------:|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| string     | Name                | Variable name, probably what you'd ask OpenGL to bind the value to in GLSL, example: "g_bAlphaTest"                                                                                                |
| uint32_t   | VariableType        | Known types: Float = 0x00, Range = 0x01, Color = 0x03, Vector = 0x02, TextureMap = 0x09, TextureSampler = 0x08, Boolean = 0x0C                                                                     |
| Depends    | VariablesValue      | Size of the value depends on type Float = 1 float, Range = 2 floats, Color = 4 floats, Vector = 3 floats, TextureMap = string path to image file, TextureSampler = *NO DATA*, Boolean = 1 uint32_t |

Note: TextureSampler contains no data, likely because you need to generate a texture name or id on DirectX, OpenGL or Vulkan that you will not know until runtime, not sure why have a texture map and a texture sampler yet.

## To Be Continued

Here is where things really confusing and hackish in the importer, it's not that the info is not there, but rather that the way it is layed out is confusing and variable names don't make entire sense. I will try to reorder it better and see if that way I can get some clues on what is what.
