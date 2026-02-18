# Copyright (C) 2021-2026 Rodrigo Jose Hernandez Cordoba
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
import json
#from multiprocessing import Pool
#from multiprocessing.dummy import Pool as ThreadPool, Lock as ThreadLock

FBXMAGICK = 0x2e
SKELETONMAGICK = 0x2
RBFMAGICK = 0xD34DB33F

FLOAT = 0x00
RANGE = 0x01
VECTOR = 0x02
COLOR = 0x03
TEXTURESAMPLER = 0x08
TEXTUREMAP = 0x09
BOOLEAN = 0x0C
INTEGER = 0x10  # e.g. g_iTextureVariationCount; has a 4-byte int32 payload

# Semantics
POSITION = 0x0
NORMAL = 0x1
TEXCOORD = 0x2
TANGENT = 0x3
VERTEX_COLOR = 0x4  # Per-vertex color, encoded as BYTE4_SNORM
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
    VECTOR: 'vector',
    COLOR: 'color',
    TEXTURESAMPLER: 'texturesampler',
    TEXTUREMAP: 'texturemap',
    BOOLEAN: 'boolean',
    INTEGER: 'integer',
}

SemanticNames = {
    POSITION: 'POSITION',
    NORMAL: 'NORMAL',
    TEXCOORD: 'TEXCOORD',
    TANGENT: 'TANGENT',
    VERTEX_COLOR: 'VERTEX_COLOR',
    BONE_INDEX: 'BONE_INDEX',
    BONE_WEIGHT: 'BONE_WEIGHT',
}

