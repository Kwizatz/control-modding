/*
Copyright (C) 2021 Rodrigo Jose Hernandez Cordoba

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
    };
    struct Header{
        uint32_t Magick;
        uint32_t VertexBufferSizes[2];
        uint32_t IndexCount;
        uint32_t IndexSize;
    };
    struct AttributeInfo{
        uint8_t Index; // 0x0 = AttributeBuffer, 0x1 = VertexBuffer
        uint8_t Type;  // 0x4 = B8G8R8A8_UNORM, 0x7 = R16G16_SINT, 0x8 = R16G16B16A16_SINT, 0xd = R16G16B16A16_UINT, 0x5 =  R8G8B8A8_UINT
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
        Mesh(std::vector<uint8_t>::const_iterator& it);
    private:
            uint32_t mLOD{};
            uint32_t mVertexCount{};
            uint32_t mTriangleCount{};
            std::array<uint32_t,2> mVertexBufferOffsets{};
            uint32_t mIndexBufferOffset{};

            int32_t mUnknown0{};

            std::array<int32_t, 4> mBoundingSphere{};
            std::array<int32_t, 6> mBoundingBox{};

            int32_t mUnknown1{};
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
    private:
        std::array<std::vector<uint8_t>, 2> mVertexBuffers{};
        std::vector<uint8_t> mIndexBuffer{};
        uint32_t mIndexSize;
        std::vector<Joint> mJoints{};

        // Block Of Unknowns
        std::array<int32_t, 2> mUnknown0{};
        float mUnknown1{};
        std::vector<float> mUnknown2;
        float mUnknown3{};
        std::array<float, 3> mUnknown4{};
        float mUnknown5{};
        std::array<float, 3> mUnknown6{};
        std::array<float, 3> mUnknown7{};
        uint32_t mUnknown8{};
        // Materials
        std::vector<Material> mMaterials{};
        std::vector<uint32_t> mMaterialMap{};
        std::vector<std::tuple<std::string,std::vector<uint32_t>>> mAlternaleMaterialMaps{};

        // Block Of Unknowns
        std::vector<uint32_t> mUnknown9{};

        // Meshes
        std::array<std::vector<Mesh>,2> mMeshes{};

        // Block Of Unknowns
        uint32_t mUnknown10{};
        float mUnknown11{};
        std::vector<float> mUnknown12{};
    };
}
#endif
