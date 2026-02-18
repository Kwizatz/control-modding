# Control Modding

This is a repo with code/scripts/tools and documentation on modding Remedy's Control Video Game.
This is **not** in any way, shape, or form an official project and all data found here was compiled from other unofficial sources or by trial and error.

# Current Status

The project provides:

- A **C++ multitool** (`control-tool`) for dumping binfbx file data.
- A **Blender 4.4 addon** (`io_binfbx`) for importing `.binfbx` meshes, `.binskeleton` skeletons, and `.rbf` corrective bone data.
- Documentation on reverse-engineered binary formats used by Remedy's Northlight engine.

## Building the C++ tool

Use CMake, and from command line run:

- control-tool binfbx &lt;binfbx file&gt;

## Blender Addon

Install from `blender/addons/io_binfbx`. Supports importing:

| Format | Extension | Description |
|--------|-----------|-------------|
| BinFBX | `.binfbx` | 3D mesh with embedded skeleton, materials, and LODs |
| BinSkeleton | `.binskeleton` | Master skeleton with full bone hierarchy |
| RBF | `.rbf` | Radial Basis Function corrective bone solver data |

The binfbx/binskeleton files can be extracted with [control-unpack](https://github.com/profMagija/control-unpack) from the ep100-000-generic package file. Character models are found at `objects/characters/intermediate`. RBF files are found at `rbfs/`.

## FNV-1a Bone Hashing

The Northlight engine identifies bones by 32-bit FNV-1a hashes with case folding. This is used consistently across `.binskeleton` bone IDs, `.rbf` bone tables, and animation binding.

```
hash = 0x811c9dc5
for each character c in name:
    hash ^= (c | 0x20)        # case-fold to lowercase
    hash = (hash * 0x01000193) & 0xFFFFFFFF
```

Bone names are case-insensitive for matching purposes. The embedded skeleton in `.binfbx` files uses this hash to bind mesh bones to master skeleton bones at runtime. The binding is **all-or-nothing**: if any bone referenced by the mesh is missing from the master skeleton, the entire bone remapping for that `MeshComponent` is discarded and the mesh renders in bind pose.

# Remedy's binfbx file format specification

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
| uint8_t    | Type                | See Vertex Format table below                                                                                      |
| uint8_t    | Usage               | 0x0 = Position, 0x1 = Normal, 0x2 = TexCoord, 0x3 = Tangent, 0x4 = VertexColor, 0x5 = BoneIndex, 0x6 = BoneWeight |
| uint8_t    | Zero                | Always 0?                                                                                                          |

### Vertex Formats

The vertex formats used in the attribute and vertex buffers. While some values resemble DXGI format enumerations and others match NVIDIA APEX SDK format values (`NxApexRenderDataFormat.h`), they are best treated as engine-specific identifiers. There is no consistent correlation with any external standard.

| Value | Name        | Layout                | Size (bytes) | Typical Usage |
|-------|-------------|----------------------|--------------|---------------|
| 0x02  | FLOAT3      | 3 × float32          | 12           | Position      |
| 0x04  | BYTE4_SNORM | 4 × int8 normalized  | 4            | Tangent       |
| 0x05  | BYTE4_UNORM | 4 × uint8 normalized | 4            | Bone Weight   |
| 0x07  | SHORT2_SNORM| 2 × int16 normalized | 4            | TexCoord      |
| 0x08  | SHORT4_SNORM| 4 × int16 normalized | 8            | Normal        |
| 0x0D  | SHORT4_UINT | 4 × uint16           | 8            | Bone Index    |
| 0x0F  | BYTE4_UINT  | 4 × uint8            | 4            | Bone Index    |

### Vertex Color (Semantic 0x4)

Vertex colour uses BYTE4_SNORM (4 signed bytes). The exact meaning of the channels is unknown but they may encode ambient occlusion, subsurface scattering intensity, or other per-vertex metadata used by the material shader. They do not appear to be standard RGB colour values.

### Shadow / LOD Meshes

Most binfbx files contain multiple mesh entries at different LOD levels. Some meshes use a reduced attribute set (e.g. Position + TexCoord only, no normals or tangent) — these appear to be shadow-map or depth-only meshes. Shadow meshes typically share the same vertex buffer as the main mesh but reference different index ranges.

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

## Remedy's rbf file format specification

This section describes the binary structure of Remedy's Control corrective bone format with extension `.rbf`. RBF stands for **Radial Basis Function** — these files define pose-space corrective bone systems driven by input bone rotations. They are used by `coregame::RBFComponent` / `RBFComponentState` at runtime.

### Purpose

When a character's skeleton is posed (e.g. bending an elbow), certain areas of the mesh can lose volume or deform unnaturally. The RBF system drives **corrective bones** (muscle bulges, tendon lines, joint folds) based on the rotation of **input bones** (wrist, elbow, knee, clavicle, etc.). The solver interpolates between sampled poses using radial basis functions to compute corrective bone transforms.

### Overview

- **Magic number**: `0xD34DB33F` (little-endian)
- **Version**: 2
- **Structure**: Tagged binary tree with 5 node type hashes
- **All leaf nodes** end with a 4-byte sentinel equal to the magic value (`0xD34DB33F`)
- **Bone identification**: FNV-1a hashes (same algorithm as `.binskeleton` bone IDs)

### File Layout

```
FILE
├── ROOT header (24 bytes)
│   ├── magic: uint32 = 0xD34DB33F
│   ├── version: uint32 = 2
│   ├── size: uint32 (= file size)
│   ├── type: uint32 = 0x97C27164
│   ├── num_sections: uint32
│   └── total_count: uint32
│
├── SECTION × num_sections
│   ├── Section header (32 bytes)
│   │   ├── magic: uint32 = 0xD34DB33F
│   │   ├── version: uint32 = 2
│   │   ├── size: uint32 (section byte size)
│   │   ├── type: uint32 = 0x62C7ECBD
│   │   ├── unk: uint32 = 0
│   │   ├── data_sz: uint32
│   │   ├── start_idx: uint32
│   │   └── count: uint32
│   │
│   ├── LEAF_A × count (36 bytes each, type 0xED01652B)
│   │   └── Solver entry: input bone reference + quaternion component
│   │
│   ├── b_count: uint32 (= count)
│   ├── LEAF_B × b_count (32 bytes each, type 0x33D0511D)
│   │   └── Output map: number of output bones affected + param
│   │
│   ├── c_count: uint32 (= count)
│   ├── LEAF_C × c_count (60 bytes each, type 0x341D0EEA)
│   │   └── Pose sample: input quaternion + output translation
│   │
│   └── Data blob
│       ├── solver_dim: uint32
│       ├── output_bone_indices: uint32[total_outputs]
│       └── solver_data: bytes (pre-computed RBF kernel data)
│
├── Input bone table
│   ├── n_input: uint32
│   └── input_hashes: uint32[n_input] (FNV-1a bone name hashes)
│
├── Output bone table
│   ├── n_output: uint32
│   └── output_hashes: uint32[n_output] (FNV-1a bone name hashes)
│
└── trailing_magic: uint32 = 0xD34DB33F
```

### Node Types

| Type Hash    | Name     | Size (bytes) | Description |
|-------------|----------|-------|-------------|
| `0x97C27164` | ROOT     | 24    | File root container |
| `0x62C7ECBD` | SECTION  | 32    | Solver section (one per bone group) |
| `0xED01652B` | LEAF_A   | 36    | Solver entry: input bone reference and quaternion component |
| `0x33D0511D` | LEAF_B   | 32    | Output mapping: how many corrective bones this entry affects |
| `0x341D0EEA` | LEAF_C   | 60    | Pose sample: input rotation as unit quaternion + output translation |

### LEAF_A Detail (Solver Entry)

| Offset | Type    | Field       | Description |
|--------|---------|-------------|-------------|
| 0      | uint32  | magic       | `0xD34DB33F` |
| 4      | uint32  | version     | 2 |
| 8      | uint32  | size        | 36 |
| 12     | uint32  | type        | `0xED01652B` |
| 16     | uint32  | always_1    | Always 1 |
| 20     | uint32  | num_inputs  | Number of input dimensions (1–4) |
| 24     | uint32  | bone_ref    | Index into the input bone hash table |
| 28     | uint32  | component   | Quaternion component: 0=x, 1=y, 2=z, 3=w |
| 32     | uint32  | sentinel    | `0xD34DB33F` |

### LEAF_B Detail (Output Map)

| Offset | Type    | Field        | Description |
|--------|---------|--------------|-------------|
| 0      | uint32  | magic        | `0xD34DB33F` |
| 4      | uint32  | version      | 2 |
| 8      | uint32  | size         | 32 |
| 12     | uint32  | type         | `0x33D0511D` |
| 16     | uint32  | always_1     | Always 1 |
| 20     | uint32  | output_count | Number of output bones this entry affects |
| 24     | uint32  | param        | Solver parameter |
| 28     | uint32  | sentinel     | `0xD34DB33F` |

### LEAF_C Detail (Pose Sample)

| Offset | Type    | Field    | Description |
|--------|---------|----------|-------------|
| 0      | uint32  | magic    | `0xD34DB33F` |
| 4      | uint32  | version  | 2 |
| 8      | uint32  | size     | 60 |
| 12     | uint32  | type     | `0x341D0EEA` |
| 16     | float   | pad0     | Padding (0.0) |
| 20     | float   | pad1     | Padding (0.0) |
| 24     | float   | qx       | Quaternion X component |
| 28     | float   | qy       | Quaternion Y component |
| 32     | float   | qz       | Quaternion Z component |
| 36     | float   | qw       | Quaternion W component |
| 40     | float   | tx       | Translation X |
| 44     | float   | ty       | Translation Y |
| 48     | float   | tz       | Translation Z |
| 52     | float   | pad2     | Padding |
| 56     | float   | sentinel | `0xD34DB33F` reinterpreted as float |

The quaternion `(qx, qy, qz, qw)` is always a unit quaternion (magnitude = 1.0).

### Data Blob

After the LEAF_C entries, each section contains a data blob:

| Offset | Type | Description |
|--------|------|-------------|
| 0 | uint32 | `solver_dim` — dimension of the solver (varies per section) |
| 4 | uint32[total_outputs] | Output bone indices — each is an index into the global output bone hash table |
| 4 + total_outputs×4 | bytes | Pre-computed solver data (RBF kernel matrix / weights) |

Where `total_outputs = sum(output_count)` across all LEAF_B entries in the section.

The relationship `data_sz = total_outputs × 4` holds for the section header's `data_sz` field.

### Bone Tables

After all sections, the file contains two bone hash tables:

- **Input bones** (drivers): The primary skeleton bones whose rotations drive the RBF solver. Typical inputs are wrist, forearm, upper arm, clavicle, upper leg, lower leg, and finger bones.
- **Output bones** (correctives): The corrective bones whose transforms are computed by the solver. Typical outputs are roll joints, tendon joints, muscle volume bones, and joint fold correctives.

All hashes use the same FNV-1a algorithm described above. For Jesse's jumpsuit, there are 56 input bones and 285 output bones. All hashes resolve to named bones in the corresponding `.binskeleton` file.

### Key Relationships

- Each section has a 1:1:1 mapping of LEAF_A : LEAF_B : LEAF_C entries
- `bone_ref` is consistent per bone: each value always maps to exactly one `component`
- The mapping `bone_ref % 4 → component` follows: `{0→0(x), 1→3(w), 2→2(z), 3→1(y)}`
- Jesse's outfits typically have 3 sections per file, 54–60 input bones, and 222–392 output bones

### Cross-Outfit RBF Summary

| Outfit | Sections | Total Entries | Input Bones | Output Bones |
|--------|----------|--------------|-------------|-------------|
| Assistant | 3 | 290 | 60 | 290 |
| Civilian | 3 | 392 | 60 | 392 |
| Expedition | 3 | 222 | 56 | 222 |
| Hedron | 3 | 274 | 55 | 274 |
| Janitor | 3 | 295 | 54 | 295 |
| Jumpsuit | 3 | 285 | 56 | 285 |
| Master | 3 | 290 | 57 | 290 |
| Suit Gold | 3 | 324 | 56 | 324 |
| Suit | 3 | 324 | 58 | 324 |
| Superhero | 2 | 222 | 24 | 222 |
| Tactical | 3 | 311 | 56 | 311 |
| Urban | 3 | 312 | 57 | 312 |

## Remedy's binapx file format (Work in Progress)

The `.binapx` files are **NvCloth / APEX cloth simulation** assets used for physically simulated clothing elements (jackets, hair, straps, pouches, etc.). They are found at `data/cloths/characters/` and are referenced by cloth-simulated submeshes in `.binfbx` files.

Each Jesse outfit has zero or more associated `.binapx` files:

| Outfit | Cloth Files | Elements |
|--------|------------|----------|
| Master | 1 | accessories |
| Civilian | 4 | jacket, fringe, ponytail, zipper pulls |
| Tactical | 1 | jacket |
| Janitor | 2 | belt, rag |
| Assistant | 2 | ponytail, fringe |
| Expedition | 4 | strands, pouch/radio, gasmask, ponytail |
| Hedron | 2 | straps, cables |
| Suit | 1 | general |
| Superhero | 1 | general |
| Jumpsuit | 0 | — |
| Suit Gold | 0 | — |
| Urban | 0 | — |

**The binapx format has not yet been reverse-engineered.** This is planned future work. These files likely contain cloth simulation parameters (stiffness, damping, collision shapes), rest-pose mesh data, and constraint definitions compatible with NVIDIA's NvCloth solver integrated into the Northlight engine.

## Related Resources

[Control Unpack](https://github.com/profMagija/control-unpack)
[Loose Files Loader](https://www.nexusmods.com/control/mods/11)