right_hand_matrix = mathutils.Matrix(
    ((-1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))

# RBF node type hashes (tagged binary tree format)
RBF_ROOT    = 0x97C27164
RBF_SECTION = 0x62C7ECBD
RBF_LEAF_A  = 0xED01652B  # Solver entry: input bone ref + component
RBF_LEAF_B  = 0x33D0511D  # Output map: output count + param
RBF_LEAF_C  = 0x341D0EEA  # Pose sample: input quaternion + output translation

# Quaternion component names for RBF
RBF_QUAT_COMP = {0: 'x', 1: 'y', 2: 'z', 3: 'w'}


def fnv1a_lower(name):
    """FNV-1a hash matching Northlight engine (case-folded via OR 0x20)."""
    h = 0x811c9dc5
    for c in name:
        h ^= (ord(c) | 0x20)
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def bone_names_from_skeleton(skel_data):
    """Extract ASCII bone name strings from raw .binskeleton bytes."""
    names = []
    i = 0
    while i < len(skel_data):
        end = i
        while end < len(skel_data) and 32 <= skel_data[end] < 127:
            end += 1
        if end - i >= 3 and end < len(skel_data) and skel_data[end] == 0:
            name = skel_data[i:end].decode('ascii')
            if any(c.isalpha() for c in name):
                names.append(name)
            i = end + 1
        else:
            i += 1
    return names


def load_rbf(filepath):
    """Parse a Northlight .rbf (Radial Basis Function) file.

    Returns a dict with keys:
        version     - file version (2)
        input_bones - list of {'hash': int, 'name': str or None}
        output_bones- list of {'hash': int, 'name': str or None}
        sections    - list of section dicts, each with:
            entries     - list of entry dicts (LEAF_A / B / C combined)
            solver_dim  - uint32 header from data blob
            solver_data - raw bytes of pre-computed solver data
    Each entry dict has:
        num_inputs     - int (1-4), number of input dimensions
        bone_ref       - int, input bone/channel reference
        component      - int (0-3), quaternion component index
        output_count   - int, number of output bones affected
        param          - int, LEAF_B param field
        input_quat     - (qx, qy, qz, qw) sample quaternion
        input_pos      - (tx, ty, tz) output translation
        output_indices - list of int, indices into output_bones table
    """
    with open(filepath, 'rb') as f:
        data = f.read()

    # Root header: magic(4) ver(4) size(4) type(4) num_sections(4) total_count(4)
    magic, ver, root_sz, root_type = struct.unpack_from('<IIII', data, 0)
    if magic != RBFMAGICK:
        raise ValueError(f"Not an RBF file (magic 0x{magic:08X}, expected 0x{RBFMAGICK:08X})")

    num_sections, total_count = struct.unpack_from('<II', data, 16)
    off = 24

    sections = []
    for s in range(num_sections):
        if off + 32 > len(data):
            break
        sec_magic, sec_ver, sec_sz, sec_type = struct.unpack_from('<IIII', data, off)
        if sec_magic != RBFMAGICK:
            break  # Gracefully stop if section header is invalid
        sec_end = off + sec_sz
        unk, data_sz, start_idx, count = struct.unpack_from('<IIII', data, off + 16)
        p = off + 32

        # --- LEAF_A: solver entries ---
        leaf_a = []
        for i in range(count):
            la_magic, la_ver, la_sz, la_type = struct.unpack_from('<IIII', data, p)
            if la_magic != RBFMAGICK:
                break
            always1, num_inputs, bone_ref, component, sentinel = struct.unpack_from('<IIIII', data, p + 16)
            leaf_a.append({'num_inputs': num_inputs, 'bone_ref': bone_ref, 'component': component})
            p += la_sz

        # --- LEAF_B: output maps ---
        b_count = struct.unpack_from('<I', data, p)[0]
        p += 4
        leaf_b = []
        for i in range(b_count):
            lb_magic, lb_ver, lb_sz, lb_type = struct.unpack_from('<IIII', data, p)
            if lb_magic != RBFMAGICK:
                break
            always1, output_count, param, sentinel = struct.unpack_from('<IIII', data, p + 16)
            leaf_b.append({'output_count': output_count, 'param': param})
            p += lb_sz

        # --- LEAF_C: pose samples ---
        c_count = struct.unpack_from('<I', data, p)[0]
        p += 4
        leaf_c = []
        for i in range(c_count):
            lc_magic, lc_ver, lc_sz, lc_type = struct.unpack_from('<IIII', data, p)
            if lc_magic != RBFMAGICK:
                break
            floats = struct.unpack_from('<10f', data, p + 16)
            # floats: [pad0, pad1, qx, qy, qz, qw, tx, ty, tz, sentinel_as_float]
            leaf_c.append({
                'input_quat': (floats[2], floats[3], floats[4], floats[5]),
                'input_pos': (floats[6], floats[7], floats[8]),
            })
            p += lc_sz

        # --- Data blob: solver_dim + output indices + solver data ---
        blob_start = p
        total_outputs = sum(e['output_count'] for e in leaf_b)
        solver_dim = 0
        output_indices_flat = []
        solver_data = b''

        if blob_start < sec_end:
            solver_dim = struct.unpack_from('<I', data, blob_start)[0]
            idx_start = blob_start + 4
            if idx_start + total_outputs * 4 <= sec_end:
                output_indices_flat = list(struct.unpack_from(
                    f'<{total_outputs}I', data, idx_start))
            solver_data_start = idx_start + total_outputs * 4
            if solver_data_start < sec_end:
                solver_data = data[solver_data_start:sec_end]

        # --- Combine into entries ---
        entries = []
        idx_offset = 0
        n = min(len(leaf_a), len(leaf_b), len(leaf_c))
        for i in range(n):
            oc = leaf_b[i]['output_count']
            entry = {
                'num_inputs': leaf_a[i]['num_inputs'],
                'bone_ref': leaf_a[i]['bone_ref'],
                'component': leaf_a[i]['component'],
                'output_count': oc,
                'param': leaf_b[i]['param'],
                'input_quat': leaf_c[i]['input_quat'],
                'input_pos': leaf_c[i]['input_pos'],
                'output_indices': output_indices_flat[idx_offset:idx_offset + oc],
            }
            entries.append(entry)
            idx_offset += oc

        sections.append({
            'entries': entries,
            'solver_dim': solver_dim,
            'solver_data': solver_data,
            'data_sz': data_sz,
            'start_idx': start_idx,
        })
        off = sec_end

    # --- Root tail: input bone hashes, output bone hashes, trailing magic ---
    input_bones = []
    output_bones = []
    if off + 4 <= len(data):
        n_input = struct.unpack_from('<I', data, off)[0]
        if n_input < 10000 and off + 4 + n_input * 4 <= len(data):
            for i in range(n_input):
                h = struct.unpack_from('<I', data, off + 4 + i * 4)[0]
                input_bones.append({'hash': h, 'name': None})
            out_off = off + 4 + n_input * 4
            if out_off + 4 <= len(data):
                n_output = struct.unpack_from('<I', data, out_off)[0]
                if n_output < 100000 and out_off + 4 + n_output * 4 <= len(data):
                    for i in range(n_output):
                        h = struct.unpack_from('<I', data, out_off + 4 + i * 4)[0]
                        output_bones.append({'hash': h, 'name': None})

    return {
        'version': ver,
        'input_bones': input_bones,
        'output_bones': output_bones,
        'sections': sections,
    }


def resolve_rbf_bone_names(rbf_data, hash_to_name):
    """Resolve bone hash values to names using a hash→name mapping dict."""
    for bone in rbf_data['input_bones']:
        bone['name'] = hash_to_name.get(bone['hash'])
    for bone in rbf_data['output_bones']:
        bone['name'] = hash_to_name.get(bone['hash'])


def Vector3IsClose(v1, v2):
    return abs(v2[0] - v1[0]) < 0.001 and abs(v2[1] - v1[1]) < 0.001 and abs(v2[2] - v1[2]) < 0.001


class IMPORT_OT_binfbx(bpy.types.Operator):
    '''Imports a binfbx, binskeleton, or rbf file'''
    bl_idname = "import.binfbx"
    bl_label = "Import"
    filepath: bpy.props.StringProperty(name="BinFBX", subtype='FILE_PATH')
    filter_glob: bpy.props.StringProperty(
        default="*.binfbx;*.binskeleton;*.rbf",
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
        elif ext == ".rbf":
            print("Importing RBF")
            return self.import_rbf()
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

    def import_rbf(self):
        """Import a Northlight .rbf (Radial Basis Function) corrective bone file.

        Parses the RBF solver data and creates a summary empty object with
        custom properties storing the section/entry data.  If a matching
        .binskeleton file is found next to the .rbf, bone hashes are
        resolved to human-readable names.
        """
        self.filepath = bpy.path.ensure_ext(self.filepath, ".rbf")

        # --- Parse the RBF file ---
        try:
            rbf = load_rbf(self.filepath)
        except Exception as e:
            self.report({'ERROR'}, f"Failed to parse RBF: {e}")
            return {'CANCELLED'}

        # --- Attempt to resolve bone hashes via a nearby skeleton ---
        hash_to_name = {}
        rbf_dir = os.path.dirname(self.filepath)
        rbf_base = os.path.splitext(os.path.basename(self.filepath))[0]

        # Search strategy: look for <base>_physx.binskeleton in common locations
        skel_search_dirs = [rbf_dir]
        # Walk up looking for data root
        data_root = os.path.abspath(self.filepath)
        for marker in (os.sep + "data" + os.sep, os.sep + "data_pc" + os.sep):
            idx = data_root.find(marker)
            if idx != -1:
                data_root = data_root[:idx + len(marker) - 1]
                skel_search_dirs.append(os.path.join(data_root, "objects", "characters", "intermediate"))
                break

        skel_candidates = [
            rbf_base + "_physx.binskeleton",
            rbf_base + ".binskeleton",
        ]
        skel_path = None
        for d in skel_search_dirs:
            for cand in skel_candidates:
                p = os.path.join(d, cand)
                if os.path.isfile(p):
                    skel_path = p
                    break
            if skel_path:
                break

        if skel_path:
            try:
                with open(skel_path, 'rb') as f:
                    skel_data = f.read()
                names = bone_names_from_skeleton(skel_data)
                hash_to_name = {fnv1a_lower(n): n for n in names}
                resolve_rbf_bone_names(rbf, hash_to_name)
                print(f"RBF: resolved bone names from {os.path.basename(skel_path)} "
                      f"({len(hash_to_name)} names)")
            except Exception as e:
                print(f"RBF: warning: could not read skeleton {skel_path}: {e}")
        else:
            # Try to resolve from any armature already in the scene
            for obj in bpy.data.objects:
                if obj.type == 'ARMATURE' and obj.data:
                    for bone in obj.data.bones:
                        h = fnv1a_lower(bone.name)
                        hash_to_name[h] = bone.name
            if hash_to_name:
                resolve_rbf_bone_names(rbf, hash_to_name)
                print(f"RBF: resolved bone names from scene armature ({len(hash_to_name)} names)")
            else:
                print("RBF: no skeleton found, bone hashes will not be resolved")

        # --- Print summary ---
        n_input = len(rbf['input_bones'])
        n_output = len(rbf['output_bones'])
        n_sections = len(rbf['sections'])
        total_entries = sum(len(sec['entries']) for sec in rbf['sections'])

        print(f"\n{'='*60}")
        print(f"RBF: {os.path.basename(self.filepath)}")
        print(f"  Version: {rbf['version']}")
        print(f"  Sections: {n_sections}, Total entries: {total_entries}")
        print(f"  Input bones (drivers): {n_input}")
        print(f"  Output bones (corrective): {n_output}")

        # Resolved input bone names
        resolved_in = [b for b in rbf['input_bones'] if b['name']]
        if resolved_in:
            print(f"\n  Input bones ({len(resolved_in)} resolved):")
            for i, b in enumerate(rbf['input_bones']):
                label = b['name'] if b['name'] else f"0x{b['hash']:08X}"
                print(f"    [{i:2d}] {label}")

        # Section summaries
        for si, sec in enumerate(rbf['sections']):
            entries = sec['entries']
            if not entries:
                continue
            # Unique input bone_refs
            unique_refs = sorted(set(e['bone_ref'] for e in entries))
            # Unique output indices
            all_out = set()
            for e in entries:
                all_out.update(e['output_indices'])
            unique_out = sorted(all_out)

            print(f"\n  Section {si}: {len(entries)} entries, solver_dim={sec['solver_dim']}")
            print(f"    Input bone_refs: {unique_refs}")
            # Show output bone names
            out_names = []
            for idx in unique_out:
                if idx < n_output:
                    b = rbf['output_bones'][idx]
                    out_names.append(b['name'] if b['name'] else f"0x{b['hash']:08X}")
                else:
                    out_names.append(f"idx_{idx}")
            print(f"    Output bones ({len(unique_out)}): {out_names}")

        print(f"{'='*60}\n")

        # --- Apply to armature if available, else create summary empty ---
        rbf_name = os.path.splitext(os.path.basename(self.filepath))[0]

        # Collect resolved name sets for quick lookup
        input_name_set = {b['name'] for b in rbf['input_bones'] if b['name']}
        output_name_set = {b['name'] for b in rbf['output_bones'] if b['name']}

        # Build per-input-bone summary: which sections/components/outputs it drives
        input_bone_info = {}  # name -> {sections, components, outputs}
        for si, sec in enumerate(rbf['sections']):
            for e in sec['entries']:
                # Resolve bone_ref to a name via the entry's component cross-ref
                # bone_ref is an index into the input_bones table
                br = e['bone_ref']
                if br < n_input and rbf['input_bones'][br]['name']:
                    bname = rbf['input_bones'][br]['name']
                else:
                    bname = f"input_{br}"
                if bname not in input_bone_info:
                    input_bone_info[bname] = {'sections': set(), 'components': set(), 'outputs': set()}
                input_bone_info[bname]['sections'].add(si)
                input_bone_info[bname]['components'].add(RBF_QUAT_COMP.get(e['component'], str(e['component'])))
                for idx in e['output_indices']:
                    if idx < n_output and rbf['output_bones'][idx]['name']:
                        input_bone_info[bname]['outputs'].add(rbf['output_bones'][idx]['name'])

        # Build per-output-bone summary: which input bones drive it
        output_bone_info = {}  # name -> {drivers}
        for si, sec in enumerate(rbf['sections']):
            for e in sec['entries']:
                br = e['bone_ref']
                if br < n_input and rbf['input_bones'][br]['name']:
                    driver_name = rbf['input_bones'][br]['name']
                else:
                    driver_name = f"input_{br}"
                for idx in e['output_indices']:
                    if idx < n_output and rbf['output_bones'][idx]['name']:
                        oname = rbf['output_bones'][idx]['name']
                    else:
                        oname = f"output_{idx}"
                    if oname not in output_bone_info:
                        output_bone_info[oname] = {'drivers': set()}
                    output_bone_info[oname]['drivers'].add(driver_name)

        # Find the target armature
        armature_obj = None
        for obj in bpy.data.objects:
            if obj.type == 'ARMATURE' and obj.data:
                armature_obj = obj
                break

        applied_count = 0
        if armature_obj and (input_name_set or output_name_set):
            armature = armature_obj.data
            bone_names_in_armature = {b.name for b in armature.bones}

            # --- Create bone collections for RBF roles (Blender 4.0+) ---
            try:
                # Input (driver) bones collection
                rbf_in_coll = None
                rbf_out_coll = None
                coll_name_in = "RBF Inputs"
                coll_name_out = "RBF Outputs"

                # Remove existing RBF collections to avoid duplicates on re-import
                for cname in (coll_name_in, coll_name_out):
                    existing = armature.collections.get(cname)
                    if existing:
                        armature.collections.remove(existing)

                rbf_in_coll = armature.collections.new(coll_name_in)
                rbf_out_coll = armature.collections.new(coll_name_out)

                # Assign bones to collections
                for bname in input_name_set & bone_names_in_armature:
                    bone = armature.bones.get(bname)
                    if bone:
                        rbf_in_coll.assign(bone)
                        applied_count += 1

                for bname in output_name_set & bone_names_in_armature:
                    bone = armature.bones.get(bname)
                    if bone:
                        rbf_out_coll.assign(bone)
                        applied_count += 1

                print(f"RBF: created bone collections '{coll_name_in}' "
                      f"({len(input_name_set & bone_names_in_armature)} bones) and "
                      f"'{coll_name_out}' ({len(output_name_set & bone_names_in_armature)} bones)")
            except (AttributeError, TypeError) as e:
                print(f"RBF: bone collections not available ({e}), skipping")

            # --- Tag bones with custom properties ---
            # We need to use pose bones for custom properties (armature.bones are read-only outside edit mode)
            pose = armature_obj.pose
            for bname in input_name_set & bone_names_in_armature:
                pbone = pose.bones.get(bname)
                if pbone and bname in input_bone_info:
                    info = input_bone_info[bname]
                    pbone["rbf_role"] = "input"
                    pbone["rbf_components"] = ",".join(sorted(info['components']))
                    pbone["rbf_output_count"] = len(info['outputs'])
                    # Store the names of output bones this input drives
                    driven = sorted(info['outputs'])
                    if len(driven) <= 20:
                        pbone["rbf_drives"] = ",".join(driven)
                    else:
                        pbone["rbf_drives"] = ",".join(driven[:20]) + f"...+{len(driven)-20}"

            for bname in output_name_set & bone_names_in_armature:
                pbone = pose.bones.get(bname)
                if pbone and bname in output_bone_info:
                    info = output_bone_info[bname]
                    pbone["rbf_role"] = "output"
                    pbone["rbf_driver_count"] = len(info['drivers'])
                    drivers = sorted(info['drivers'])
                    if len(drivers) <= 10:
                        pbone["rbf_driven_by"] = ",".join(drivers)
                    else:
                        pbone["rbf_driven_by"] = ",".join(drivers[:10]) + f"...+{len(drivers)-10}"

            print(f"RBF: tagged {len(input_name_set & bone_names_in_armature)} input and "
                  f"{len(output_name_set & bone_names_in_armature)} output pose bones with custom properties")

            # Store file-level metadata on the armature object itself
            armature_obj["rbf_file"] = os.path.basename(self.filepath)
            armature_obj["rbf_version"] = rbf['version']
            armature_obj["rbf_section_count"] = n_sections
            armature_obj["rbf_total_entries"] = total_entries
        else:
            if not armature_obj:
                print("RBF: no armature in scene — import a .binskeleton first to apply RBF data to bones")
            elif not (input_name_set or output_name_set):
                print("RBF: no bone names resolved — cannot apply to armature")

        # --- Always create summary empty with full solver data ---
        rbf_empty = bpy.data.objects.new(rbf_name + "_rbf", None)
        rbf_empty.empty_display_type = 'PLAIN_AXES'
        rbf_empty.empty_display_size = 0.1
        bpy.context.collection.objects.link(rbf_empty)

        # Parent to armature if available
        if armature_obj:
            rbf_empty.parent = armature_obj

        # Store metadata as custom properties
        rbf_empty["rbf_version"] = rbf['version']
        rbf_empty["rbf_input_bone_count"] = n_input
        rbf_empty["rbf_output_bone_count"] = n_output
        rbf_empty["rbf_section_count"] = n_sections

        # Store bone name lists
        input_names = [b['name'] if b['name'] else f"0x{b['hash']:08X}" for b in rbf['input_bones']]
        output_names = [b['name'] if b['name'] else f"0x{b['hash']:08X}" for b in rbf['output_bones']]
        rbf_empty["rbf_input_bones"] = json.dumps(input_names)
        rbf_empty["rbf_output_bones"] = json.dumps(output_names)

        # Store per-section data
        for si, sec in enumerate(rbf['sections']):
            prefix = f"rbf_s{si}"
            rbf_empty[f"{prefix}_entry_count"] = len(sec['entries'])
            rbf_empty[f"{prefix}_solver_dim"] = sec['solver_dim']

            # Compact entry data: parallel arrays for each field
            rbf_empty[f"{prefix}_bone_ref"] = [e['bone_ref'] for e in sec['entries']]
            rbf_empty[f"{prefix}_component"] = [e['component'] for e in sec['entries']]
            rbf_empty[f"{prefix}_num_inputs"] = [e['num_inputs'] for e in sec['entries']]
            rbf_empty[f"{prefix}_output_count"] = [e['output_count'] for e in sec['entries']]
            rbf_empty[f"{prefix}_param"] = [e['param'] for e in sec['entries']]

            # Flatten quaternions and positions into float arrays
            quats = []
            positions = []
            out_idx = []
            for e in sec['entries']:
                quats.extend(e['input_quat'])
                positions.extend(e['input_pos'])
                out_idx.extend(e['output_indices'])
            rbf_empty[f"{prefix}_input_quats"] = quats
            rbf_empty[f"{prefix}_input_positions"] = positions
            rbf_empty[f"{prefix}_output_indices"] = out_idx

        status = f"Imported RBF: {n_sections} sections, {total_entries} entries, " \
                 f"{n_input} input bones, {n_output} output bones"
        if applied_count > 0:
            status += f", applied to {applied_count} armature bones"
        self.report({'INFO'}, status)
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
                elif UniformType == INTEGER:
                    # Integer uniform (e.g. g_iTextureVariationCount); 4-byte int32 payload
                    struct.unpack("i", file.read(4))
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

        # Second Material Map (SecondaryMaterialMap)
        # Used by Group1 (shadow mesh) for material assignments.
        # Needed for alpha-tested shadow passes (e.g. foliage opacity masking).
        (count, ) = struct.unpack('I', file.read(4))
        MaterialMaps.append(struct.unpack(
            str(count) + 'I', file.read(count*4)))

        # Read Meshes
        # Two groups of primitives (sub-meshes) are stored:
        #   Group0 = Visual mesh: draw calls for the color/shading pass.
        #            Uses the PrimaryMaterialMap (MaterialMaps[0]).
        #   Group1 = Shadow mesh: draw calls for the shadow map / depth-only pass.
        #            Uses the SecondaryMaterialMap (MaterialMaps[1]).
        #
        # Group1 is a shadow rendering optimization. It typically merges
        # multiple Group0 submeshes into fewer draw calls (shadow pass only
        # needs depth, not per-material shading) and drops submeshes that
        # don't cast shadows (glass, decals, FX/particles). Both groups
        # reference the same vertex and index buffers.
        # If Group1 has zero meshes, the object does not cast shadows.
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
                SemanticCount = { POSITION: 0, NORMAL: 0, TEXCOORD: 0, TANGENT: 0, VERTEX_COLOR: 0, BONE_INDEX: 0, BONE_WEIGHT: 0 }
                for j in range(VertexAttribCount):
                    (BufferIndex, Type, Semantic, Zero) = struct.unpack(
                        '4B', file.read(4))
                    # The attribute descriptors use the RENDERER stream index:
                    #   buf 0 = position + skinning data (renderer stream 0)
                    #   buf 1 = shading attributes: tangent, normal, texcoord (renderer stream 1)
                    # But in the file, the physical buffer order is reversed:
                    #   VB0 (first in file)  = shading attributes
                    #   VB1 (second in file) = position + skinning
                    # So we swap the index to align with the physical file buffer order
                    # (VertexBuffers[0] = VB0 = shading, VertexBuffers[1] = VB1 = position).
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
                    ], "Tangents": [], "Colors": [], "Indices": [], "Weights": []}
                    for j in range(SemanticCount[NORMAL]):
                        MeshData["Normals"].append([])
                    for j in range(SemanticCount[TEXCOORD]):
                        MeshData["UVs"].append([])
                    for j in range(SemanticCount[TANGENT]):
                        MeshData["Tangents"].append([])
                    for j in range(SemanticCount[VERTEX_COLOR]):
                        MeshData["Colors"].append([])
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

                                elif attrib["Semantic"] == VERTEX_COLOR:
                                    # Per-vertex color, stored as BYTE4_SNORM
                                    assert attrib["Type"] == BYTE4_SNORM
                                    MeshData["Colors"][attrib["SemanticIndex"]].append(
                                        (vertex[attrib["Index"]]/127.0, vertex[attrib["Index"] + 1]/127.0, vertex[attrib["Index"] + 2]/127.0, vertex[attrib["Index"] + 3]/127.0))

                                elif attrib["Semantic"] == BONE_INDEX:
                                    # We're only supporting SHORT4_UINT, BYTE4_UINT or BYTE4_UNORM indices for now
                                    assert attrib["Type"] == SHORT4_UINT or attrib["Type"] == BYTE4_UINT or attrib["Type"] == BYTE4_UNORM
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
                Colors = []
                Indices = []
                Weights = []
                VertexMap = {}
                for j in range(SemanticCount[NORMAL]):
                    Normals.append([])
                for j in range(SemanticCount[TEXCOORD]):
                    UVs.append([])
                for j in range(SemanticCount[TANGENT]):
                    Tangents.append([])
                for j in range(SemanticCount[VERTEX_COLOR]):
                    Colors.append([])
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
                            for j in range(SemanticCount[VERTEX_COLOR]):
                                Colors[j].append(
                                    MeshData["Colors"][j][index])
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

                for j in range(SemanticCount[VERTEX_COLOR]):
                    color_layer = mesh_data.color_attributes.new(
                        name="Color" + str(j), type='FLOAT_COLOR', domain='POINT')
                    for k in range(len(Colors[j])):
                        color_layer.data[k].color = Colors[j][k]

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
