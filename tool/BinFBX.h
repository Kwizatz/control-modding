/*
Copyright (C) 2021,2022 Rodrigo Jose Hernandez Cordoba

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef CONTROL_MODDING_BINFBX_H
#define CONTROL_MODDING_BINFBX_H

#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <tuple>
#include <unordered_map>

namespace ControlModding
{
    const uint32_t BinFBXMagick = 0x2e;
    enum UniformVariableType {
        Float = 0x00,
        Range = 0x01,
        Color = 0x03,
        Vector = 0x02,
        TextureMap = 0x09,
        TextureSampler = 0x08,
        Boolean = 0x0C,
        NoPayload = 0x10, // Observed type with no payload
    };
    enum AttributeType : uint8_t {
        FLOAT3       = 0x2,  // POSITION
        BYTE4_SNORM  = 0x4,  // TANGENT ?
        BYTE4_UNORM  = 0x5, // BONE_WEIGHT
        SHORT2_SNORM = 0x7,  // TEXCOORD
        SHORT4_SNORM = 0x8,  // NORMAL
        SHORT4_UINT  = 0xd,  // BONE_INDEX
        BYTE4_UINT   = 0xf,  // BONE_INDEX
    };

    const std::unordered_map<AttributeType, std::string> AttributeTypeNames = {
        {AttributeType::FLOAT3, "FLOAT3"},
        {AttributeType::BYTE4_SNORM, "BYTE4_SNORM"},
        {AttributeType::BYTE4_UNORM, "BYTE4_UNORM"},
        {AttributeType::SHORT2_SNORM, "SHORT2_SNORM"},
        {AttributeType::SHORT4_SNORM, "SHORT4_SNORM"},
        {AttributeType::SHORT4_UINT, "SHORT4_UINT"},
        {AttributeType::BYTE4_UINT, "BYTE4_UINT"}
    };

    struct Header{
        uint32_t Magick;
        uint32_t VertexBufferSizes[2];
        uint32_t IndexCount;
        uint32_t IndexSize;
    };
    struct AttributeInfo{
        uint8_t Index; // 0x0 = AttributeBuffer, 0x1 = VertexBuffer
        AttributeType Type;  // 0x4 = B8G8R8A8_UNORM, 0x7 = R16G16_SINT, 0x8 = R16G16B16A16_SINT, 0xd = R16G16B16A16_UINT, 0x5 =  R8G8B8A8_UINT
        uint8_t Usage; // 0x1 = Normal, 0x2 = TexCoord, 0x3 = Tangent, 0x5 = Index, 0x6 = Weight
        uint8_t Zero;  // Always 0?
        operator uint32_t() const { return *reinterpret_cast<const uint32_t*>(this); }
        AttributeInfo(std::vector<uint8_t>::const_iterator& it) : Index{*it++}, Type{*it++}, Usage{*it++}, Zero{*it++} {}
    };

    class Joint
    {
    public:
        Joint(std::vector<uint8_t>::const_iterator& it);
        void Write(std::ofstream& out) const;
    private:
        std::string mName;
        std::array<float, 12> mMatrix{};
        std::array<float, 3> mEnvelope{};
        float mRadius{};
        int32_t mParent{};
    };

    class UniformVariable
    {
    public:
        UniformVariable(std::vector<uint8_t>::const_iterator& it);
        void Write(std::ofstream& out) const;
    private:
        using UniformData = std::variant<std::monostate, float, uint32_t, std::array<float, 2>, std::array<float, 3>, std::array<float, 4>, std::string>;
        std::string mName{};
        uint32_t mUniformType{};
        UniformData mData{};
    };

    class Material
    {
    public:
        Material(std::vector<uint8_t>::const_iterator& it);
        void Write(std::ofstream& out) const;
    private:
        std::array<uint8_t, 8> mMaterialId{};
        std::string mName;
        std::string mType;
        std::string mPath;
        std::array<int32_t, 6> mUnknown0{};
        std::vector<UniformVariable> mUniformVariables{};
    };

    class Mesh
    {
    public:
        Mesh(size_t aIndex, const std::array<std::vector<uint8_t>, 2>& aVertexBuffers,const std::vector<uint8_t>& aIndexBuffer, uint32_t aIndexSize, std::vector<uint8_t>::const_iterator& it);
        void Write(std::ofstream& out) const;
        void Dump() const;
        size_t GetIndex() const { return mIndex; }
        size_t GetLOD() const { return mLOD; }
        std::tuple<size_t, size_t> GetVertexSizes() const;
        size_t GetTriangleCount() const { return mTriangleCount; }
    // Compute and append per-triangle areas to 'out'; returns true if positions were found
    bool AccumulateTriangleAreas(std::vector<float>& out) const;
    private:
        //Internal Data-------------------------------------------------------
        size_t mIndex{};
        /*  @note 
            mVertexBuffers and mIndexBuffer are local copies with base 0 indices,
            used to easilly reconstruct global buffers when removing and adding meshes. 
        */
        std::array<std::vector<uint8_t>, 2> mVertexBuffers{};
        std::vector<uint8_t> mIndexBuffer{};
        uint32_t mIndexSize;
        //External Data-------------------------------------------------------
        uint32_t mLOD{};
        uint32_t mVertexCount{};
        uint32_t mTriangleCount{};
        std::array<uint32_t,2> mVertexBufferOffsets{};
        uint32_t mIndexBufferOffset{};

        int32_t mMeshFlags0{};                        // Per-mesh flags0 (bitfield)

        std::array<float, 4> mBoundingSphere{};       // (cx, cy, cz, r)
        std::array<float, 6> mBoundingBox{};          // (minx, miny, minz, maxx, maxy, maxz)

        int32_t mMeshFlags1{};                        // Per-mesh flags1 (bitfield)
        
        /// @note IMPORTANT: The AttributeInfo count must be written as a uint_8_t, not an int_32_t.
        std::vector<AttributeInfo> mAttributeInfos{};

        int32_t mUnknown2{};
        float mUnknown3{};
        uint8_t mUnknown4{};
        float mUnknown5{};
    };

    class BinFBX
    {
    public:
        BinFBX(const std::vector<uint8_t>& aBuffer);
        void Write(std::string_view aFileName) const;
        void Dump() const;
        void RemoveMesh(uint32_t aGroup, uint32_t aLOD, uint32_t aIndex);
    private:
    void RecomputeTrailerFromMeshes(); // updates mTotalSurfaceArea & mTriangleAreaCDF if possible
        std::array<std::vector<uint8_t>, 2> mVertexBuffers{};
        std::vector<uint8_t> mIndexBuffer{};
        uint32_t mIndexSize;
        std::vector<Joint> mJoints{};

        // Global params block (after joints)
        std::array<int32_t, 2> mReservedInts{};       // Reserved0, Reserved1
        float mGlobalScale{};                          // Global scale
        std::vector<float> mLODThresholds;             // Optional LOD thresholds
        float mMirrorSign{};                           // Mirror sign (handedness)
        std::array<float, 3> mAABBCenter{};            // AABB center
        float mBoundingSphereRadius{};                 // Bounding sphere radius
        std::array<float, 3> mAABBMin{};               // AABB min
        std::array<float, 3> mAABBMax{};               // AABB max
        uint32_t mGlobalLODCount{};                    // Number of LOD levels present
        // Materials
        std::vector<Material> mMaterials{};
        std::array<std::vector<uint32_t>,2> mMaterialMaps{};
        std::vector<std::tuple<std::string,std::vector<uint32_t>>> mAlternateMaterialMaps{};

        // Meshes
        std::array<std::vector<Mesh>,2> mMeshes{};

        // Trailing block
        uint32_t mTailReserved0{};            // Observed 0 across dataset
        float mTotalSurfaceArea{};            // Sum of triangle areas (approx.)
        std::vector<float> mTriangleAreaCDF{}; // Monotonic [0..1], length ~ total triangles
    };
}
#endif
