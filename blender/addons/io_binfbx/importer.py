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
import struct
import mathutils
import operator
import itertools
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

class BINFBX_OT_importer(bpy.types.Operator):

    '''Imports a binfbx file'''
    bl_idname = "import.binfbx"
    bl_label = "Import BinFBX"

    filepath: bpy.props.StringProperty(subtype='FILE_PATH')

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        # bpy.ops.object.mode_set()
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx")
        # Open File
        file = open(self.filepath, "rb")
        # Read Magick
        Magick = struct.unpack("I",file.read(4))
        if Magick[0] != MAGICK:
            self.report({'ERROR'}, "Invalid BinFBX file")
            return {'CANCELLED'}

        (AttributeBufferSize, VertexBufferSize, IndexCount, IndexSize) = struct.unpack("IIII",file.read(16))
        # Move Past the Buffers
        file.read(AttributeBufferSize + VertexBufferSize + (IndexCount * IndexSize))

        JointCount = struct.unpack('I',file.read(4))[0]
        # Read Skeleton
        print("Joint Count:", JointCount)

        if JointCount > 0:
            armature_data = bpy.data.armatures.new("armature")
            armature_object = bpy.data.objects.new("skeleton", armature_data)

            bpy.context.collection.objects.link(armature_object)

            bpy.ops.object.select_all(action='DESELECT')
            bpy.context.view_layer.objects.active = armature_object

            print("Creating Joints")
            bpy.ops.object.mode_set(mode='EDIT')

            joints = []
            # Sadly a parent joint may appear after its child, so we need to do multiple passes
            # Pass 1 - Collect Data
            for i in range(JointCount):
                JointName = file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                matrix = struct.unpack('12f',file.read(4*12))
                tail = mathutils.Vector(struct.unpack('3f',file.read(12)))
                radius = struct.unpack('f',file.read(4))[0]
                parent = struct.unpack('i',file.read(4))
                rotation = mathutils.Matrix(((matrix[0], matrix[1], matrix[2], 0.0), (matrix[3], matrix[4], matrix[5], 0.0), (matrix[6], matrix[7], matrix[8], 0.0), (0.0,0.0,0.0, 1.0)))
                translation = mathutils.Matrix.Translation((matrix[9], matrix[10], matrix[11]))
                transform = rotation @ translation
                joints.append([JointName, transform , parent[0], tail, radius])
            # Pass 2 - Create Bones
            for joint in joints:
                armature_data.edit_bones.new(joint[0])
            # Pass 3 - Assign Parent and Matrix
            for joint_index in range(JointCount):
                if joints[joint_index][2] >= 0:
                    armature_data.edit_bones[joint_index].parent = armature_data.edit_bones[joints[joint_index][2]]
                armature_data.edit_bones[joint_index].matrix = joints[joint_index][1]
                # Avoid zero length bones as well as unused radius and tail going to the origin 
                if (joints[joint_index][4] == 0.0 and joints[joint_index][3] == mathutils.Vector((0.0,0.0,0.0))) or (armature_data.edit_bones[joint_index].head == -joints[joint_index][3]):
                    armature_data.edit_bones[joint_index].length = 0.01
                else:
                    armature_data.edit_bones[joint_index].tail = -joints[joint_index][3]
                    armature_data.edit_bones[joint_index].tail_radius = joints[joint_index][4]
                    armature_data.edit_bones[joint_index].head_radius = joints[joint_index][4]
            bpy.ops.object.mode_set(mode='OBJECT')

        # Skip Unknown Data
        struct.unpack("II",file.read(8))
        struct.unpack("f",file.read(4))
        ( count ) = struct.unpack("I",file.read(4))
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
        (MaterialCount) = struct.unpack('I',file.read(4))
        for i in range(MaterialCount):
            # Material Magick
            struct.unpack("I",file.read(4))
            # Material ID
            struct.unpack("8B",file.read(8))
            
            #Material Name
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            #Material Type
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            #Material Path
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')

            struct.unpack("6I",file.read(24))

            (UniformCount) = struct.unpack('I',file.read(4))


            for i in range(UniformCount):
                #Uniform Name
                file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                # Uniform Type
                ( UniformType ) = struct.unpack("I",file.read(4))
            
                if UniformType == FLOAT:
                    struct.unpack("f",file.read(4))
                elif UniformType == RANGE:
                    struct.unpack("2f",file.read(8))
                elif UniformType == COLOR:
                    struct.unpack("4f",file.read(16))
                elif UniformType == VECTOR:
                    struct.unpack("f",file.read(12))
                elif UniformType == TEXTUREMAP:
                    file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                elif UniformType == TEXTURESAMPLER:
                    pass
                elif UniformType == BOOLEAN:
                    struct.unpack("I",file.read(4))

        (MaterialMapCount) = struct.unpack('I',file.read(4))
        # Material Map
        struct.unpack(str(MaterialMapCount) + 'I',file.read(MaterialMapCount*4))

        (AlternateMaterialMapCount) = struct.unpack('I',file.read(4))
        for i in range(AlternateMaterialMapCount):
            file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
            struct.unpack(str(MaterialMapCount) + 'I',file.read(MaterialMapCount*4))

        # Unknown Int Array
        struct.unpack(str(MaterialMapCount) + 'I',file.read(MaterialMapCount*4))

        file.close()
        bpy.context.view_layer.update()
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}
