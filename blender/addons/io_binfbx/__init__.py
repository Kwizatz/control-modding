# Copyright (C) 2021,2022 Rodrigo Jose Hernandez Cordoba
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

bl_info = {
    "name": "Remedy's Control Mesh Format (.binfbx)",
    "author": "Rodrigo Hernandez",
    "version": (1, 0, 0),
    "blender": (2, 80, 0),
    "location": "File > Import/Export > Control BinFBX",
    "description": "BinFBX importing and exporting",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"}

import bpy
from . import importer

def binfbx_import_menu_func(self, context):
    self.layout.operator(
        importer.BINFBX_OT_importer.bl_idname,
        text="BinFBX (.binfbx)")


def register():
    bpy.utils.register_class(importer.BINFBX_OT_importer)
    bpy.types.TOPBAR_MT_file_import.append(binfbx_import_menu_func)

def unregister():
    bpy.utils.unregister_class(importer.BINFBX_OT_importer)
    bpy.types.TOPBAR_MT_file_import.remove(binfbx_import_menu_func)

if __name__ == "__main__":
    register()
