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
            for i in range(JointCount):
                JointName = file.read(struct.unpack('I',file.read(4))[0]).decode('utf-8')
                matrix = struct.unpack('12f',file.read(4*12))
                struct.unpack('4f',file.read(4*4))
                parent = struct.unpack('i',file.read(4))
                joints.append([JointName, ((matrix[0], matrix[1], matrix[2], 0.0), (matrix[3], matrix[4], matrix[5], 0.0), (matrix[6], matrix[7], matrix[8], 0.0), (matrix[9], matrix[10], matrix[11], 1.0)), parent[0]])
                #joints.append([JointName, ((matrix[0], matrix[3], matrix[6], 0.0), (matrix[1], matrix[4], matrix[7], 0.0), (matrix[2], matrix[5], matrix[8], 0.0), (-matrix[9], -matrix[10], -matrix[11], 1.0)), parent[0]])
                print("JointName", JointName, "parent", parent[0])
            for joint in joints:
                armature_data.edit_bones.new(joint[0])
            for joint_index in range(JointCount):
                if joints[joint_index][2] >= 0:
                    armature_data.edit_bones[joint_index].parent = armature_data.edit_bones[joints[joint_index][2]]
            for joint_index in range(JointCount):
                armature_data.edit_bones[joint_index].use_relative_parent = True
                armature_data.edit_bones[joint_index].matrix =  joints[joint_index][1]
                print("joint_index",joint_index,"matrix",armature_data.edit_bones[joint_index].matrix.decompose())
                armature_data.edit_bones[joint_index].length = 0.1

            bpy.ops.object.mode_set(mode='OBJECT')

        file.close()
        bpy.context.view_layer.update()
        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}