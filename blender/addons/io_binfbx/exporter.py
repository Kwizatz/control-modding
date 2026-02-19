# Copyright (C) 2023,2026 Rodrigo Jose Hernandez Cordoba
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
import re
import struct
import json
import math
import mathutils

# ── Constants (mirrored from importer.py) ────────────────────────────────────

FBXMAGICK = 0x2e

FLOAT = 0x00
RANGE = 0x01
VECTOR = 0x02
COLOR = 0x03
TEXTURESAMPLER = 0x08
TEXTUREMAP = 0x09
BOOLEAN = 0x0C
INTEGER = 0x10

POSITION = 0x0
NORMAL = 0x1
TEXCOORD = 0x2
TANGENT = 0x3
VERTEX_COLOR = 0x4
BONE_INDEX = 0x5
BONE_WEIGHT = 0x6

FLOAT3 = 2
BYTE4_SNORM = 4
BYTE4_UNORM = 5
SHORT2_SNORM = 7
SHORT4_SNORM = 8
SHORT4_UINT = 13
BYTE4_UINT = 15

# struct format strings per vertex format type
FormatStruct = {
    FLOAT3: '3f',
    BYTE4_SNORM: '4b',
    BYTE4_UNORM: '4B',
    SHORT2_SNORM: '2h',
    SHORT4_SNORM: '4h',
    SHORT4_UINT: '4H',
    BYTE4_UINT: '4B',
}

# byte sizes per vertex format type
FormatSize = {
    FLOAT3: 12,
    BYTE4_SNORM: 4,
    BYTE4_UNORM: 4,
    SHORT2_SNORM: 4,
    SHORT4_SNORM: 8,
    SHORT4_UINT: 8,
    BYTE4_UINT: 4,
}

