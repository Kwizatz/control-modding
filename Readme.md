# Control Modding

This is a repo with code/scripts/tools and documentation on modding Remedy's Control Video Game.
This is **not** in any way, shape, or form an official project and all data found here was compiled from other unofficial sources or by trial and error.

# Current Status

Right now there is only source for an executable 'multitool' with a single tool, binfbx file dumping.

To build the executable use CMake, and from command line run the tool as:

- control-tool binfbx &lt;binfbx file&gt;

The binfbx files can be extracted with [control-unpack](https://github.com/profMagija/control-unpack) from the ep100-000-generic package file, character models are found at objects\characters\intermediate.

# Remedy's binfbx file format specification.

This section tries to describe the binary structure of Remedy's Control video game 3d mesh format with extension .binfbx.

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
| uint32_t[2]| Reserved            | Always both zero accross all game files                                        |
| float      | GlobalScale         | Not sure what this does                                                        |
| uint32_t[] | LOD Thresholds      | Not sure what this does (related to CDFs?)                                     |
| float      | MirrorSign          | Not sure what this does, supposed to be related to matrix mirroring            |
| float[3]   | Bounding Sphere Ctr | Fixed size array of 3 floats representing Global Bounding Sphere Center        |
| float      | B. Sphere radius    | 1 float representing Global Bounding Sphere Radius                             |
| float[3]   | AABB Min            | Fixed size array of 3 floats representing Global AABB Min                      |
| float[3]   | AABB Max            | Fixed size array of 3 floats representing Global AABB Max                      |
| float[11]  | Unknown             | Fixed size array of 11 floats                                                  |
| uint32_t   | LoDCount            | Number of Level of Detail meshes                                               |
| See below  | Material Data       | A combination of uniform metadata and actual values to set in uniform bindings |
| See below  | Mesh Data           | Data on where each mesh is located in the buffers                              |

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

### Mesh Data

| Type       |  Name               |  Comment                                                                                                                                                                                           |
|------------|:-------------------:|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| uint32_t   | LOD                 | Level of detail for the mesh                                                                                                                                                                       |
| uint32_t   | VertexCount         | Number of vertices in the mesh                                                                                                                                                                     |
| uint32_t   | TriangleCount       | Number of triangles in the mesh                                                                                                                                                                    |
| uint32_t[2]| VertexBufferOffset  | Offset to the vertex buffer data for both Vertex and Attribute Buffers                                                                                                                             |
| uint32_t   | IndexBufferOffset   | Offset to the index buffer data                                                                                                                                                                    |
| int32_t    | mMeshFlags0         | Not sure                                                                                                                                                                                           |
| float[4]   | mBoundingSphere     | Bounding sphere of the mesh                                                                                                                                                                        |
| float[6]   | mBoundingBox        | Bounding box of the mesh                                                                                                                                                                           |
| int32_t    | mMeshFlags1         | Not sure                                                                                                                                                                                           |
| See Below  | Attribute Infos     | Vertex Attribute metadata                                                                                                                                                                          |
| uint32_t   | MaterialBufferOffset| Offset to the material buffer data                                                                                                                                                                 |
| uint32_t   | SubMeshBufferOffset | Offset to the sub-mesh buffer data                                                                                                                                                                 |
| int32_t    | Joint               | Unsure, its definitely related to the skeleton, but its usually a very high if not the last of the joint indices. Could suggest a "Joint Palette", but I dont know that that even means            |
| int32_t    | Unknown             | Who knows?                                                                                                                                                                                         |
| uint8_t    | IsRigidMesh         | If no skeleton joint deforms this mesh, this is 1 otherwhise is 0                                                                                                                                  |
| float      | Unknown             | Who knows?                                                                                                                                                                                         |

### Attribute Infos

This is an array of the following structure, one for each vertex attribute:

| Type       |  Name               |  Comment                                                                                                           |
|------------|:-------------------:|--------------------------------------------------------------------------------------------------------------------|
| uint8_t    | Index               | 0x0 = AttributeBuffer, 0x1 = VertexBuffer                                                                         |
| uint8_t    | Type                | 0x4 = B8G8R8A8_UNORM, 0x7 = R16G16_SINT, 0x8 = R16G16B16A16_SINT, 0xd = R16G16B16A16_UINT, 0x5 = R8G8B8A8_UINT |
| uint8_t    | Usage               | 0x1 = Normal, 0x2 = TexCoord, 0x3 = Tangent, 0x5 = Index, 0x6 = Weight                                           |
| uint8_t    | Zero                | Always 0?                                                                                                          |

## Remedy's binskeleton file format specification

This section describes the binary structure of Remedy's Control video game skeleton format with extension .binskeleton.

This research is entirelly original based on reverse engineering with the help of IDA Free 9.0 as well as Ghidra.

### Skeleton File Structure

Field names are not in any way official and were selected to best represent the information they contain.

### Skeleton General arrangement

| Type       |  Name                    |  Comment                                                                                                                                                                           |
|------------|:------------------------:|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| uint128_t  | Magick                   | Always 0x2. The format uses 128 bit alignment and there is no code check for the Magick number, so it is really unknown whether this is an 8,16,64 or 128 bit number plus padding. |
| uint32_t   | Offset to section start  | Number of bytes after the first 16 bytes of the Magick to the start of the first data section (in practice number of bytes from the address of this field)                         |
| uint32_t   | Size of section in bytes | Size in bytes of the first section of data, this is padded to 128 bits for the next section start.                                                                                 |
| uint32_t   | Subsection count         | Usually 3 (bone transform array, bone parent array, bone id array).                                                                                                                |
| uint32_t[] | Subsection Offset Array  | Subsection count entries representing offsets from the start of the section to the location of another set of offsets to the subsections but 64 bit this time.                     |
| Padding    | 128 bit 0 padding        | Zero padding to align data to 128 bits.                                                                                                                                            |
| uint64_t   | Bone Count               | This is where the first offset points to and contains the total number of bones to be used to extract the data from the subsections.                                               |
| uint64_t[] | Subsection Offset Array  | 64 bit offset array from the start of the section to the start of each subsection, the previous 32 bit offsets point to these which are converted into 64 bit pointers at runtime. |
| float[8]   | Bone Transform Array     | Bone count array of 8 floats representing bone transforms. First subsection offset points here.                                                                                    |
| uint32_t   | Bone Parent Array        | Bone count array of parent indices. Second subsection offset points here.                                                                                                          |
| Padding    | 128 bit 0 padding        | Zero padding to align data to 128 bits.                                                                                                                                            |
| uint32_t   | Bone Id Array            | Bone count array of bone ids. Each id is calculated as the 32 bit FNV1a hash value of its name. Third subsection offset points here.                                               |
| Padding    | 128 bit 0 padding        | Zero padding to align data to 128 bits.                                                                                                                                            |
| uint32_t   | Offset to section start  | This second main section contains bone names (in practice number of bytes from the address of this field).                                                                         |
| uint32_t   | Size of section in bytes | Size in bytes of the first section of data, this is padded to 128 bits for the next section start.                                                                                 |
| uint32_t   | Subsection count         | 1 + bone count. The first subsection offset points to a struct consisting of an offset to an array of offsets to strings and the string count.                                     |
| uint32_t[] | Subsection Offset Array  | Subsection count entries representing offsets for the start of the 64bit string offsets and string offset count plus as many 32 bit offsets to 64 bit offsets to each string.      |
| Padding    | 128 bit 0 padding        | Zero padding to align data to 128 bits.                                                                                                                                            |
| uint64_t[2]| Offset start + count     | 2 64 bit values representing the offset to the final array of offsets and the offset count. The last Offset to section start points here.                                          |
| uint64_t[] | 64 bit offset Array      | 64 bit offsets to each of the string names of each bone, also counting from the last Offset to section start                                                                       |
| char[]     | Bone names as strings    | Zero terminated strings representing the names of the bones. No lenght is stored, the previous offsets just point to the start of the string and its end is marked by a zero.      |

## Related Resources

[Control Unpack](https://github.com/profMagija/control-unpack)
[Loose Files Loader](https://www.nexusmods.com/control/mods/11)
