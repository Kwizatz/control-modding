# Copyright (C) 2021 Rodrigo Jose Hernandez Cordoba
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
import operator
import itertools
import sys
from multiprocessing import Pool
from multiprocessing.dummy import Pool as ThreadPool, Lock as ThreadLock

MAGICK = 0x2e
FLOAT = 0x00
RANGE = 0x01
COLOR = 0x03
VECTOR = 0x02
TEXTUREMAP = 0x09
TEXTURESAMPLER = 0x08
BOOLEAN = 0x0C

POSITION = 0x0
NORMAL   = 0x1
TEXCOORD = 0x2
TANGENT  = 0x3
INDEX    = 0x5
WEIGHT   = 0x6

R32G32B32_FLOAT   = 0x2
B8G8R8A8_UNORM    = 0x4
R8G8B8A8_UINT     = 0x5
R16G16_SINT       = 0x7
R16G16B16A16_SINT = 0x8
R16G16B16A16_UINT = 0xd

Format = {
    R32G32B32_FLOAT   : '3f',
    B8G8R8A8_UNORM    : '4B',
    R8G8B8A8_UINT     : '4B',
    R16G16_SINT       : '2h',
    R16G16B16A16_SINT : '4h',
    R16G16B16A16_UINT : '4h'
}

FormatIndexCount = {
    R32G32B32_FLOAT   : 3,
    B8G8R8A8_UNORM    : 4,
    R8G8B8A8_UINT     : 4,
    R16G16_SINT       : 2,
    R16G16B16A16_SINT : 4,
    R16G16B16A16_UINT : 4
}

right_hand_matrix = mathutils.Matrix(((-1, 0, 0, 0), (0, -1, 0, 0), (0, 0, -1, 0), (0, 0, 0, 1)))

def Vector3IsClose(v1, v2):
    return abs(v2[0] - v1[0]) < 0.001 and abs(v2[1] - v1[1]) < 0.001 and abs(v2[2] - v1[2]) < 0.001