right_hand_matrix = mathutils.Matrix(
    ((-1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))

# Inverse of right_hand_matrix (used to convert Blender coords back to game coords).
# For this specific matrix, inverse == transpose == self.
inv_right_hand_matrix = right_hand_matrix.inverted()

# Regex to parse mesh object names: Group{g}-LOD{l}-Mesh{m}
MESH_NAME_RE = re.compile(r'^Group(\d+)-LOD(\d+)-Mesh(\d+)$')


# ── Helper: clamp ────────────────────────────────────────────────────────────

def clamp(value, lo, hi):
    return max(lo, min(hi, value))


# ── Scene Interrogation ──────────────────────────────────────────────────────

def find_meta_empty():
    """Find the _binfbx_meta empty object in the scene."""
    for obj in bpy.data.objects:
        if obj.name == "_binfbx_meta" and obj.type == 'EMPTY':
            return obj
    return None


def find_armature():
    """Find the first armature object in the scene."""
    for obj in bpy.data.objects:
        if obj.type == 'ARMATURE' and obj.data:
            return obj
    return None


def collect_meshes_from_scene():
    """Walk the scene collection hierarchy and collect mesh objects.

    Returns a dict:  {group_index: {lod_index: [(mesh_index, mesh_object), ...]}}
    Meshes within each LOD are sorted by mesh_index.
    """
    result = {}
    for collection in bpy.context.scene.collection.children:
        # Match top-level Group collections: "Group0", "Group1"
        if not collection.name.startswith("Group"):
            continue
        try:
            group_idx = int(collection.name[len("Group"):])
        except ValueError:
            continue
        result[group_idx] = {}
        for lod_collection in collection.children:
            for obj in lod_collection.objects:
                if obj.type != 'MESH':
                    continue
                match = MESH_NAME_RE.match(obj.name)
                if not match:
                    print(f"Warning: skipping mesh '{obj.name}' — name does not match Group{{g}}-LOD{{l}}-Mesh{{m}}")
                    continue
                g, l, m = int(match.group(1)), int(match.group(2)), int(match.group(3))
                if g != group_idx:
                    print(f"Warning: mesh '{obj.name}' is in Group{group_idx} collection but name says Group{g}")
                    continue
                if l not in result[g]:
                    result[g][l] = []
                result[g][l].append((m, obj))
        # Sort each LOD's meshes by mesh_index
        for lod in result[group_idx]:
            result[group_idx][lod].sort(key=lambda x: x[0])
    return result


# ── Joint Palette ────────────────────────────────────────────────────────────

def build_joint_palette(armature_obj):
    """Build the joint palette from an armature.

    Returns a list of (name, float12_matrix, float3_envelope, radius, parent_index).
    The matrix is the inverted bind-pose transform in game coordinate space
    (inverse of what the importer produces).

    The importer builds:
        transform = right_hand @ (Scale(-1) @ rotation @ translation) @ right_hand
    So we invert:
        game_matrix = inv_rh @ bone.matrix @ inv_rh
    then extract Scale(-1) to get rot and trans:
        inner = Scale(-1) @ game_matrix
        matrix[0..8] = rot (3x3), matrix[9..11] = translation col
    """
    if armature_obj is None:
        return []

    armature = armature_obj.data
    joints = []
    bone_name_to_index = {}

    # We need edit-mode data for matrix, but we can only access rest-pose
    # transforms from armature.bones (pose-independent).
    for idx, bone in enumerate(armature.bones):
        bone_name_to_index[bone.name] = idx

    for idx, bone in enumerate(armature.bones):
        name = bone.name
        parent_idx = bone_name_to_index[bone.parent.name] if bone.parent else -1

        # Reverse the importer's coordinate transform
        # Importer: transform = rh @ (Scale(-1) @ rot @ trans) @ rh
        # So: Scale(-1) @ rot @ trans = inv_rh @ bone.matrix @ inv_rh
        game_matrix = inv_right_hand_matrix @ bone.matrix_local @ inv_right_hand_matrix
        inner = mathutils.Matrix.Scale(-1.0, 4) @ game_matrix

        # Extract 3x3 rotation into row-major float[9] and translation into float[3]
        mat12 = [
            inner[0][0], inner[0][1], inner[0][2],
            inner[1][0], inner[1][1], inner[1][2],
            inner[2][0], inner[2][1], inner[2][2],
            inner[0][3], inner[1][3], inner[2][3],
        ]

        # Envelope (tail in game coords) and radius
        tail = inv_right_hand_matrix @ bone.tail_local
        envelope = [tail.x, tail.y, tail.z]
        radius = bone.head_radius if bone.head_radius > 0 else 0.0

        joints.append((name, mat12, envelope, radius, parent_idx))

    return joints


# ── Vertex Buffer Encoding ───────────────────────────────────────────────────

def get_attrib_infos_for_mesh(mesh_obj):
    """Get the attribute info descriptors for a mesh.

    Returns a list of [BufferIndex, Type, Semantic, Zero] in the ORIGINAL
    file format (renderer stream indices, NOT swapped).

    If the mesh has stored binfbx_attrib_infos from import, use those.
    Otherwise, infer a standard layout based on whether the mesh has bone weights.
    """
    stored = mesh_obj.get("binfbx_attrib_infos")
    if stored is not None:
        return json.loads(stored)

    # Infer standard layout
    has_weights = len(mesh_obj.vertex_groups) > 0
    has_colors = len(mesh_obj.data.color_attributes) > 0

    # Renderer convention: buf 0 = position+skinning, buf 1 = shading
    attribs = []
    # Position (always in renderer buf 0 = file VB1)
    attribs.append([0, FLOAT3, POSITION, 0])
    # Normal (renderer buf 1 = file VB0)
    attribs.append([1, SHORT4_SNORM, NORMAL, 0])
    # TexCoord (renderer buf 1 = file VB0)
    attribs.append([1, SHORT2_SNORM, TEXCOORD, 0])
    # Tangent (renderer buf 1 = file VB0)
    attribs.append([1, BYTE4_SNORM, TANGENT, 0])
    if has_colors:
        attribs.append([1, BYTE4_SNORM, VERTEX_COLOR, 0])
    if has_weights:
        attribs.append([0, SHORT4_UINT, BONE_INDEX, 0])
        attribs.append([0, BYTE4_UNORM, BONE_WEIGHT, 0])

    return attribs


def encode_mesh_buffers(mesh_obj, attrib_infos, joint_names):
    """Encode a Blender mesh's geometry into packed binary vertex/index buffers.

    Parameters:
        mesh_obj:     Blender mesh object
        attrib_infos: list of [BufferIndex, Type, Semantic, Zero] in file format
                      (renderer stream indices, NOT swapped)
        joint_names:  ordered list of joint names from the skeleton palette

    Returns:
        (vb0_bytes, vb1_bytes, ib_bytes, vertex_count, triangle_count, index_size)

    The attribute infos use renderer convention:
        buf 0 = position + skinning (written to file VB1)
        buf 1 = shading (written to file VB0)
    """
    mesh = mesh_obj.data

    # Ensure we have loop triangles
    mesh.calc_loop_triangles()
    if not mesh.loop_triangles:
        return b'', b'', b'', 0, 0, 2

    # Calc tangents if we have UV layers (required for tangent export)
    if mesh.uv_layers:
        mesh.calc_tangents()

    # Build bone name→index lookup
    joint_name_to_idx = {name: idx for idx, name in enumerate(joint_names)}

    # Determine which attributes go to which buffer
    # Renderer buf 0 → file VB1 (position/skinning), Renderer buf 1 → file VB0 (shading)
    buf0_attribs = []  # renderer stream 0 → file VB1
    buf1_attribs = []  # renderer stream 1 → file VB0
    for info in attrib_infos:
        buf_idx, fmt_type, semantic, zero = info
        if buf_idx == 0:
            buf0_attribs.append((fmt_type, semantic))
        else:
            buf1_attribs.append((fmt_type, semantic))

    # Compute strides
    stride_renderer0 = sum(FormatSize[a[0]] for a in buf0_attribs)  # file VB1
    stride_renderer1 = sum(FormatSize[a[0]] for a in buf1_attribs)  # file VB0

    # Build unique vertex list from loop triangles
    # A "vertex" is a unique combination of position + all per-loop attributes
    vertex_data = []  # list of dicts with all attribute data per unique vertex
    vertex_map = {}   # (loop_vert_idx, loop_idx_tuple_hash) → vertex_data index
    triangles = []    # list of (i0, i1, i2) indices into vertex_data

    # Prepare vertex group data — for each vertex, collect (bone_idx, weight) tuples
    # across all vertex groups
    vgroup_data = {}  # vertex_index → list of (joint_palette_idx, weight)
    if joint_names:
        for vgroup in mesh_obj.vertex_groups:
            jidx = joint_name_to_idx.get(vgroup.name, -1)
            if jidx < 0:
                continue
            for vert in mesh.vertices:
                try:
                    w = vgroup.weight(vert.index)
                    if w > 0:
                        if vert.index not in vgroup_data:
                            vgroup_data[vert.index] = []
                        vgroup_data[vert.index].append((jidx, w))
                except RuntimeError:
                    pass

    for tri in mesh.loop_triangles:
        face_indices = []
        for loop_idx in tri.loops:
            loop = mesh.loops[loop_idx]
            vert_idx = loop.vertex_index
            vert = mesh.vertices[vert_idx]

            # Build a hashable key from loop data to deduplicate
            # (vertex position is per-vertex, but normal/UV/tangent are per-loop)
            pos = tuple(vert.co)
            normal = tuple(loop.normal)
            uv = tuple(mesh.uv_layers[0].data[loop_idx].uv) if mesh.uv_layers else (0.0, 0.0)
            tangent = (tuple(loop.tangent), loop.bitangent_sign) if mesh.uv_layers else ((0, 0, 0), 1.0)

            key = (vert_idx, normal, uv, tangent)

            if key in vertex_map:
                face_indices.append(vertex_map[key])
            else:
                new_idx = len(vertex_data)
                vertex_map[key] = new_idx

                # Collect all data for this vertex
                vdata = {
                    'position': pos,
                    'normal': normal,
                    'uv': uv,
                    'tangent': tangent,
                    'vert_idx': vert_idx,
                }

                # Vertex colors
                if mesh.color_attributes:
                    vdata['color'] = tuple(mesh.color_attributes[0].data[vert_idx].color)
                else:
                    vdata['color'] = (0, 0, 0, 0)

                # Bone weights — sorted by weight descending, take top 4
                if joint_names and vert_idx in vgroup_data:
                    weights = sorted(vgroup_data[vert_idx], key=lambda x: x[1], reverse=True)[:4]
                    # Pad to 4
                    while len(weights) < 4:
                        weights.append((0, 0.0))
                    # Normalize weights to sum to 255
                    total_w = sum(w for _, w in weights)
                    if total_w > 0:
                        bone_indices = tuple(w[0] for w in weights)
                        raw_weights = [w[1] / total_w * 255.0 for w in weights]
                        # Round and ensure sum is exactly 255
                        int_weights = [int(round(w)) for w in raw_weights]
                        diff = 255 - sum(int_weights)
                        if diff != 0:
                            int_weights[0] += diff
                        bone_weights = tuple(clamp(w, 0, 255) for w in int_weights)
                    else:
                        bone_indices = (0, 0, 0, 0)
                        bone_weights = (0, 0, 0, 0)
                else:
                    bone_indices = (0, 0, 0, 0)
                    bone_weights = (0, 0, 0, 0)

                vdata['bone_indices'] = bone_indices
                vdata['bone_weights'] = bone_weights

                vertex_data.append(vdata)
                face_indices.append(new_idx)

        # Reverse winding order (importer uses face.insert(0, ...) which reverses)
        triangles.append((face_indices[2], face_indices[1], face_indices[0]))

    vertex_count = len(vertex_data)
    triangle_count = len(triangles)

    # Determine index size
    if vertex_count <= 0xFF:
        index_size = 1
        index_fmt = 'B'
    elif vertex_count <= 0xFFFF:
        index_size = 2
        index_fmt = 'H'
    else:
        index_size = 4
        index_fmt = 'I'

    # Pack index buffer
    ib_parts = []
    for tri in triangles:
        ib_parts.append(struct.pack(f'3{index_fmt}', *tri))
    ib_bytes = b''.join(ib_parts)

    # Pack vertex buffers
    # Renderer buf 1 → file VB0 (shading: normal, texcoord, tangent, vertex color)
    # Renderer buf 0 → file VB1 (position, bone index, bone weight)
    vb0_parts = []  # file VB0 = renderer buf 1 (shading)
    vb1_parts = []  # file VB1 = renderer buf 0 (position/skinning)

    for vd in vertex_data:
        # Transform position back to game coords
        pos = inv_right_hand_matrix @ mathutils.Vector(vd['position'])
        # Transform normal back to game coords
        nrm = inv_right_hand_matrix @ mathutils.Vector(vd['normal'])

        # Pack renderer buf 0 attributes → file VB1
        for fmt_type, semantic in buf0_attribs:
            if semantic == POSITION:
                vb1_parts.append(struct.pack('3f', pos.x, pos.y, pos.z))
            elif semantic == BONE_INDEX:
                bi = vd['bone_indices']
                if fmt_type == SHORT4_UINT:
                    vb1_parts.append(struct.pack('4H', *bi))
                elif fmt_type == BYTE4_UINT:
                    vb1_parts.append(struct.pack('4B', *bi))
                elif fmt_type == BYTE4_UNORM:
                    vb1_parts.append(struct.pack('4B', *bi))
                else:
                    vb1_parts.append(struct.pack('4H', *bi))
            elif semantic == BONE_WEIGHT:
                bw = vd['bone_weights']
                vb1_parts.append(struct.pack('4B', *bw))

        # Pack renderer buf 1 attributes → file VB0
        for fmt_type, semantic in buf1_attribs:
            if semantic == NORMAL:
                nx = clamp(int(round(nrm.x * 32767)), -32768, 32767)
                ny = clamp(int(round(nrm.y * 32767)), -32768, 32767)
                nz = clamp(int(round(nrm.z * 32767)), -32768, 32767)
                nw = 0  # 4th component, typically 0 for normals
                vb0_parts.append(struct.pack('4h', nx, ny, nz, nw))
            elif semantic == TEXCOORD:
                u, v = vd['uv']
                # Importer: u = raw/4095, v = 1 - raw/4095
                # So: raw_u = u*4095, raw_v = (1-v)*4095
                su = clamp(int(round(u * 4095)), -32768, 32767)
                sv = clamp(int(round((1.0 - v) * 4095)), -32768, 32767)
                vb0_parts.append(struct.pack('2h', su, sv))
            elif semantic == TANGENT:
                tang_vec, bitangent_sign = vd['tangent']
                tx = clamp(int(round(tang_vec[0] * 127)), -128, 127)
                ty = clamp(int(round(tang_vec[1] * 127)), -128, 127)
                tz = clamp(int(round(tang_vec[2] * 127)), -128, 127)
                tw = clamp(int(round(bitangent_sign * 127)), -128, 127)
                vb0_parts.append(struct.pack('4b', tx, ty, tz, tw))
            elif semantic == VERTEX_COLOR:
                c = vd['color']
                cx = clamp(int(round(c[0] * 127)), -128, 127)
                cy = clamp(int(round(c[1] * 127)), -128, 127)
                cz = clamp(int(round(c[2] * 127)), -128, 127)
                cw = clamp(int(round(c[3] * 127)), -128, 127)
                vb0_parts.append(struct.pack('4b', cx, cy, cz, cw))

    vb0_bytes = b''.join(vb0_parts)
    vb1_bytes = b''.join(vb1_parts)

    return vb0_bytes, vb1_bytes, ib_bytes, vertex_count, triangle_count, index_size


# ── Global Buffer Assembly ───────────────────────────────────────────────────

def build_global_vertex_buffers(mesh_buffers_list):
    """Concatenate per-mesh buffers into global buffers and compute offsets.

    Parameters:
        mesh_buffers_list: list of (vb0, vb1, ib, vert_count, tri_count, index_size)
                           in flat order across all groups/LODs

    Returns:
        (global_vb0, global_vb1, global_ib, index_size,
         per_mesh_info_list)
    where per_mesh_info_list is a list of dicts:
        { 'vb0_offset': int, 'vb1_offset': int, 'ib_offset': int,
          'vertex_count': int, 'triangle_count': int }
    """
    global_vb0 = bytearray()
    global_vb1 = bytearray()
    global_ib = bytearray()
    info_list = []

    # Use the largest index_size across all meshes
    index_size = 2
    for vb0, vb1, ib, vc, tc, isz in mesh_buffers_list:
        if isz > index_size:
            index_size = isz

    # Index format for repacking if needed
    ib_offset_count = 0  # offset in INDEX units (not bytes)

    for vb0, vb1, ib, vc, tc, isz in mesh_buffers_list:
        vb0_offset = len(global_vb0)
        vb1_offset = len(global_vb1)
        ib_offset = ib_offset_count

        global_vb0.extend(vb0)
        global_vb1.extend(vb1)

        # Repack index buffer if the per-mesh index size differs from global
        if isz == index_size:
            global_ib.extend(ib)
        else:
            # Unpack with original size, repack with global size
            src_fmt = {1: 'B', 2: 'H', 4: 'I'}[isz]
            dst_fmt = {1: 'B', 2: 'H', 4: 'I'}[index_size]
            n_indices = tc * 3
            indices = struct.unpack(f'{n_indices}{src_fmt}', ib)
            global_ib.extend(struct.pack(f'{n_indices}{dst_fmt}', *indices))

        ib_offset_count += tc * 3

        info_list.append({
            'vb0_offset': vb0_offset,
            'vb1_offset': vb1_offset,
            'ib_offset': ib_offset,
            'vertex_count': vc,
            'triangle_count': tc,
        })

    return bytes(global_vb0), bytes(global_vb1), bytes(global_ib), index_size, info_list


# ── Metadata Collection (from stored custom properties) ──────────────────────

def collect_global_params(meta_empty):
    """Read global parameters from the _binfbx_meta empty.

    Returns a dict with all global param fields.
    If meta_empty is None, returns defaults (zeros).
    """
    defaults = {
        'index_size': 2,
        'reserved': [0, 0],
        'global_scale': 1.0,
        'lod_thresholds': [],
        'mirror_sign': 1.0,
        'aabb_center': [0.0, 0.0, 0.0],
        'sphere_radius': 0.0,
        'aabb_min': [0.0, 0.0, 0.0],
        'aabb_max': [0.0, 0.0, 0.0],
        'global_lod_count': 0,
    }
    if meta_empty is None:
        return defaults

    return {
        'index_size': meta_empty.get("binfbx_index_size", defaults['index_size']),
        'reserved': json.loads(meta_empty.get("binfbx_reserved", "null")) or defaults['reserved'],
        'global_scale': meta_empty.get("binfbx_global_scale", defaults['global_scale']),
        'lod_thresholds': json.loads(meta_empty.get("binfbx_lod_thresholds", "null")) or defaults['lod_thresholds'],
        'mirror_sign': meta_empty.get("binfbx_mirror_sign", defaults['mirror_sign']),
        'aabb_center': json.loads(meta_empty.get("binfbx_aabb_center", "null")) or defaults['aabb_center'],
        'sphere_radius': meta_empty.get("binfbx_sphere_radius", defaults['sphere_radius']),
        'aabb_min': json.loads(meta_empty.get("binfbx_aabb_min", "null")) or defaults['aabb_min'],
        'aabb_max': json.loads(meta_empty.get("binfbx_aabb_max", "null")) or defaults['aabb_max'],
        'global_lod_count': meta_empty.get("binfbx_global_lod_count", defaults['global_lod_count']),
    }


def collect_materials():
    """Collect material data from Blender materials that have binfbx metadata.

    Returns a list of material dicts in the order they appear in bpy.data.materials,
    filtered to only those with binfbx_material_id set.  Also returns a name→index
    mapping for building material maps.
    """
    materials = []
    name_to_index = {}
    for mat in bpy.data.materials:
        if mat.get("binfbx_material_id") is None:
            continue
        idx = len(materials)
        name_to_index[mat.name] = idx
        materials.append({
            'name': mat.name,
            'material_id': json.loads(mat["binfbx_material_id"]),
            'type': mat.get("binfbx_material_type", ""),
            'path': mat.get("binfbx_material_path", ""),
            'params': json.loads(mat.get("binfbx_material_params", "[0,0,0,0,0,0]")),
            'uniforms': json.loads(mat.get("binfbx_uniforms", "[]")),
        })
    return materials, name_to_index


def collect_material_maps(meta_empty):
    """Read material maps from the _binfbx_meta empty.

    Returns (primary_map, alternate_maps, secondary_map).
    """
    if meta_empty is None:
        return [], [], []

    primary = json.loads(meta_empty.get("binfbx_primary_material_map", "[]"))
    secondary = json.loads(meta_empty.get("binfbx_secondary_material_map", "[]"))
    alternates = json.loads(meta_empty.get("binfbx_alternate_material_maps", "[]"))

    return primary, alternates, secondary


def collect_per_mesh_metadata(mesh_obj):
    """Read per-mesh metadata from a mesh object's custom properties.

    Returns a dict of flags/joint/unknown fields with defaults if not present.
    """
    return {
        'flags0': mesh_obj.get("binfbx_flags0", 0),
        'flags1': mesh_obj.get("binfbx_flags1", 0),
        'joint': mesh_obj.get("binfbx_joint", 0),
        'unknown3': mesh_obj.get("binfbx_unknown3", 1.0),
        'is_rigid': mesh_obj.get("binfbx_is_rigid", 0),
        'unknown5': mesh_obj.get("binfbx_unknown5", 0.0),
    }


# ── Bounding Volume Computation (STUB — returns zeros) ──────────────────────

def compute_mesh_bounds(mesh_obj):
    """Compute bounding sphere and box for a mesh in game coordinates.

    Returns (bounding_sphere, bounding_box) where:
        bounding_sphere = (cx, cy, cz, radius)
        bounding_box = (minx, miny, minz, maxx, maxy, maxz)

    STUB: returns zeros.  Will be implemented with actual computation later.
    """
    # TODO: Implement actual bounding volume computation
    return (0.0, 0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)


# ── Trailer / CDF Computation (STUB — returns zeros) ────────────────────────

def compute_trailer(group0_lod0_mesh_objects):
    """Compute the trailing block (triangle area CDF) for Group0 LOD0 meshes.

    Returns (reserved, total_surface_area, cdf_array).

    STUB: returns zeros.  Will be implemented with actual cross-product area
    computation later.
    """
    # TODO: Implement triangle area CDF computation
    return 0, 0.0, []


# ── Binary Serialization ────────────────────────────────────────────────────

def write_string(out, s):
    """Write a length-prefixed string (uint32 length + raw bytes, no null terminator)."""
    data = s.encode('utf-8')
    out.write(struct.pack('I', len(data)))
    out.write(data)


def write_joint(out, joint):
    """Write a single joint entry."""
    name, mat12, envelope, radius, parent = joint
    write_string(out, name)
    out.write(struct.pack('12f', *mat12))
    out.write(struct.pack('3f', *envelope))
    out.write(struct.pack('f', radius))
    out.write(struct.pack('i', parent))


def write_material(out, mat_data):
    """Write a single material entry."""
    # Magick = 7
    out.write(struct.pack('I', 7))
    # Material ID (8 bytes)
    out.write(struct.pack('8B', *mat_data['material_id']))
    # Name, Type, Path
    write_string(out, mat_data['name'])
    write_string(out, mat_data['type'])
    write_string(out, mat_data['path'])
    # MaterialParams (6 × uint32)
    out.write(struct.pack('6I', *mat_data['params']))
    # Uniform variables
    uniforms = mat_data['uniforms']
    out.write(struct.pack('I', len(uniforms)))
    for u in uniforms:
        write_string(out, u['name'])
        utype = u['type']
        out.write(struct.pack('I', utype))
        if utype == FLOAT:
            out.write(struct.pack('f', u['value']))
        elif utype == RANGE:
            out.write(struct.pack('2f', *u['value']))
        elif utype == COLOR:
            out.write(struct.pack('4f', *u['value']))
        elif utype == VECTOR:
            out.write(struct.pack('3f', *u['value']))
        elif utype == TEXTUREMAP:
            write_string(out, u['value'])
        elif utype == TEXTURESAMPLER:
            pass  # No data
        elif utype == BOOLEAN:
            out.write(struct.pack('I', u['value']))
        elif utype == INTEGER:
            out.write(struct.pack('i', u['value']))


def write_mesh_descriptor(out, mesh_info, per_mesh_meta, attrib_infos,
                          bounding_sphere, bounding_box):
    """Write a single mesh descriptor (the metadata block, not the vertex data)."""
    info = mesh_info
    meta = per_mesh_meta

    out.write(struct.pack('I', info['lod']))
    out.write(struct.pack('I', info['vertex_count']))
    out.write(struct.pack('I', info['triangle_count']))
    # VertexBufferOffsets[0] and [1] — in file format these are:
    # [0] = VB0 offset (shading), [1] = VB1 offset (position)
    out.write(struct.pack('2I', info['vb0_offset'], info['vb1_offset']))
    out.write(struct.pack('I', info['ib_offset']))

    out.write(struct.pack('i', meta['flags0']))
    out.write(struct.pack('4f', *bounding_sphere))
    out.write(struct.pack('6f', *bounding_box))
    out.write(struct.pack('i', meta['flags1']))

    # Attribute infos — count as uint8
    out.write(struct.pack('B', len(attrib_infos)))
    for ai in attrib_infos:
        out.write(struct.pack('4B', *ai))

    out.write(struct.pack('i', meta['joint']))
    out.write(struct.pack('f', meta['unknown3']))
    out.write(struct.pack('B', meta['is_rigid']))
    out.write(struct.pack('f', meta['unknown5']))


def write_binfbx(filepath, context):
    """Main export entry point.  Serializes the Blender scene to a .binfbx file.

    Follows the exact serialization order from BinFBX::Write() in BinFBX.cpp:
    1. Header (Magick, VBSize0, VBSize1, IndexCount, IndexSize)
    2. VB0 (shading), VB1 (position/skinning)
    3. Index buffer
    4. Joint count + joints
    5. Global params
    6. Material count + materials
    7. Primary material map
    8. Alternate material maps
    9. Secondary material map
    10. Mesh groups (Group0, Group1) — each: count + mesh descriptors
    11. Trailer (reserved, total_area, CDF)
    """
    meta_empty = find_meta_empty()
    armature_obj = find_armature()
    global_params = collect_global_params(meta_empty)
    materials, mat_name_to_idx = collect_materials()
    primary_map, alternate_maps, secondary_map = collect_material_maps(meta_empty)

    # Build joint palette from armature
    joints = build_joint_palette(armature_obj)
    joint_names = [j[0] for j in joints]

    # Collect meshes organized by group/lod/index
    scene_meshes = collect_meshes_from_scene()

    # Build flat ordered mesh list per group and encode buffers
    # group_mesh_data[g] = list of (mesh_obj, attrib_infos, vb0, vb1, ib, vc, tc, isz, per_mesh_meta, lod)
    group_mesh_data = {0: [], 1: []}
    all_mesh_buffers = []  # flat list for global buffer assembly

    for g in sorted(scene_meshes.keys()):
        for l in sorted(scene_meshes[g].keys()):
            for m_idx, mesh_obj in scene_meshes[g][l]:
                attrib_infos = get_attrib_infos_for_mesh(mesh_obj)
                vb0, vb1, ib, vc, tc, isz = encode_mesh_buffers(
                    mesh_obj, attrib_infos, joint_names)
                meta = collect_per_mesh_metadata(mesh_obj)
                group_mesh_data[g].append({
                    'mesh_obj': mesh_obj,
                    'attrib_infos': attrib_infos,
                    'lod': l,
                    'meta': meta,
                    'buf_idx': len(all_mesh_buffers),
                })
                all_mesh_buffers.append((vb0, vb1, ib, vc, tc, isz))

    # Build global buffers
    global_vb0, global_vb1, global_ib, index_size, buf_info_list = \
        build_global_vertex_buffers(all_mesh_buffers)

    # Override index_size from metadata if available and we have no meshes
    if not all_mesh_buffers:
        index_size = global_params['index_size']

    # Compute trailer from Group0 LOD0 meshes
    group0_lod0_objs = []
    if 0 in scene_meshes and 0 in scene_meshes.get(0, {}):
        group0_lod0_objs = [obj for _, obj in scene_meshes[0][0]]
    trailer_reserved, total_area, cdf = compute_trailer(group0_lod0_objs)

    # ── Write the file ──
    out = open(filepath, 'wb')

    # 1. Header
    out.write(struct.pack('I', FBXMAGICK))
    out.write(struct.pack('I', len(global_vb0)))
    out.write(struct.pack('I', len(global_vb1)))
    index_count = len(global_ib) // index_size if index_size > 0 else 0
    out.write(struct.pack('I', index_count))
    out.write(struct.pack('I', index_size))

    # 2. Vertex buffers
    out.write(global_vb0)
    out.write(global_vb1)

    # 3. Index buffer
    out.write(global_ib)

    # 4. Joints
    out.write(struct.pack('I', len(joints)))
    for j in joints:
        write_joint(out, j)

    # 5. Global params
    out.write(struct.pack('2i', *global_params['reserved']))
    out.write(struct.pack('f', global_params['global_scale']))
    thresholds = global_params['lod_thresholds']
    out.write(struct.pack('I', len(thresholds)))
    if thresholds:
        out.write(struct.pack(f'{len(thresholds)}f', *thresholds))
    out.write(struct.pack('f', global_params['mirror_sign']))
    out.write(struct.pack('3f', *global_params['aabb_center']))
    out.write(struct.pack('f', global_params['sphere_radius']))
    out.write(struct.pack('3f', *global_params['aabb_min']))
    out.write(struct.pack('3f', *global_params['aabb_max']))
    out.write(struct.pack('I', global_params['global_lod_count']))

    # 6. Materials
    out.write(struct.pack('I', len(materials)))
    for m in materials:
        write_material(out, m)

    # 7. Primary material map (Group0)
    out.write(struct.pack('I', len(primary_map)))
    for idx in primary_map:
        out.write(struct.pack('I', idx))

    # 8. Alternate material maps
    out.write(struct.pack('I', len(alternate_maps)))
    for alt in alternate_maps:
        write_string(out, alt['name'])
        indices = alt['indices']
        for idx in indices:
            out.write(struct.pack('I', idx))

    # 9. Secondary material map (Group1)
    out.write(struct.pack('I', len(secondary_map)))
    for idx in secondary_map:
        out.write(struct.pack('I', idx))

    # 10. Mesh groups
    for g in range(2):
        mesh_list = group_mesh_data.get(g, [])
        out.write(struct.pack('I', len(mesh_list)))
        for md in mesh_list:
            bi = md['buf_idx']
            info = buf_info_list[bi]
            mesh_info = {
                'lod': md['lod'],
                'vertex_count': info['vertex_count'],
                'triangle_count': info['triangle_count'],
                'vb0_offset': info['vb0_offset'],
                'vb1_offset': info['vb1_offset'],
                'ib_offset': info['ib_offset'],
            }
            bsphere, bbox = compute_mesh_bounds(md['mesh_obj'])
            write_mesh_descriptor(out, mesh_info, md['meta'],
                                  md['attrib_infos'], bsphere, bbox)

    # 11. Trailer
    out.write(struct.pack('I', trailer_reserved))
    out.write(struct.pack('f', total_area))
    out.write(struct.pack('I', len(cdf)))
    if cdf:
        out.write(struct.pack(f'{len(cdf)}f', *cdf))

    out.close()
    return {'FINISHED'}


# ── Blender Operator ─────────────────────────────────────────────────────────

class EXPORT_OT_binfbx(bpy.types.Operator):
    '''Exports to a binfbx file'''
    bl_idname = "export.binfbx"
    bl_label = "Export BinFBX"

    filepath: bpy.props.StringProperty(name="BinFBX", subtype='FILE_PATH')
    filter_glob: bpy.props.StringProperty(
        default="*.binfbx",
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        # Allow export if there are any Group collections in the scene
        for coll in context.scene.collection.children:
            if coll.name.startswith("Group"):
                return True
        return False

    def execute(self, context):
        bpy.context.window.cursor_set("WAIT")
        self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx")
        try:
            result = write_binfbx(self.filepath, context)
            self.report({'INFO'}, f"Exported BinFBX to {self.filepath}")
        except Exception as e:
            self.report({'ERROR'}, f"Export failed: {e}")
            import traceback
            traceback.print_exc()
            result = {'CANCELLED'}
        bpy.context.window.cursor_set("DEFAULT")
        return result

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = bpy.path.ensure_ext(
                os.path.dirname(bpy.data.filepath) + os.sep + "export",
                ".binfbx")
        else:
            self.filepath = bpy.path.ensure_ext(self.filepath, ".binfbx")
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}
