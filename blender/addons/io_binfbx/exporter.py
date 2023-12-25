# Copyright (C) 2023 Rodrigo Jose Hernandez Cordoba
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

class EXPORT_OT_binfbx(bpy.types.Operator):
    '''Exports to a binfbx file'''
    bl_idname = "export.binfbx"
    bl_label = "Export BinFBX"

    filepath: bpy.props.StringProperty(name="BinFBX", subtype='FILE_PATH')
    filter_glob: bpy.props.StringProperty(
        default="*.binfbx.mesh",
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        if (context.active_object.type == 'MESH'):
            return True
        return False

    def execute(self, context):
        if (context.active_object.type != 'MESH'):
            return {'CANCELLED'}
        bpy.ops.object.mode_set()
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx.mesh")
        #exporter = MSH_OT_exporterCommon(self.filepath)
        #exporter.run(context.active_object)
        print(context.active_object.name)
        return {'FINISHED'}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = bpy.path.ensure_ext(
                os.path.dirname(
                    bpy.data.filepath) +
                os.sep +
                context.active_object.name,
                "binfbx.mesh")
        else:
            self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx.mesh")
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}