class BINFBX_OT_importer(bpy.types.Operator):

    '''Imports a binfbx file'''
    bl_idname = "import.binfbx"
    bl_label = "Import BinFBX"

    filepath: bpy.props.StringProperty(name="BinFBX", subtype='FILE_PATH')
    #directory: bpy.props.StringProperty(name="Path", subtype='DIR_PATH')
    #runtimedata: bpy.props.StringProperty(name="Runtime Data Path", subtype='DIR_PATH')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx")
        # Open File
        file = open(self.filepath, "rb")
        # Read Magick
        Magick = struct.unpack("I",file.read(4))
        if Magick[0] != MAGICK:
            self.report({'ERROR'}, "Invalid BinFBX file")
            return {'CANCELLED'}

        VertexBufferSizes = [0, 0]
        (VertexBufferSizes[0], VertexBufferSizes[1], IndexCount, IndexSize) = struct.unpack("IIII",file.read(16))
        # Read Buffers
        VertexBuffers = [ file.read(VertexBufferSizes[0]), file.read(VertexBufferSizes[1]) ]
        IndexBuffer   = file.read(IndexCount * IndexSize)

        IndexFormat = ""
        if IndexSize == 1:
            IndexFormat = "3B"
        elif IndexSize == 2:
            IndexFormat = "3H"
        elif IndexSize == 4:
            IndexFormat = "3I"

        ( JointCount, ) = struct.unpack('I',file.read(4))
        print("JointCount:", JointCount)
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
            for i in range(JointCount):
                JointName = file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                JointNames.append(JointName)
                matrix = struct.unpack('12f',file.read(4*12))
                tail = mathutils.Vector(struct.unpack('3f',file.read(12)))
                radius = struct.unpack('f',file.read(4))[0]
                parent = struct.unpack('i',file.read(4))
                rotation = mathutils.Matrix(((matrix[0], matrix[1], matrix[2], 0.0), (matrix[3], matrix[4], matrix[5], 0.0), (matrix[6], matrix[7], matrix[8], 0.0), (0.0,0.0,0.0, 1.0)))
                translation = mathutils.Matrix.Translation((matrix[9], matrix[10], matrix[11]))
                transform = (right_hand_matrix @ (rotation @ translation) @ right_hand_matrix)
                joints.append([JointName, transform , parent[0], tail, radius])
            # Pass 2 - Create Bones
            for joint in joints:
                armature_data.edit_bones.new(joint[0])
            # Pass 3 - Assign Parent and Matrix
            for joint in joints:
                if joint[2] >= 0:
                    armature_data.edit_bones[joint[0]].parent = armature_data.edit_bones[joints[joint[2]][0]]
                armature_data.edit_bones[joint[0]].matrix = joint[1]
                # Avoid zero length bones as well as unused radius and tail going to the origin
                if Vector3IsClose(joint[3] , armature_data.edit_bones[joint[0]].head) or joint[3].length == 0.0:
                    armature_data.edit_bones[joint[0]].length = 0.01
                else:
                    armature_data.edit_bones[joint[0]].tail = joint[3]

                if joint[4] > 0.0:
                    armature_data.edit_bones[joint[0]].tail_radius = joint[4]
                    armature_data.edit_bones[joint[0]].head_radius = joint[4]

            bpy.ops.object.mode_set(mode='OBJECT')
            assert(len(armature_data.bones) == JointCount)

        # Skip Unknown Data
        struct.unpack("II",file.read(8))
        struct.unpack("f",file.read(4))
        ( count, ) = struct.unpack("I",file.read(4))
        for i in range(count):
            struct.unpack("f",file.read(4))

        struct.unpack("f",file.read(4))
        struct.unpack("fff",file.read(12))
        struct.unpack("f",file.read(4))
        struct.unpack("fff",file.read(12))
        struct.unpack("fff",file.read(12))

        # LOD Count
        struct.unpack("I",file.read(4))

        # Read Materials
        Materials = []
        ( MaterialCount, ) = struct.unpack('I',file.read(4))
        for i in range(MaterialCount):
            print("Material", i)
            # Material Magick
            struct.unpack("I",file.read(4))
            # Material ID
            struct.unpack("8B",file.read(8))
            
            #Material Name
            MaterialName = file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            #Material Type
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            #Material Path
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            
            material = bpy.data.materials.new(MaterialName)
            #material.use_nodes = True

            struct.unpack("6I",file.read(24))

            ( UniformCount, ) = struct.unpack('I',file.read(4))

            #print("Uniform Count:", UniformCount)
            for j in range(UniformCount):
                #Uniform Name
                file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                # Uniform Type
                ( UniformType, ) = struct.unpack("I",file.read(4))
                if UniformType == FLOAT:
                    struct.unpack("f",file.read(4))
                elif UniformType == RANGE:
                    struct.unpack("2f",file.read(8))
                elif UniformType == COLOR:
                    struct.unpack("4f",file.read(16))
                elif UniformType == VECTOR:
                    struct.unpack("3f",file.read(12))
                elif UniformType == TEXTUREMAP:
                    image_path = file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                    image_path = image_path.replace("\\", os.sep)
                    image_path = image_path.replace("/", os.sep)
                    image_path = image_path.replace("runtimedata", self.runtimedata)
                    if os.path.exists(image_path.strip()):
                        # TODO: Load Image as DDS and add to material
                        print("Image Path", image_path, "exists")
                    #else:
                    #    print(image_path, "does not exist, fill the Runtime Data Path field to change where the textures are looked for")
                elif UniformType == TEXTURESAMPLER:
                    pass
                elif UniformType == BOOLEAN:
                    struct.unpack("I",file.read(4))
            Materials.append(material)

        ( MaterialMapCount, ) = struct.unpack('I',file.read(4))
        print("Material Map Count:", MaterialMapCount)
        # First Material Map
        MaterialMaps = []
        MaterialMaps.append(struct.unpack(str(MaterialMapCount) + 'I',file.read(MaterialMapCount*4)))

        ( AlternateMaterialMapCount, ) = struct.unpack('I',file.read(4))
        print("Alternate Material Map Count:", AlternateMaterialMapCount)
        for i in range(AlternateMaterialMapCount):
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            struct.unpack(str(MaterialMapCount) + 'I',file.read(MaterialMapCount*4))

        # Second Material Map
        ( count, ) = struct.unpack('I',file.read(4))
        MaterialMaps.append(struct.unpack(str(count) + 'I',file.read(count*4)))

        # Read Meshes
        MeshCollectionNames = ["Group0", "Group1"]
        for MeshCollectionName in MeshCollectionNames:        
            ( MeshCount, ) = struct.unpack('I',file.read(4))
            MeshCollection = None
            if MeshCount > 0:
                MeshCollection = bpy.data.collections.new(MeshCollectionName)
                bpy.context.scene.collection.children.link(MeshCollection)  # Add the collection to the scene
            LOD = -1
            LODMeshIndex = None
            LODCollection = None
            for i in range(MeshCount):
                VertexOffsets = [0,0]
                old_lod = LOD
                ( LOD, VertexCount, FaceCount, VertexOffsets[0], VertexOffsets[1], IndexOffset ) = struct.unpack('6I',file.read(6*4))
                if old_lod != LOD:
                    LODCollection = bpy.data.collections.new(MeshCollectionName + "-LOD-" + str(LOD))
                    MeshCollection.children.link(LODCollection)
                    LODMeshIndex = 0
                mesh_data = bpy.data.meshes.new(MeshCollectionName + "LOD-"+str(LOD)+"-Mesh-"+str(LODMeshIndex))
                mesh_object = bpy.data.objects.new(MeshCollectionName + "LOD-"+str(LOD)+"-Mesh-"+str(LODMeshIndex), mesh_data)
                LODMeshIndex += 1

                LODCollection.objects.link(mesh_object)

                bpy.ops.object.select_all(action='DESELECT')
                bpy.context.view_layer.objects.active = mesh_object

                # Unknown
                struct.unpack('i',file.read(4))
                # Bounding Sphere
                struct.unpack('4i',file.read(4*4))
                # Bounding Box
                struct.unpack('6i',file.read(6*4))
                
                # Unknown
                struct.unpack('i',file.read(4))

                ( VertexAttribCount, ) = struct.unpack('B',file.read(1))
                VertexAttribs = [[],[]]
                FormatStrings = ["", ""]
                AttribIndex = [0, 0]
                SemanticCount = {}
                for j in range(VertexAttribCount):
                    (BufferIndex, Type, Semantic, Zero) = struct.unpack('4B',file.read(4))
                    # Why are these switched?
                    if BufferIndex == 0:
                        BufferIndex = 1
                    elif BufferIndex == 1:
                        BufferIndex = 0
                    if Semantic not in SemanticCount:
                        SemanticCount[Semantic] = 0
                    VertexAttribs[BufferIndex].append({"Type": Type, "Semantic": Semantic, "SemanticIndex": SemanticCount[Semantic], "Index": AttribIndex[BufferIndex], "IndexCount": FormatIndexCount[Type]})
                    SemanticCount[Semantic] += 1
                    FormatStrings[BufferIndex] += Format[Type]
                    AttribIndex[BufferIndex] += FormatIndexCount[Type]

                Normals = []
                for j in range(SemanticCount[NORMAL]):
                    Normals.append([])
                UVs = []
                for j in range(SemanticCount[TEXCOORD]):
                    UVs.append([])
                Tangents = []
                for j in range(SemanticCount[TANGENT]):
                    Tangents.append([])
                Indices = []
                for j in range(SemanticCount[INDEX]):
                    Indices.append([])
                Weights = []
                for j in range(SemanticCount[WEIGHT]):
                    Weights.append([])
                Positions = []
                for j in range(2):
                    for vertex in struct.iter_unpack(FormatStrings[j], VertexBuffers[j][VertexOffsets[j]:VertexOffsets[j]  + (VertexCount*struct.calcsize(FormatStrings[j]))]):
                        for attrib in VertexAttribs[j]:
                            if attrib["Semantic"] == POSITION:
                                assert attrib["Type"] == R32G32B32_FLOAT # Position is always 3 floats
                                assert attrib["SemanticIndex"] == 0 # There should only be one position semantic
                                Positions.append((vertex[attrib["Index"]],vertex[attrib["Index"] + 1],vertex[attrib["Index"] + 2]))

                            elif attrib["Semantic"] == NORMAL:
                                assert attrib["Type"] == R16G16B16A16_SINT # We're only supporting R16G16B16A16_SINT normals for now
                                Normals[attrib["SemanticIndex"]].append((vertex[attrib["Index"]]/32767.0,vertex[attrib["Index"] + 1]/32767.0,vertex[attrib["Index"] + 2]/32767.0))

                            elif attrib["Semantic"] == TEXCOORD:
                                assert attrib["Type"] == R16G16_SINT # We're only supporting R16G16_SINT texcoords for now
                                UVs[attrib["SemanticIndex"]].append((vertex[attrib["Index"]]/4095.0, 1.0-(vertex[attrib["Index"] + 1]/4095.0)))

                            elif attrib["Semantic"] == TANGENT:
                                # This can be commented out as tangents cannot be directly set in Blender
                                assert attrib["Type"] == B8G8R8A8_UNORM # We're only supporting B8G8R8A8_UNORM tangents for now
                                Tangents[attrib["SemanticIndex"]].append((vertex[attrib["Index"]]/255.0,vertex[attrib["Index"] + 1]/255.0,vertex[attrib["Index"] + 2]/255.0,vertex[attrib["Index"] + 3]/255.0))

                            elif attrib["Semantic"] == INDEX:
                                assert attrib["Type"] == R16G16B16A16_UINT # We're only supporting R16G16B16A16_UINT indices for now
                                Indices[attrib["SemanticIndex"]].append((vertex[attrib["Index"]], vertex[attrib["Index"] + 1], vertex[attrib["Index"] + 2], vertex[attrib["Index"] + 3]))
                                #if vertex[attrib["Index"]]>=913 or vertex[attrib["Index"] + 1]>=913 or vertex[attrib["Index"] + 2]>=913 or vertex[attrib["Index"] + 3]>=913:
                                #    print("WARNING: Index out of range:", vertex[attrib["Index"]], vertex[attrib["Index"] + 1], vertex[attrib["Index"] + 2], vertex[attrib["Index"] + 3], j)
                                

                            elif attrib["Semantic"] == WEIGHT:
                                assert attrib["Type"] == R8G8B8A8_UINT # We're only supporting R8G8B8A8_UINT weights for now
                                Weights[attrib["SemanticIndex"]].append((vertex[attrib["Index"]]/255.0, vertex[attrib["Index"] + 1]/255.0, vertex[attrib["Index"] + 2]/255.0, vertex[attrib["Index"] + 3]/255.0))

                Faces = []
                for j in struct.iter_unpack(IndexFormat, IndexBuffer[IndexOffset*IndexSize:(IndexOffset*IndexSize)+(FaceCount*3*IndexSize)]):
                    Faces.append(j)
                mesh_data.from_pydata(Positions, [], Faces)

                for j in range(SemanticCount[NORMAL]):
                    # This loop will likely only run once, but it's here for completeness
                    mesh_data.use_auto_smooth = True
                    mesh_data.normals_split_custom_set_from_vertices( Normals[j] )

                for j in range(SemanticCount[TEXCOORD]):
                    mesh_data.uv_layers.new(name="UV"+str(j))
                    for k in range(len(mesh_data.uv_layers[j].data)):
                        mesh_data.uv_layers[j].data[k].uv = UVs[j][mesh_data.loops[k].vertex_index]

                # Cannot directly set tangents [sadface]
                mesh_data.calc_tangents()

                armature_modifier = mesh_object.modifiers.new('armature', 'ARMATURE')
                armature_modifier.object = armature_object
                armature_modifier.use_bone_envelopes = False
                armature_modifier.use_vertex_groups = True


                for vertex in mesh_data.vertices:
                    for j in range(SemanticCount[INDEX]):
                        for k in range(4):
                            # Skip 0 weights or vertices already added
                            if Weights[j][vertex.index][k] == 0:
                                continue
                            if JointNames[Indices[j][vertex.index][k]] not in mesh_object.vertex_groups:
                                #print("Added Group",armature_data.bones[Indices[j][loop.vertex_index][k]].name)
                                mesh_object.vertex_groups.new(name=JointNames[Indices[j][vertex.index][k]])
                            mesh_object.vertex_groups[JointNames[Indices[j][vertex.index][k]]].add([vertex.index], Weights[j][vertex.index][k], 'ADD')
               
                # Remove Unused Vertices for this mesh
                #bpy.ops.object.editmode_toggle()
                #bpy.ops.mesh.delete_loose()
                #bpy.ops.object.editmode_toggle()

                # Unknown
                struct.unpack('i',file.read(4))
                # Unknown
                struct.unpack('f',file.read(4))
                # Unknown
                struct.unpack('B',file.read(1))
                # Unknown
                struct.unpack('f',file.read(4))

                mesh_object.data.materials.append(Materials[MaterialMaps[MeshCollectionNames.index(MeshCollectionName)][i]])

        file.close()
        bpy.context.view_layer.update()
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

