# Copyright (C) 2021-2025 Rodrigo Jose Hernandez Cordoba
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import bpy
import os
import os.path
import struct
import mathutils
#from multiprocessing import Pool
#from multiprocessing.dummy import Pool as ThreadPool, Lock as ThreadLock

FBXMAGICK = 0x2e
SKELETONMAGICK = 0x2

FLOAT = 0x00
RANGE = 0x01
COLOR = 0x03
VECTOR = 0x02
TEXTUREMAP = 0x09
TEXTURESAMPLER = 0x08
BOOLEAN = 0x0C

# Semantics
POSITION = 0x0
NORMAL = 0x1
TEXCOORD = 0x2
TANGENT = 0x3
BONE_INDEX = 0x5
BONE_WEIGHT = 0x6

# Vertex Formats
# Some Vertex Formats seem to match the DXGI formats at
# https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-formats
# But not all of them, some others match NVIDIA APEX framework formats in the SDK at
# APEXSDK-1.3.2-Build6-CL18960576-PhysX_3.3.2-WIN-VC10-BIN\framework\public\NxApexRenderDataFormat.h
# and yet some others are just custom formats or match something else entirely
# Since there is no correlation between the formats and the vertex buffer they are used in,
# and that the APEX formats are Unsigned but then threated as Signed, we should conclude that
# these formats are just custom formats used by the game engine and not DXGI or APEX formats,
# just a coincidence that they match some of them in byte size.

FLOAT3 = 2       # POSITION
BYTE4_SNORM = 4  # TANGENT ?
BYTE4_UNORM =  5 # BONE_WEIGHT
SHORT2_SNORM = 7 # TEXCOORD
SHORT4_SNORM = 8 # NORMAL
SHORT4_UINT = 13 # BONE_INDEX
BYTE4_UINT  = 15 # BONE_INDEX

Format = {
    FLOAT3: '3f',
    BYTE4_SNORM: '4b',
    BYTE4_UNORM: '4B',
    SHORT2_SNORM: '2h',
    SHORT4_SNORM: '4h',
    SHORT4_UINT: '4H',
    BYTE4_UINT:  '4B',
}

FormatIndexCount = {
    FLOAT3: 3,
    BYTE4_SNORM: 4,
    BYTE4_UNORM: 4,
    SHORT2_SNORM: 2,
    SHORT4_SNORM: 4,
    SHORT4_UINT: 4,
    BYTE4_UINT:  4,
}

# This is just for testing and debugging
UniformTypeNames = {
    FLOAT: 'float',
    RANGE: 'range',
    COLOR: 'color',
    VECTOR: 'vector',
    TEXTUREMAP: 'texturemap',
    TEXTURESAMPLER: 'texturesampler',
    BOOLEAN: 'boolean'
}

