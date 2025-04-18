# Copyright (C) 2021-2023,2025 Rodrigo Jose Hernandez Cordoba
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

from . import importer
from . import exporter
import bpy
bl_info = {
    "name": "Remedy's Control Mesh and Skeleton Format (.binfbx,.binskeleton)",
    "author": "Rodrigo Hernandez",
    "version": (0, 0, 5),
    "blender": (4, 4, 0),
    "location": "File > Import/Export > Control BinFBX",
    "description": "BinFBX importing and exporting",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"
}


def binfbx_import_menu_func(self, context):
    self.layout.operator(
        importer.IMPORT_OT_binfbx.bl_idname,
        text="Control Mesh or Skeleton (.binfbx,.binskeleton)")
    
def binfbx_export_menu_func(self, context):
    self.layout.operator(
        exporter.EXPORT_OT_binfbx.bl_idname,
        text="Control Mesh or Skeleton (.binfbx,.binskeleton)")


def register():
    bpy.utils.register_class(importer.IMPORT_OT_binfbx)
    bpy.types.TOPBAR_MT_file_import.append(binfbx_import_menu_func)

    bpy.utils.register_class(exporter.EXPORT_OT_binfbx)
    bpy.types.TOPBAR_MT_file_export.append(binfbx_export_menu_func)


def unregister():
    bpy.utils.unregister_class(importer.IMPORT_OT_binfbx)
    bpy.types.TOPBAR_MT_file_import.remove(binfbx_import_menu_func)

    bpy.utils.unregister_class(exporter.EXPORT_OT_binfbx)
    bpy.types.TOPBAR_MT_file_export.remove(binfbx_export_menu_func)


if __name__ == "__main__":
    register()