right_hand_matrix = mathutils.Matrix(
    ((-1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))


def Vector3IsClose(v1, v2):
    return abs(v2[0] - v1[0]) < 0.001 and abs(v2[1] - v1[1]) < 0.001 and abs(v2[2] - v1[2]) < 0.001


class IMPORT_OT_binfbx(bpy.types.Operator):
    '''Imports a binfbx or binskeleton file'''
    bl_idname = "import.binfbx"
    bl_label = "Import"
    filepath: bpy.props.StringProperty(name="BinFBX", subtype='FILE_PATH')
    filter_glob: bpy.props.StringProperty(
        default="*.binfbx;*.binskeleton",
        options={'HIDDEN'},
    )
    # Optional: verbose debug logging of parsed fields (global params and per-mesh bounds/flags)
    debug: bpy.props.BoolProperty(name="Debug logs", default=False)

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        bpy.context.window.cursor_set("WAIT")
        # Force Object Mode
        # bpy.ops.object.mode_set()
        (name, ext) = os.path.splitext(self.filepath)
        if ext == ".binskeleton":
            print("Importing Skeleton")
            # Import Skeleton
            return self.import_binskeleton()
        elif ext == ".binfbx":
            # Import Mesh (Includes implicit skeleton)
            print("Importing Mesh")
            return self.import_binfbx()
        else:
            self.report({'ERROR'}, "Invalid file extension")
            return {'CANCELLED'}

    def import_binskeleton(self):
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binskeleton")        
        file = open(self.filepath, "rb")
        # Read the whole file
        file_data = file.read()
        file.close()
        # Check Magick        
        pointer = 0
        Magick = struct.unpack("I", file_data[pointer:pointer+4])

        if Magick[0] != SKELETONMAGICK:
            self.report({'ERROR'}, "Invalid BinSkeleton file")
            return {'CANCELLED'}

        pointer += 0x10
        bone_data_start, bone_data_size, bone_data_subsection_count = struct.unpack("III", file_data[pointer:pointer+12])
        bone_data_offset = pointer
        bone_names_offset = pointer + (bone_data_start + bone_data_size) + ((bone_data_start + bone_data_size) % 0x10)
        pointer += 0x0C
        bone_count = struct.unpack("Q", file_data[(bone_data_offset + bone_data_start):(bone_data_offset + bone_data_start)+8])[0]        
        transform_offset = 0
        parents_offset   = 0
        for i in range(bone_data_subsection_count):
            offset = struct.unpack("I", file_data[pointer+(i*4):pointer+(i*4)+4])[0]
            if i == 0:
                transform_offset = bone_data_offset + bone_data_start + struct.unpack("Q",file_data[(bone_data_offset + bone_data_start + offset):(bone_data_offset + bone_data_start + offset)+8])[0]
            elif i == 1:
                parents_offset = bone_data_offset + bone_data_start + struct.unpack("Q",file_data[(bone_data_offset + bone_data_start + offset):(bone_data_offset + bone_data_start + offset)+8])[0]
            # We could parse the bone ids here, but we don't need them for importing since any changes to the bone names will change the bone ids

        bone_names_start, bone_names_size, bone_names_subsection_count = struct.unpack("III", file_data[bone_names_offset:bone_names_offset+12])

        pointer = bone_names_offset + 0x0C
        name_offsets = []
        for i in range(bone_names_subsection_count):
            offset = struct.unpack("I", file_data[pointer+(i*4):pointer+(i*4)+4])[0]
            if i != 0: # We don't need the first subsection since it is redundant
                name_offsets.append(bone_names_offset + bone_names_start + struct.unpack("Q",file_data[(bone_names_offset + bone_names_start + offset):(bone_names_offset + bone_names_start + offset)+8])[0])
        bones = []
        assert(len(name_offsets) == bone_count)
        for i in range(bone_count):
            transform = struct.unpack("8f", file_data[transform_offset + (i*4*8):transform_offset + (i*4*8)+4*8])
            bones.append(
                {
                    "parent": struct.unpack("i", file_data[parents_offset + (i*4):parents_offset + (i*4)+4])[0],
                    "rotation": mathutils.Quaternion((transform[3], transform[0], transform[1], transform[2])),
                    "location": mathutils.Vector((transform[4], transform[5], transform[6])),
                    "name": file_data[name_offsets[i]:].split(b'\0')[0].decode('utf-8')
                })

        armature_data = bpy.data.armatures.new("armature")
        armature_object = bpy.data.objects.new("skeleton", armature_data)

        bpy.context.collection.objects.link(armature_object)

        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.view_layer.objects.active = armature_object

        # Creating Joints
        bpy.ops.object.mode_set(mode='EDIT')

        for bone in bones:
            joint = armature_data.edit_bones.new(bone["name"])
            parent_matrix = mathutils.Matrix.Identity(4)
            if bone["parent"] >= 0:
                joint.parent = armature_data.edit_bones[bones[bone["parent"]]["name"]]
                parent_matrix = armature_data.edit_bones[bones[bone["parent"]]["name"]].matrix

            joint.length = 0.01
            joint.matrix = parent_matrix @ mathutils.Matrix.LocRotScale(bone["location"], bone["rotation"], (1, 1, 1))
        
        for joint in armature_data.edit_bones:
            joint.matrix = right_hand_matrix @ joint.matrix
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.context.window.cursor_set("DEFAULT")
        return {'FINISHED'}
        
    def import_binfbx(self):
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx")
        # try to find runtime data path
        runtime_data_path = os.path.abspath(self.filepath)
        data_folder_index = max(runtime_data_path.find(
            os.sep + "data" + os.sep), runtime_data_path.find(os.sep + "data_pc" + os.sep))
        if data_folder_index != -1:
            runtime_data_path = runtime_data_path[:data_folder_index]
            print("Runtime data path found at ", runtime_data_path)
        else:
            runtime_data_path = ""
            print("Runtime data path NOT found.", runtime_data_path)
        # Open File
        file = open(self.filepath, "rb")
        # Read Magick
        Magick = struct.unpack("I", file.read(4))
        if Magick[0] != FBXMAGICK:
            self.report({'ERROR'}, "Invalid BinFBX file")
            return {'CANCELLED'}

        VertexBufferSizes = [0, 0]
        (VertexBufferSizes[0], VertexBufferSizes[1], IndexCount,
         IndexSize) = struct.unpack("IIII", file.read(16))
        # Read Buffers
        VertexBuffers = [file.read(VertexBufferSizes[0]),
                         file.read(VertexBufferSizes[1])]
        IndexBuffer = file.read(IndexCount * IndexSize)

        IndexFormat = ""
        if IndexSize == 1:
            IndexFormat = "3B"
        elif IndexSize == 2:
            IndexFormat = "3H"
        elif IndexSize == 4:
            IndexFormat = "3I"

        (JointCount, ) = struct.unpack('I', file.read(4))
        # Read Skeleton
        # We have to keep the stored joint order because of Blender's bone order shenanigans
        JointNames = []
        if JointCount > 0:
            armature_data = bpy.data.armatures.new("armature")
            armature_object = bpy.data.objects.new("skeleton", armature_data)

            bpy.context.collection.objects.link(armature_object)

            bpy.ops.object.select_all(action='DESELECT')
            bpy.context.view_layer.objects.active = armature_object

            # Creating Joints
            bpy.ops.object.mode_set(mode='EDIT')

            joints = []
            # Sadly a parent joint may appear after its child, so we need to do multiple passes
            # Pass 1 - Collect Data
            bpy.context.window_manager.progress_begin(0, JointCount)
            for i in range(JointCount):
                JointName = file.read(struct.unpack(
                    'I', file.read(4))[0]).decode('utf-8')
                JointNames.append(JointName)
                matrix = struct.unpack('12f', file.read(4*12))
                tail = right_hand_matrix @ mathutils.Vector(
                    struct.unpack('3f', file.read(12)))
                radius = struct.unpack('f', file.read(4))[0]
                parent = struct.unpack('i', file.read(4))
                rotation = mathutils.Matrix(((matrix[0], matrix[1], matrix[2], 0.0), (
                    matrix[3], matrix[4], matrix[5], 0.0), (matrix[6], matrix[7], matrix[8], 0.0), (0.0, 0.0, 0.0, 1.0)))
                translation = mathutils.Matrix.Translation(
                    (matrix[9], matrix[10], matrix[11]))
                # This is the inverted skeleton with parent transforms applied, so we need to invert it with -1 scale
                transform = (right_hand_matrix @ (mathutils.Matrix.Scale(-1.0, 4)
                             @ rotation @ translation) @ right_hand_matrix)
                joints.append([JointName, transform, parent[0], tail, radius])
                bpy.context.window_manager.progress_update(i)
            bpy.context.window_manager.progress_end()

            # Pass 2 - Create Bones
            i = 0
            bpy.context.window_manager.progress_begin(i, len(joints))
            for joint in joints:
                armature_data.edit_bones.new(joint[0])
                bpy.context.window_manager.progress_update(i)
                i += 1
            bpy.context.window_manager.progress_end()
            # Pass 3 - Assign Parent and Matrix
            i = 0
            bpy.context.window_manager.progress_begin(i, len(joints))
            for joint in joints:
                if joint[2] >= 0:
                    armature_data.edit_bones[joint[0]
                                             ].parent = armature_data.edit_bones[joints[joint[2]][0]]
                armature_data.edit_bones[joint[0]].matrix = joint[1]
                # Avoid zero length bones as well as unused radius and tail going to the origin
                if Vector3IsClose(joint[3], armature_data.edit_bones[joint[0]].head) or joint[3].length == 0.0:
                    armature_data.edit_bones[joint[0]].length = 0.01
                else:
                    armature_data.edit_bones[joint[0]].tail = joint[3]

                if joint[4] > 0.0:
                    armature_data.edit_bones[joint[0]].tail_radius = joint[4]
                    armature_data.edit_bones[joint[0]].head_radius = joint[4]
                bpy.context.window_manager.progress_update(i)
                i += 1
            bpy.context.window_manager.progress_end()

            bpy.ops.object.mode_set(mode='OBJECT')
            assert(len(armature_data.bones) == JointCount)

        # Global params block (after joints)
        (Reserved0, Reserved1) = struct.unpack("ii", file.read(8))
        (GlobalScale, ) = struct.unpack("f", file.read(4))
        (LODThresholdCount, ) = struct.unpack("I", file.read(4))
        LODThresholds = ()
        if LODThresholdCount > 0:
            LODThresholds = struct.unpack(str(LODThresholdCount) + 'f', file.read(LODThresholdCount * 4))

        (MirrorSign, ) = struct.unpack("f", file.read(4))
        AABBCenter = struct.unpack("fff", file.read(12))
        (BoundingSphereRadius, ) = struct.unpack("f", file.read(4))
        AABBMin = struct.unpack("fff", file.read(12))
        AABBMax = struct.unpack("fff", file.read(12))

        # Global LOD count (number of LOD levels present)
        (GlobalLODCount, ) = struct.unpack("I", file.read(4))

        if self.debug:
            print("GlobalParams:")
            print("  Reserved:", Reserved0, Reserved1)
            print("  GlobalScale:", GlobalScale)
            print("  LODThresholdCount:", LODThresholdCount, "Thresholds:", LODThresholds)
            print("  MirrorSign:", MirrorSign)
            print("  AABBCenter:", AABBCenter, "SphereRadius:", BoundingSphereRadius)
            print("  AABBMin:", AABBMin, "AABBMax:", AABBMax)
            print("  GlobalLODCount:", GlobalLODCount)

        # Read Materials
        Materials = []
        (MaterialCount, ) = struct.unpack('I', file.read(4))
        bpy.context.window_manager.progress_begin(0, JointCount)
        for i in range(MaterialCount):
            # Material Magick
            struct.unpack("I", file.read(4))
            # Material ID
            struct.unpack("8B", file.read(8))

            # Material Name
            MaterialName = file.read(struct.unpack(
                'I', file.read(4))[0]).decode('utf-8')
            # Material Type
            file.read(struct.unpack('I', file.read(4))[0]).decode('utf-8')
            # Material Path
            file.read(struct.unpack('I', file.read(4))[0]).decode('utf-8')

            material = bpy.data.materials.new(MaterialName)
            material.use_nodes = True
            nodes = material.node_tree.nodes
            links = material.node_tree.links

            nodes.clear()

            # Add a Principled Shader node
            node_principled = nodes.new(type='ShaderNodeBsdfPrincipled')
            node_principled.location = 0, 0

            # Add the Output node
            node_output = nodes.new(type='ShaderNodeOutputMaterial')
            node_output.location = 400, 0

            links.new(
                node_principled.outputs["BSDF"], node_output.inputs["Surface"])

            struct.unpack("6I", file.read(24))

            (UniformCount, ) = struct.unpack('I', file.read(4))

            node_y_location = 0
            for j in range(UniformCount):
                # Uniform Name
                UniformName = file.read(struct.unpack(
                    'I', file.read(4))[0]).decode('utf-8')
                # Uniform Type
                (UniformType, ) = struct.unpack("I", file.read(4))
                if UniformType == FLOAT:
                    struct.unpack("f", file.read(4))
                elif UniformType == RANGE:
                    struct.unpack("2f", file.read(8))
                elif UniformType == COLOR:
                    struct.unpack("4f", file.read(16))
                elif UniformType == VECTOR:
                    struct.unpack("3f", file.read(12))
                elif UniformType == TEXTUREMAP:
                    image_path = file.read(struct.unpack(
                        'I', file.read(4))[0]).decode('utf-8')
                    if runtime_data_path != "":
                        image_path = image_path.replace(
                            "runtimedata", runtime_data_path)
                        try:
                            # Add the Image Texture node
                            node_tex = nodes.new('ShaderNodeTexImage')
                            # Assign the image
                            image = bpy.data.images.get(
                                os.path.basename(image_path))
                            if image:
                                node_tex.image = image
                            else:
                                node_tex.image = bpy.data.images.load(
                                    image_path)
                                node_tex.image.colorspace_settings.name = 'Non-Color'
                            node_tex.location = -800, node_y_location
                            if UniformName.find("g_sColorMap") != -1:
                                node_tex.image.colorspace_settings.name = 'sRGB'
                                links.new(
                                    node_tex.outputs["Color"], node_principled.inputs["Base Color"])
                            # Note: I could not find the difference between g_sSpecularColorMap and g_sSpecShiftMap
                            # They may have the same use on different shaders so they are named differently.
                            elif UniformName.find("g_sSpecularColorMap") != -1 or UniformName.find("g_sSpecShiftMap") != -1:
                                links.new(
                                    node_tex.outputs["Color"], node_principled.inputs["Specular"])
                            elif UniformName.find("g_sNormalMap") != -1:
                                node_normal = nodes.new('ShaderNodeNormalMap')
                                node_normal.location = -400, node_y_location
                                links.new(
                                    node_normal.outputs["Normal"], node_principled.inputs["Normal"])
                                links.new(
                                    node_tex.outputs["Color"], node_normal.inputs["Color"])
                            elif UniformName.find("g_sSmoothnessMap") != -1:
                                node_invert = nodes.new('ShaderNodeInvert')
                                node_invert.location = -400, node_y_location
                                links.new(
                                    node_invert.outputs["Color"], node_principled.inputs["Roughness"])
                                links.new(
                                    node_tex.outputs["Color"], node_invert.inputs["Color"])
                            elif UniformName.find("g_sAlphaTestSampler") != -1:
                                material.blend_method = 'BLEND'
                                links.new(
                                    node_tex.outputs["Alpha"], node_principled.inputs["Alpha"])
                            node_y_location += 400
                        except:
                            print("Image NOT found:", image_path)

                elif UniformType == TEXTURESAMPLER:
                    pass
                elif UniformType == BOOLEAN:
                    struct.unpack("I", file.read(4))
                elif UniformType == 0x10:
                    # Unknown uniform type observed in some assets; appears to carry no payload
                    pass
            Materials.append(material)
            bpy.context.window_manager.progress_update(i)
        bpy.context.window_manager.progress_end()

        (MaterialMapCount, ) = struct.unpack('I', file.read(4))
        # First Material Map
        MaterialMaps = []
        MaterialMaps.append(struct.unpack(
            str(MaterialMapCount) + 'I', file.read(MaterialMapCount*4)))

        (AlternateMaterialMapCount, ) = struct.unpack('I', file.read(4))
        for i in range(AlternateMaterialMapCount):
            file.read(struct.unpack('I', file.read(4))[0]).decode('utf-8')
            struct.unpack(str(MaterialMapCount) + 'I',
                          file.read(MaterialMapCount*4))

        # Second Material Map
        (count, ) = struct.unpack('I', file.read(4))
        MaterialMaps.append(struct.unpack(
            str(count) + 'I', file.read(count*4)))

        # Read Meshes
        MeshCollectionNames = ["Group0", "Group1"]
        Meshes = {}
        for MeshCollectionName in MeshCollectionNames:
            (MeshCount, ) = struct.unpack('I', file.read(4))
            MeshCollection = None
            if MeshCount > 0:
                MeshCollection = bpy.data.collections.new(MeshCollectionName)
                bpy.context.scene.collection.children.link(
                    MeshCollection)  # Add the collection to the scene
            LOD = -1
            LODMeshIndex = None
            LODCollection = None
            bpy.context.window_manager.progress_begin(0, MeshCount)
            for i in range(MeshCount):
                VertexOffsets = [0, 0]
                old_lod = LOD
                (LOD, VertexCount, FaceCount, VertexOffsets[0], VertexOffsets[1], IndexOffset) = struct.unpack(
                    '6I', file.read(6*4))
                if old_lod != LOD:
                    LODCollection = bpy.data.collections.new(
                        MeshCollectionName + "-LOD-" + str(LOD))
                    MeshCollection.children.link(LODCollection)
                    LODMeshIndex = 0
                mesh_data = bpy.data.meshes.new(
                    MeshCollectionName + "LOD-"+str(LOD)+"-Mesh-"+str(LODMeshIndex))
                mesh_object = bpy.data.objects.new(
                    MeshCollectionName + "LOD-"+str(LOD)+"-Mesh-"+str(LODMeshIndex), mesh_data)
                LODMeshIndex += 1
                LODCollection.objects.link(mesh_object)

                bpy.ops.object.select_all(action='DESELECT')
                bpy.context.view_layer.objects.active = mesh_object

                # Per-mesh flags and bounds
                # Flags0 (bitfield)
                (MeshFlags0,) = struct.unpack('i', file.read(4))
                # Bounding Sphere as 4x float32: (center.x, center.y, center.z, radius)
                MeshBoundingSphere = struct.unpack('4f', file.read(4*4))
                # Bounding Box as 6x float32: (min.x, min.y, min.z, max.x, max.y, max.z)
                MeshBoundingBox = struct.unpack('6f', file.read(6*4))

                # Flags1 (bitfield)
                (MeshFlags1,) = struct.unpack('i', file.read(4))

                if self.debug:
                    print(
                        f"MeshParams group={MeshCollectionName} LOD={LOD} idx={LODMeshIndex-1} "
                        f"flags0=0x{MeshFlags0:08X} flags1=0x{MeshFlags1:08X} "
                        f"sphere={MeshBoundingSphere} box={MeshBoundingBox}"
                    )
                    cx, cy, cz, rr = MeshBoundingSphere
                    minx, miny, minz, maxx, maxy, maxz = MeshBoundingBox
                    ex, ey, ez = abs(maxx - minx), abs(maxy - miny), abs(maxz - minz)
                    print(
                        f"  sphere=(cx={cx:.6f}, cy={cy:.6f}, cz={cz:.6f}, r={rr:.6f})"
                    )
                    print(
                        f"  box=min({minx:.6f}, {miny:.6f}, {minz:.6f}) max({maxx:.6f}, {maxy:.6f}, {maxz:.6f}) extents=({ex:.6f}, {ey:.6f}, {ez:.6f})"
                    )

                (VertexAttribCount, ) = struct.unpack('B', file.read(1))
                VertexAttribs = [[], []]
                FormatStrings = ["", ""]
                AttribIndex = [0, 0]
                SemanticCount = { POSITION: 0, NORMAL: 0, TEXCOORD: 0, TANGENT: 0, BONE_INDEX: 0, BONE_WEIGHT: 0 }
                for j in range(VertexAttribCount):
                    (BufferIndex, Type, Semantic, Zero) = struct.unpack(
                        '4B', file.read(4))
                    # Why are these switched?
                    if BufferIndex == 0:
                        BufferIndex = 1
                    elif BufferIndex == 1:
                        BufferIndex = 0
                    if Semantic not in SemanticCount:
                        SemanticCount[Semantic] = 0
                    if self.debug:
                        print(BufferIndex, Type, Semantic, Zero)
                    VertexAttribs[BufferIndex].append({"Type": Type, "Semantic": Semantic, "SemanticIndex": SemanticCount[Semantic],
                                                      "Index": AttribIndex[BufferIndex], "IndexCount": FormatIndexCount[Type]})
                    SemanticCount[Semantic] += 1
                    FormatStrings[BufferIndex] += Format[Type]
                    AttribIndex[BufferIndex] += FormatIndexCount[Type]
                if self.debug:
                    print(VertexAttribs)
                    print(FormatStrings)
                    print(struct.calcsize(FormatStrings[0]))
                    print(struct.calcsize(FormatStrings[1]))
                # Unknown
                struct.unpack('i', file.read(4))
                # Unknown
                struct.unpack('f', file.read(4))
                # Unknown
                struct.unpack('B', file.read(1))
                # Unknown
                struct.unpack('f', file.read(4))

                # At this point all data is read from the file, so we can start creating the mesh

                # Avoid extracting data more than once if the vertex range is the same
                if (VertexCount, VertexOffsets[0], VertexOffsets[1]) not in Meshes:
                    MeshData = {"Positions": [], "Normals": [], "UVs": [
                    ], "Tangents": [], "Indices": [], "Weights": []}
                    for j in range(SemanticCount[NORMAL]):
                        MeshData["Normals"].append([])
                    for j in range(SemanticCount[TEXCOORD]):
                        MeshData["UVs"].append([])
                    for j in range(SemanticCount[TANGENT]):
                        MeshData["Tangents"].append([])
                    for j in range(SemanticCount[BONE_INDEX]):
                        MeshData["Indices"].append([])
                    for j in range(SemanticCount[BONE_WEIGHT]):
                        MeshData["Weights"].append([])
                    for j in range(2):
                        for vertex in struct.iter_unpack(FormatStrings[j], VertexBuffers[j][VertexOffsets[j]:VertexOffsets[j] + (VertexCount*struct.calcsize(FormatStrings[j]))]):
                            for attrib in VertexAttribs[j]:
                                if attrib["Semantic"] == POSITION:
                                    # Position is always 3 floats
                                    assert attrib["Type"] == FLOAT3
                                    # There should only be one position semantic
                                    assert attrib["SemanticIndex"] == 0
                                    MeshData["Positions"].append(right_hand_matrix @ mathutils.Vector(
                                        (vertex[attrib["Index"]], vertex[attrib["Index"] + 1], vertex[attrib["Index"] + 2])))
                                    #if self.debug:
                                    #    print(vertex[attrib["Index"]], vertex[attrib["Index"] + 1], vertex[attrib["Index"] + 2])

                                elif attrib["Semantic"] == NORMAL:
                                    # We're only supporting SHORT4_SNORM normals for now
                                    assert attrib["Type"] == SHORT4_SNORM
                                    MeshData["Normals"][attrib["SemanticIndex"]].append(right_hand_matrix @ mathutils.Vector(
                                        (vertex[attrib["Index"]]/32767.0, vertex[attrib["Index"] + 1]/32767.0, vertex[attrib["Index"] + 2]/32767.0)))

                                elif attrib["Semantic"] == TEXCOORD:
                                    # We're only supporting SHORT2_SNORM texcoords for now
                                    assert attrib["Type"] == SHORT2_SNORM
                                    MeshData["UVs"][attrib["SemanticIndex"]].append(
                                        (vertex[attrib["Index"]]/4095.0, 1.0-(vertex[attrib["Index"] + 1]/4095.0)))

                                elif attrib["Semantic"] == TANGENT:
                                    # This can be commented out as tangents cannot be directly set in Blender
                                    # We're only supporting BYTE4_SNORM tangents for now
                                    assert attrib["Type"] == BYTE4_SNORM
                                    MeshData["Tangents"][attrib["SemanticIndex"]].append(
                                        (vertex[attrib["Index"]]/255.0, vertex[attrib["Index"] + 1]/255.0, vertex[attrib["Index"] + 2]/255.0, vertex[attrib["Index"] + 3]/255.0))

                                elif attrib["Semantic"] == BONE_INDEX:                                    
                                    # We're only supporting SHORT4_UINT or BYTE4_UINT indices for now
                                    assert attrib["Type"] == SHORT4_UINT or attrib["Type"] == BYTE4_UINT
                                    MeshData["Indices"][attrib["SemanticIndex"]].append(
                                        (vertex[attrib["Index"]], vertex[attrib["Index"] + 1], vertex[attrib["Index"] + 2], vertex[attrib["Index"] + 3]))
                                elif attrib["Semantic"] == BONE_WEIGHT:
                                    # We're only supporting BYTE4_UNORM weights for now
                                    assert attrib["Type"] == BYTE4_UNORM
                                    MeshData["Weights"][attrib["SemanticIndex"]].append(
                                        (vertex[attrib["Index"]]/255.0, vertex[attrib["Index"] + 1]/255.0, vertex[attrib["Index"] + 2]/255.0, vertex[attrib["Index"] + 3]/255.0))
                    Meshes[(VertexCount, VertexOffsets[0],
                            VertexOffsets[1])] = MeshData

                MeshData = Meshes[(
                    VertexCount, VertexOffsets[0], VertexOffsets[1])]
                Faces = []
                Positions = []
                Normals = []
                UVs = []
                Tangents = []
                Indices = []
                Weights = []
                VertexMap = {}
                for j in range(SemanticCount[NORMAL]):
                    Normals.append([])
                for j in range(SemanticCount[TEXCOORD]):
                    UVs.append([])
                for j in range(SemanticCount[TANGENT]):
                    Tangents.append([])
                for j in range(SemanticCount[BONE_INDEX]):
                    Indices.append([])
                for j in range(SemanticCount[BONE_WEIGHT]):
                    Weights.append([])

                for triangle in struct.iter_unpack(IndexFormat, IndexBuffer[IndexOffset*IndexSize:(IndexOffset*IndexSize)+(FaceCount*3*IndexSize)]):
                    face = []
                    for index in triangle:
                        if index not in VertexMap:
                            VertexMap[index] = len(Positions)
                            Positions.append(MeshData["Positions"][index])
                            for j in range(SemanticCount[NORMAL]):
                                Normals[j].append(
                                    MeshData["Normals"][j][index])
                            for j in range(SemanticCount[TEXCOORD]):
                                UVs[j].append(MeshData["UVs"][j][index])
                            for j in range(SemanticCount[TANGENT]):
                                Tangents[j].append(
                                    MeshData["Tangents"][j][index])
                            for j in range(SemanticCount[BONE_INDEX]):
                                Indices[j].append(
                                    MeshData["Indices"][j][index])
                            for j in range(SemanticCount[BONE_WEIGHT]):
                                Weights[j].append(
                                    MeshData["Weights"][j][index])
                        face.insert(0, VertexMap[index])
                    Faces.append(face)

                mesh_data.from_pydata(Positions, [], Faces)
                if (4, 1, 0) > bpy.app.version:
                    mesh_data.use_auto_smooth = True

                for j in range(SemanticCount[NORMAL]):
                    mesh_data.normals_split_custom_set_from_vertices(
                        Normals[j])

                for j in range(SemanticCount[TEXCOORD]):
                    mesh_data.uv_layers.new(name="UV"+str(j))
                    for k in range(len(mesh_data.uv_layers[j].data)):
                        mesh_data.uv_layers[j].data[k].uv = UVs[j][mesh_data.loops[k].vertex_index]

                # Cannot directly set tangents [sadface]
                mesh_data.calc_tangents()

                if SemanticCount[BONE_INDEX] != 0:
                    armature_modifier = mesh_object.modifiers.new(
                        'armature', 'ARMATURE')
                    armature_modifier.object = armature_object
                    armature_modifier.use_bone_envelopes = False
                    armature_modifier.use_vertex_groups = True

                    for vertex in mesh_data.vertices:
                        for j in range(SemanticCount[BONE_INDEX]):
                            for k in range(4):
                                # Skip 0 weights or vertices already added
                                if len(Weights) > 0 and Weights[j][vertex.index][k] == 0:
                                    continue
                                if JointNames[Indices[j][vertex.index][k]] not in mesh_object.vertex_groups:
                                    mesh_object.vertex_groups.new(
                                        name=JointNames[Indices[j][vertex.index][k]])
                                weight = Weights[j][vertex.index][k] if len(Weights) > 0 else 1.0
                                mesh_object.vertex_groups[JointNames[Indices[j][vertex.index][k]]].add(
                                    [vertex.index], weight, 'ADD')

                mesh_object.data.materials.append(
                    Materials[MaterialMaps[MeshCollectionNames.index(MeshCollectionName)][i]])
                bpy.context.window_manager.progress_update(i)
            bpy.context.window_manager.progress_end()

        file.close()
        bpy.context.view_layer.update()
        bpy.context.window.cursor_set("DEFAULT")
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(self, "debug")
