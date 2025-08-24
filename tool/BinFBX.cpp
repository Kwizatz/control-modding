/*
Copyright (C) 2021,2022,2025 Rodrigo Jose Hernandez Cordoba

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

#include "BinFBX.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <sstream>
namespace ControlModding
{
    Joint::Joint(std::vector<uint8_t>::const_iterator& it) :
        mName
        {
            reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
            static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
        }
    {
        it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));
        memcpy(mMatrix.data(), &(*it), sizeof(float) * mMatrix.size());
        it += sizeof(float)*mMatrix.size();
        memcpy(mEnvelope.data(), &(*it), sizeof(float)*mEnvelope.size());
        it += sizeof(float)*mEnvelope.size();
        mRadius = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);
        mParent = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
    }

    void Joint::Write(std::ofstream& out) const
    {
        uint32_t size = static_cast<uint32_t>(mName.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(mName.c_str(), size);
        out.write(reinterpret_cast<const char*>(mMatrix.data()), sizeof(float)*mMatrix.size());
        out.write(reinterpret_cast<const char*>(mEnvelope.data()), sizeof(float)*mEnvelope.size());
        out.write(reinterpret_cast<const char*>(&mRadius), sizeof(float));
        out.write(reinterpret_cast<const char*>(&mParent), sizeof(int32_t));
    }

    


    UniformVariable::UniformVariable(std::vector<uint8_t>::const_iterator& it):
        mName
        {
            reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
            static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
        }
    {
        it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));
        mUniformType = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);            
        switch(mUniformType)
        {
        case Float:
            mData = *reinterpret_cast<const float*>(&(*it));
            it += sizeof(float);
            break;
        case Range:
            mData = std::array<float,2>
            {
                *reinterpret_cast<const float*>(&(*it)),
                *reinterpret_cast<const float*>(&(*it) + sizeof(float))
            };
            it += sizeof(float) * 2;
            break;
        case Color:
            mData = std::array<float,4>{
                *reinterpret_cast<const float*>(&(*it)),
                * reinterpret_cast<const float*>(&(*it) + sizeof(float)),
                * reinterpret_cast<const float*>(&(*it) + sizeof(float)*2),
                * reinterpret_cast<const float*>(&(*it) + sizeof(float)*3)
            };
            it += sizeof(float) * 4;
            break;
        case Vector:
            mData = std::array<float,3>
            {
                *reinterpret_cast<const float*>(&(*it)),
                *reinterpret_cast<const float*>(&(*it) + sizeof(float)),
                *reinterpret_cast<const float*>(&(*it) + sizeof(float)*2)
            };
            it += sizeof(float) * 3;
            break;
        case TextureMap:
            mData = std::string
            {
                reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
                static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
            };
            it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));
            break;
        case TextureSampler:
            break;
        case Boolean:
            mData = *reinterpret_cast<const uint32_t*>(&(*it));
            it += sizeof(uint32_t);
            break;
        case NoPayload:
            // No extra data
            break;
        }
    }
    void UniformVariable::Write(std::ofstream& out) const
    {
        uint32_t size = static_cast<uint32_t>(mName.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(mName.data(), size);

        out.write(reinterpret_cast<const char*>(&mUniformType), sizeof(uint32_t));
    switch(mUniformType)
        {
        case Float:
            out.write(reinterpret_cast<const char*>(&std::get<float>(mData)), sizeof(float));
            break;
        case Range:
            out.write(reinterpret_cast<const char*>(std::get<std::array<float,2>>(mData).data()), sizeof(float)*2);
            break;
        case Color:
            out.write(reinterpret_cast<const char*>(std::get<std::array<float,4>>(mData).data()), sizeof(float)*4);
            break;
        case Vector:
            out.write(reinterpret_cast<const char*>(std::get<std::array<float,3>>(mData).data()), sizeof(float)*3);
            break;
        case TextureMap:
            {
                uint32_t size = static_cast<uint32_t>(std::get<std::string>(mData).size());
                out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
                out.write(std::get<std::string>(mData).data(), size);
            }
            break;
        case TextureSampler:
            break;
        case Boolean:
            out.write(reinterpret_cast<const char*>(&std::get<uint32_t>(mData)), sizeof(uint32_t));
            break;
        case NoPayload:
            // No data to write
            break;
        }
    }

    Material::Material(std::vector<uint8_t>::const_iterator& it)
    {
        int32_t magick = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);

        if (magick != 7)
        {
            throw std::runtime_error("Invalid Material.");
        }

        memcpy(mMaterialId.data(), &(*it), mMaterialId.size());
        it += mMaterialId.size();

        mName = 
        {
            reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
            static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
        };
        it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));
        mType =
        {
            reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
            static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
        };
        it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));
        mPath = 
        {
            reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
            static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
        };
        it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));

    memcpy(mMaterialParams.data(), &(*it), sizeof(uint32_t) * mMaterialParams.size());
    it += sizeof(uint32_t) * mMaterialParams.size();

        int32_t count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUniformVariables.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mUniformVariables.emplace_back(it);
        }
    }
    void Material::Write(std::ofstream& out) const
    {
        uint32_t size{7};
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mMaterialId), mMaterialId.size());
        size = static_cast<uint32_t>(mName.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(mName.data(), size);
        size = static_cast<uint32_t>(mType.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(mType.data(), size);
        size = static_cast<uint32_t>(mPath.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(mPath.data(), size);
        out.write(reinterpret_cast<const char*>(mMaterialParams.data()), sizeof(uint32_t) * mMaterialParams.size());
        size = static_cast<uint32_t>(mUniformVariables.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(int32_t));
        for (const auto& i : mUniformVariables)
        {
            i.Write(out);
        }
    }

    void Material::Dump() const
    {
        auto hexu = [](uint32_t v){ std::ostringstream oss; oss << "0x" << std::hex << v; return oss.str(); };
        auto familyName = [&](uint32_t v)->const char*
        {
            switch(v){
                case 0x0: return "standard";
                case 0x1: return "hair";
                case 0x2: return "eye";
                case 0x3: return "cloth";
                default:  return "unknown";
            }
        };
        auto renderName = [&](uint32_t v)->const char*
        {
            switch(v){
                case 0x4: return "opaque/default";
                case 0x1: return "alpha/masked";
                case 0x8: return "additive";
                default:  return "unknown";
            }
        };

        const uint32_t u0 = mMaterialParams[0]; // MaterialFlags (bitfield)
        const uint32_t u1 = mMaterialParams[1]; // DecalMode
        const uint32_t u2 = mMaterialParams[2]; // LayoutVariant
        const uint32_t u3 = mMaterialParams[3]; // LightingVariant
        const uint32_t u4 = mMaterialParams[4]; // MaterialFamily
        const uint32_t u5 = mMaterialParams[5]; // RenderMode

        const bool specialPipeline = (u0 & 0x80000000u) != 0;

        std::cout << "Material" << std::endl;
        std::cout << "  Name: " << mName << " Type: " << mType << std::endl;
        std::cout << "  Path: " << mPath << std::endl;
        std::cout << "  Family(u4): " << hexu(u4) << " (" << familyName(u4) << ")" << std::endl;
        std::cout << "  RenderMode(u5): " << hexu(u5) << " (" << renderName(u5) << ")" << std::endl;
        std::cout << "  Flags(u0): " << hexu(u0) << " (SpecialPipeline=" << (specialPipeline?"on":"off") << ")" << std::endl;
        std::cout << "  DecalMode(u1): " << hexu(u1);
        if (mType.find("decal") != std::string::npos) std::cout << "  [decal]";
        std::cout << std::endl;
        std::cout << "  LayoutVariant(u2): " << hexu(u2) << ", LightingVariant(u3): " << hexu(u3) << std::endl;
        std::cout << "  Uniforms: " << mUniformVariables.size() << std::endl;
    }

    Mesh::Mesh(size_t aIndex, const std::array<std::vector<uint8_t>, 2>& aVertexBuffers,const std::vector<uint8_t>& aIndexBuffer, uint32_t aIndexSize, std::vector<uint8_t>::const_iterator& it) : mIndex{aIndex}, mIndexSize{aIndexSize}
    {
        mLOD = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);

        mVertexCount = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);

        mTriangleCount = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);

        memcpy(mVertexBufferOffsets.data(), &(*it), sizeof(uint32_t) *mVertexBufferOffsets.size());
        it += sizeof(uint32_t) * mVertexBufferOffsets.size();

        mIndexBufferOffset = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);

        mMeshFlags0 = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);

        memcpy(mBoundingSphere.data(), &(*it), sizeof(float) * mBoundingSphere.size());
        it += sizeof(float) * mBoundingSphere.size();

        memcpy(mBoundingBox.data(), &(*it), sizeof(float) * mBoundingBox.size());
        it += sizeof(float) * mBoundingBox.size();

        mMeshFlags1 = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);

        uint8_t count = *it++;
        mAttributeInfos.reserve(count);
        for (uint8_t i = 0; i < count; ++i)
        {
            mAttributeInfos.emplace_back(it);
        }

    mJoint = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown3 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

    mIsRigidMesh = *it++;

        mUnknown5 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        // Build local buffers
        std::tuple<size_t, size_t> vertex_sizes = GetVertexSizes();
        std::unordered_map<size_t, size_t> index_to_local{};
        index_to_local.reserve(mIndexSize * mTriangleCount * 3);
        mVertexBuffers[0].reserve(mTriangleCount * 3 * std::get<0>(vertex_sizes));
        mVertexBuffers[1].reserve(mTriangleCount * 3 * std::get<1>(vertex_sizes));
        mIndexBuffer.resize(mIndexSize * mTriangleCount * 3);
        const uint8_t* index_data = aIndexBuffer.data() + mIndexBufferOffset * mIndexSize;
        const size_t stride[2] = { std::get<0>(vertex_sizes), std::get<1>(vertex_sizes) };

        for (size_t i = 0; i < mTriangleCount; ++i)
        {
            for (size_t j = 0; j < 3; ++j)
            {
                size_t index{};
                switch(mIndexSize)
                {
                case 1:
                    index = index_data[i * 3 + j];
                break;
                case 2:
                    index = reinterpret_cast<const uint16_t*>(index_data)[i * 3 + j];
                break;
                case 4:
                    index = reinterpret_cast<const uint32_t*>(index_data)[i * 3 + j];
                break;
                case 8:
                    index = reinterpret_cast<const uint64_t*>(index_data)[i * 3 + j];
                break;
                }
                if(index_to_local.find(index) == index_to_local.end())
                {
                    //std::cout << "Mapping index " << index << " to local index " << index_to_local.size() << " with offsets " << mVertexBufferOffsets[0] << " and " << mVertexBufferOffsets[1] << " and Index Offset " << mIndexBufferOffset << std::endl;
                    index_to_local[index] = index_to_local.size();
                    for(size_t k = 0;k<2;++k)
                    {
                        auto src_begin = aVertexBuffers[k].begin()
                                        + static_cast<std::ptrdiff_t>(mVertexBufferOffsets[k])
                                        + static_cast<std::ptrdiff_t>(index)  * stride[k];
                        auto src_end   = src_begin + static_cast<std::ptrdiff_t>(stride[k]);
                        mVertexBuffers[k].insert(mVertexBuffers[k].end(), src_begin, src_end);
                    }
                }
                switch(mIndexSize)
                {
                case 1:
                    mIndexBuffer[i * 3 + j] = index_to_local[index_data[i * 3 + j]];
                break;
                case 2:
                    reinterpret_cast<uint16_t*>(mIndexBuffer.data())[i * 3 + j] = index_to_local[ reinterpret_cast<const uint16_t*>(index_data)[i * 3 + j]];
                break;
                case 4:
                    reinterpret_cast<uint32_t*>(mIndexBuffer.data())[i * 3 + j] = index_to_local[ reinterpret_cast<const uint32_t*>(index_data)[i * 3 + j]];
                break;
                case 8:
                    reinterpret_cast<uint64_t*>(mIndexBuffer.data())[i * 3 + j] = index_to_local[ reinterpret_cast<const uint64_t*>(index_data)[i * 3 + j]];
                break;
                }                
            }
        }
    }

    std::tuple<size_t, size_t> Mesh::GetVertexSizes() const
    {
        std::tuple<size_t, size_t> result{0, 0};
        for(auto i: mAttributeInfos)
        {
            switch(i.Type)
            {
            case AttributeType::FLOAT3:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 12;
            break;
                case AttributeType::BYTE4_SNORM:
                case AttributeType::BYTE4_UNORM:
                case AttributeType::BYTE4_UINT:
                case AttributeType::SHORT2_SNORM:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 4;
            break;
                case AttributeType::SHORT4_SNORM:
                case AttributeType::SHORT4_UINT:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 8;
            break;
            }
        }
        return result;
    }

    bool Mesh::AccumulateTriangleAreas(std::vector<float>& out) const
    {
        // Find POSITION attribute entry (FLOAT3). Index indicates which buffer (0: attr, 1: vertex)
        int posIndex = -1;
        for (const auto& a : mAttributeInfos)
        {
            if (a.Type == AttributeType::FLOAT3 && a.Usage == 0x00 /*POSITION by convention*/)
            {
                posIndex = a.Index ? 0 : 1; // our storage uses [0]=attr, [1]=vertex
                break;
            }
        }
        if (posIndex < 0) return false;

        const uint8_t* posBuf = mVertexBuffers[posIndex].data();
        size_t stride = (posIndex==0) ? std::get<0>(GetVertexSizes()) : std::get<1>(GetVertexSizes());
        if (stride == 0) return false;

        auto loadPos = [&](size_t v)->std::array<float,3>
        {
            const float* p = reinterpret_cast<const float*>(posBuf + v*stride);
            return {p[0], p[1], p[2]};
        };
        auto sub = [](const std::array<float,3>& a, const std::array<float,3>& b){return std::array<float,3>{a[0]-b[0],a[1]-b[1],a[2]-b[2]};};
        auto cross = [](const std::array<float,3>& a, const std::array<float,3>& b){return std::array<float,3>{a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]};};
        auto norm = [](const std::array<float,3>& a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);};

        // Use local index buffer which is already remapped
        const uint8_t* indices = mIndexBuffer.data();
        for (size_t i = 0; i < mTriangleCount; ++i)
        {
            size_t i0, i1, i2;
            switch(mIndexSize)
            {
            case 1:
                i0 = indices[i*3+0]; i1 = indices[i*3+1]; i2 = indices[i*3+2];
                break;
            case 2:
                i0 = reinterpret_cast<const uint16_t*>(indices)[i*3+0];
                i1 = reinterpret_cast<const uint16_t*>(indices)[i*3+1];
                i2 = reinterpret_cast<const uint16_t*>(indices)[i*3+2];
                break;
            case 4:
                i0 = reinterpret_cast<const uint32_t*>(indices)[i*3+0];
                i1 = reinterpret_cast<const uint32_t*>(indices)[i*3+1];
                i2 = reinterpret_cast<const uint32_t*>(indices)[i*3+2];
                break;
            case 8:
                i0 = static_cast<size_t>(reinterpret_cast<const uint64_t*>(indices)[i*3+0]);
                i1 = static_cast<size_t>(reinterpret_cast<const uint64_t*>(indices)[i*3+1]);
                i2 = static_cast<size_t>(reinterpret_cast<const uint64_t*>(indices)[i*3+2]);
                break;
            default:
                return false;
            }
            auto p0 = loadPos(i0);
            auto p1 = loadPos(i1);
            auto p2 = loadPos(i2);
            auto e0 = sub(p1,p0);
            auto e1 = sub(p2,p0);
            float area = 0.5f * norm(cross(e0,e1));
            out.push_back(area);
        }
        return true;
    }

    

    void Mesh::Write(std::ofstream& out) const
    {
        out.write(reinterpret_cast<const char*>(&mLOD), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mVertexCount), sizeof(uint32_t)); 
        out.write(reinterpret_cast<const char*>(&mTriangleCount), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mVertexBufferOffsets.data()), sizeof(uint32_t) * mVertexBufferOffsets.size());
        out.write(reinterpret_cast<const char*>(&mIndexBufferOffset), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mMeshFlags0), sizeof(int32_t));
        out.write(reinterpret_cast<const char*>(mBoundingSphere.data()), sizeof(float) * mBoundingSphere.size());
        out.write(reinterpret_cast<const char*>(mBoundingBox.data()), sizeof(float) * mBoundingBox.size());
        out.write(reinterpret_cast<const char*>(&mMeshFlags1), sizeof(int32_t));
        uint8_t count = static_cast<uint8_t>(mAttributeInfos.size());
        out.write(reinterpret_cast<const char*>(&count), 1);
        out.write(reinterpret_cast<const char*>(mAttributeInfos.data()), sizeof(decltype(mAttributeInfos)::value_type) * mAttributeInfos.size());
        out.write(reinterpret_cast<const char*>(&mJoint), sizeof(int32_t));
        out.write(reinterpret_cast<const char*>(&mUnknown3), sizeof(float));
        out.write(reinterpret_cast<const char*>(&mIsRigidMesh), 1);
        out.write(reinterpret_cast<const char*>(&mUnknown5), sizeof(float));
    }

    void Mesh::Dump() const
    {
        std::cout << "Mesh" << std::endl;
        std::cout << "LOD: " << mLOD << std::endl;
        std::cout << "VertexCount: " << mVertexCount << std::endl;
        std::cout << "TriangleCount: " << mTriangleCount << std::endl;
        std::cout << "Flags0: 0x" << std::hex << mMeshFlags0 << std::dec << std::endl;
        std::cout << "VertexBufferOffsets: ";
        for (const auto& i : mVertexBufferOffsets)
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        std::cout << "IndexBufferOffset: " << mIndexBufferOffset << std::endl;
        std::cout << "BoundingSphere: (" << mBoundingSphere[0] << ", " << mBoundingSphere[1] << ", " << mBoundingSphere[2] << ", r=" << mBoundingSphere[3] << ")" << std::endl;
        std::cout << "AABBMin: (" << mBoundingBox[0] << ", " << mBoundingBox[1] << ", " << mBoundingBox[2] << ")" << std::endl;
        std::cout << "AABBMax: (" << mBoundingBox[3] << ", " << mBoundingBox[4] << ", " << mBoundingBox[5] << ")" << std::endl;
        std::cout << "Flags1: 0x" << std::hex << mMeshFlags1 << std::dec << std::endl;
    // Unknowns: surfaced for analysis. Early dataset hints:
    //  - Unknown3 and Unknown5 are floats; they cluster to a few constants in many files.
    std::cout << "Joint (int32): " << mJoint << std::endl;
    std::cout << "Unknown3 (float): " << mUnknown3 << std::endl;
    {
        const char* skinStr = (mIsRigidMesh == 0) ? "skinned" : (mIsRigidMesh == 1 ? "rigid" : "unknown");
        std::cout << "IsRigidMesh (u8): " << static_cast<uint32_t>(mIsRigidMesh) << " [" << skinStr << "]" << std::endl;
    }
    std::cout << "Unknown5 (float): " << mUnknown5 << std::endl;
    //
#if 0
        std::cout << "Indices: " << std::endl;
        const uint8_t* indices = mIndexBuffer.data();
        for (size_t i = 0; i < mTriangleCount * 3; ++i)
        {
            switch(mIndexSize)
            {
            case 1:
                std::cout << static_cast<size_t>(indices[i]) << " ";
                ++indices;
            break;
            case 2:
                std::cout << *reinterpret_cast<const uint16_t*>(indices) << " ";
                indices+=2;
            break;
            case 4:
                std::cout << *reinterpret_cast<const uint32_t*>(indices) << " ";
                indices+=4;
            break;
            case 8:
                std::cout << *reinterpret_cast<const uint64_t*>(indices) << " ";
                indices+=8;
            break;
            }
        }
        std::cout << std::endl;
        std::tuple<size_t, size_t> vertex_sizes = GetVertexSizes();
        std::cout << "VertexSizes: " << std::get<0>(vertex_sizes) << " " << std::get<1>(vertex_sizes) << std::endl;
#endif
    }

    BinFBX::BinFBX(const std::vector<uint8_t>& aBuffer)
    {
        const Header* header = reinterpret_cast<const Header*>(aBuffer.data());
        if (header->Magick != BinFBXMagick)
        {
            throw std::runtime_error("Invalid BinFBX file.");
        }
        mIndexSize = header->IndexSize;
        std::vector<uint8_t>::const_iterator it = aBuffer.begin() + sizeof(Header);

        mVertexBuffers[0].resize(header->VertexBufferSizes[0]);
        std::copy(it, it+mVertexBuffers[0].size(), mVertexBuffers[0].begin());
        it += mVertexBuffers[0].size();

        mVertexBuffers[1].resize(header->VertexBufferSizes[1]);
        std::copy(it, it+mVertexBuffers[1].size(), mVertexBuffers[1].begin());
        it += mVertexBuffers[1].size();

        mIndexBuffer.resize(header->IndexCount * mIndexSize);
        std::copy(it, it + mIndexBuffer.size(), mIndexBuffer.begin());
        it += mIndexBuffer.size();


        int32_t count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mJoints.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mJoints.emplace_back(it);
        }

        // Global params block
        memcpy(mReservedInts.data(), &(*it), sizeof(int32_t) * mReservedInts.size());
        it += sizeof(int32_t) * mReservedInts.size();

        mGlobalScale = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mLODThresholds.clear();
        mLODThresholds.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mLODThresholds.emplace_back(*reinterpret_cast<const float*>(&(*it)));
            it += sizeof(float);
        }

        mMirrorSign = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        memcpy(mAABBCenter.data(), &(*it), sizeof(float) * mAABBCenter.size());
        it += sizeof(float) * mAABBCenter.size();

        mBoundingSphereRadius = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        memcpy(mAABBMin.data(), &(*it), sizeof(float) * mAABBMin.size());
        it += sizeof(float) * mAABBMin.size();

        memcpy(mAABBMax.data(), &(*it), sizeof(float) * mAABBMax.size());
        it += sizeof(float) * mAABBMax.size();

        mGlobalLODCount = *reinterpret_cast<const uint32_t*>(&(*it));
        it += sizeof(uint32_t);

        // Materials
        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mMaterials.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mMaterials.emplace_back(it);
        }

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mMaterialMaps[0].reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mMaterialMaps[0].emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
            it += sizeof(uint32_t);
        }

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mAlternateMaterialMaps.reserve(count);

        for (int32_t i = 0; i < count; ++i)
        {
            std::string name =
            {
                reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
                static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
            };
            it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));

            std::vector<uint32_t> v;
            v.reserve(mMaterialMaps[0].size());
            for (int32_t j = 0; j < mMaterialMaps[0].size(); ++j)
            {
                v.emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
                it += sizeof(uint32_t);
             }
            mAlternateMaterialMaps.emplace_back(std::move(name), std::move(v));
        }

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mMaterialMaps[1].reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mMaterialMaps[1].emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
            it += sizeof(uint32_t);
        }

        // Meshes
        for(auto& m : mMeshes)
        {
            count = *reinterpret_cast<const int32_t*>(&(*it));
            it += sizeof(int32_t);
            m.reserve(count);
            uint32_t lod{*reinterpret_cast<const uint32_t*>(&(*it))};
            uint32_t index{0};
            for (int32_t i = 0; i < count; ++i)
            {
                if(lod != *reinterpret_cast<const uint32_t*>(&(*it)))
                {
                    lod = *reinterpret_cast<const uint32_t*>(&(*it));
                    index = 0;
                }
                m.emplace_back(index++, mVertexBuffers, mIndexBuffer, mIndexSize, it);
            }
        }

        // Trailing block
        mTailReserved0 = *reinterpret_cast<const uint32_t*>(&(*it)); 
        it += sizeof(uint32_t);
        mTotalSurfaceArea = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mTriangleAreaCDF.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mTriangleAreaCDF.emplace_back(*reinterpret_cast<const float*>(&(*it)));
            it += sizeof(float);
        }
    }

    void BinFBX::RecomputeTrailerFromMeshes()
    {
        // Aggregate triangle areas across both groups
        std::vector<float> areas;
        areas.reserve(1024);
        bool ok = false;
        for (const auto& group : mMeshes)
        {
            for (const auto& mesh : group)
            {
                ok = mesh.AccumulateTriangleAreas(areas) || ok;
            }
        }
        if (!ok || areas.empty()) return; // leave existing trailer untouched if we couldn't compute
        // Compute total and CDF normalized to 1.0
        double total = 0.0;
        for (float a : areas) total += a;
        if (total <= 0.0) return;
        mTotalSurfaceArea = static_cast<float>(total);
        mTriangleAreaCDF.resize(areas.size());
        double accum = 0.0;
        for (size_t i = 0; i < areas.size(); ++i)
        {
            accum += areas[i];
            mTriangleAreaCDF[i] = static_cast<float>(accum / total);
        }
        // Clamp last to 1.0 exactly
        if (!mTriangleAreaCDF.empty()) mTriangleAreaCDF.back() = 1.0f;
        mTailReserved0 = 0;
    }

    void BinFBX::Write(std::string_view aFileName) const
    {
        std::ofstream out;
        out.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        out.open(aFileName.data(), std::ofstream::out | std::ofstream::binary);
        out.write("\x2e\0\0\0", 4);

        uint32_t size{static_cast<uint32_t>(mVertexBuffers[0].size())};
        out.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));

        size = static_cast<uint32_t>(mVertexBuffers[1].size());
        out.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));

        size = static_cast<uint32_t>(mIndexBuffer.size()/mIndexSize);
        out.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mIndexSize), sizeof(uint32_t));

        for(auto& i: mVertexBuffers)
        {
            out.write(reinterpret_cast<const char*>(i.data()), i.size());
        }

        out.write(reinterpret_cast<const char*>(mIndexBuffer.data()), mIndexBuffer.size());

        size = static_cast<uint32_t>(mJoints.size());
        out.write(reinterpret_cast<char*>(&size), sizeof(uint32_t));
        for(auto& i: mJoints)
        {
            i.Write(out);
        }

        // Global params block
        out.write(reinterpret_cast<const char*>(mReservedInts.data()), sizeof(decltype(mReservedInts)::value_type) * mReservedInts.size());
        out.write(reinterpret_cast<const char*>(&mGlobalScale), sizeof(float));

        size = static_cast<uint32_t>(mLODThresholds.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        if (size)
        {
            out.write(reinterpret_cast<const char*>(mLODThresholds.data()), sizeof(float) * mLODThresholds.size());
        }

        out.write(reinterpret_cast<const char*>(&mMirrorSign), sizeof(float));
        out.write(reinterpret_cast<const char*>(mAABBCenter.data()), sizeof(float) * mAABBCenter.size());
        out.write(reinterpret_cast<const char*>(&mBoundingSphereRadius), sizeof(float));
        out.write(reinterpret_cast<const char*>(mAABBMin.data()), sizeof(float) * mAABBMin.size());
        out.write(reinterpret_cast<const char*>(mAABBMax.data()), sizeof(float) * mAABBMax.size());
        out.write(reinterpret_cast<const char*>(&mGlobalLODCount), sizeof(uint32_t));

        // Materials
        size = static_cast<uint32_t>(mMaterials.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        for(auto& i: mMaterials)
        {
            i.Write(out);
        }

        size = static_cast<uint32_t>(mMaterialMaps[0].size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mMaterialMaps[0].data()), sizeof(decltype(mMaterialMaps)::value_type::value_type) * mMaterialMaps[0].size());

        size = static_cast<uint32_t>(mAlternateMaterialMaps.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        for(auto& i: mAlternateMaterialMaps)
        {
            size = static_cast<uint32_t>(std::get<std::string>(i).size());
            out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
            out.write(std::get<std::string>(i).data(), std::get<std::string>(i).size());
            size = static_cast<uint32_t>(std::get<std::vector<uint32_t>>(i).size());
            out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
            out.write(reinterpret_cast<const char*>(std::get<std::vector<uint32_t>>(i).data()), sizeof(uint32_t) * std::get<std::vector<uint32_t>>(i).size());
        }

        size = static_cast<uint32_t>(mMaterialMaps[1].size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mMaterialMaps[1].data()), sizeof(decltype(mMaterialMaps)::value_type::value_type) * mMaterialMaps[1].size());

        for(auto& i: mMeshes)
        {
            size = static_cast<uint32_t>(i.size());
            out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
            for(auto& j: i)
            {
                j.Write(out);
            }
        }

        out.write(reinterpret_cast<const char*>(&mTailReserved0), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mTotalSurfaceArea), sizeof(float));
        size = static_cast<uint32_t>(mTriangleAreaCDF.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mTriangleAreaCDF.data()), sizeof(float) * mTriangleAreaCDF.size());
        out.close();
    }

    void BinFBX::Dump() const
    {
        std::cout << "GlobalParams" << std::endl;
        std::cout << "  Reserved: " << mReservedInts[0] << ", " << mReservedInts[1] << std::endl;
        std::cout << "  GlobalScale: " << mGlobalScale << std::endl;
        std::cout << "  LODThresholds (" << mLODThresholds.size() << "):";
        for (auto v : mLODThresholds) {std::cout << " " << v;}
        std::cout << std::endl;
        std::cout << "  MirrorSign: " << mMirrorSign << std::endl;
        std::cout << "  AABBCenter: (" << mAABBCenter[0] << ", " << mAABBCenter[1] << ", " << mAABBCenter[2] << ")" << std::endl;
        std::cout << "  BoundingSphereRadius: " << mBoundingSphereRadius << std::endl;
        std::cout << "  AABBMin: (" << mAABBMin[0] << ", " << mAABBMin[1] << ", " << mAABBMin[2] << ")" << std::endl;
        std::cout << "  AABBMax: (" << mAABBMax[0] << ", " << mAABBMax[1] << ", " << mAABBMax[2] << ")" << std::endl;
        std::cout << "  GlobalLODCount: " << mGlobalLODCount << std::endl;
        // Materials summary
        std::cout << "Materials (" << mMaterials.size() << ")" << std::endl;
        for (const auto& m : mMaterials)
        {
            m.Dump();
        }

        std::array<std::vector<size_t>, 2> totalTriangleCount{{}};
        size_t triangleCount[2] = {0};
        {
            size_t groupIndex = 0;
            for (const auto &group : mMeshes)
            {
                std::cout << "Group " << groupIndex << std::endl;
                for (const auto &mesh : group)
                {
                    mesh.Dump();
                    totalTriangleCount[groupIndex].push_back(mesh.GetTriangleCount());
                    triangleCount[groupIndex] += mesh.GetTriangleCount();
                }
                ++groupIndex;
            }
        }

        // Trailing block summary
        std::cout << "Trailer" << std::endl;
        std::cout << "  Reserved0 (u32): " << mTailReserved0 << std::endl;
        std::cout << "  TotalSurfaceArea (float): " << mTotalSurfaceArea << std::endl;
        std::cout << "  TriangleAreaCDF count: " << mTriangleAreaCDF.size() << std::endl;
        if (!mTriangleAreaCDF.empty())
        {
            std::cout << "    head:";
            for (size_t i = 0; i < std::min<size_t>(mTriangleAreaCDF.size(), 8); ++i)
            {
                std::cout << " " << mTriangleAreaCDF[i];
            }
            auto minmax = std::minmax_element(mTriangleAreaCDF.begin(), mTriangleAreaCDF.end());
            bool nonneg = std::all_of(mTriangleAreaCDF.begin(), mTriangleAreaCDF.end(), [](float x){return x >= 0.0f;});
            bool mono_inc = std::is_sorted(mTriangleAreaCDF.begin(), mTriangleAreaCDF.end());
            bool mono_dec = std::is_sorted(mTriangleAreaCDF.begin(), mTriangleAreaCDF.end(), std::greater<float>());
            std::cout << "\n    min=" << *minmax.first << " max=" << *minmax.second
                      << " nonneg=" << (nonneg?"true":"false")
                      << " mono_inc=" << (mono_inc?"true":"false")
                      << " mono_dec=" << (mono_dec?"true":"false")
                      << std::endl;
            for (const auto& group : totalTriangleCount)
            {
                std::cout << "  Group " << (&group - &totalTriangleCount[0]) << ":";
                for (const auto& count : group)
                {
                    std::cout << " " << count;
                }
                std::cout << std::endl;
            }
            std::cout << "TotalTriangleCount: " << triangleCount[0] << " " << triangleCount[1] << std::endl;
        }
    }
    void BinFBX::RemoveMesh(uint32_t aGroup, uint32_t aLOD, uint32_t aIndex)
    {
        ///@todo If the material is no longer used, remove it.
        // Find the mesh
        auto it = std::find_if(mMeshes[aGroup].begin(), mMeshes[aGroup].end(), [&](const Mesh& i) 
            { 
                return (i.GetIndex() == aIndex) && (i.GetLOD() == aLOD); 
            });
        if(it == mMeshes[aGroup].end())
        {
            std::cout << "Mesh " << aGroup << "-" << aLOD << "-" << aIndex << " not found." << std::endl;
            return;
        }
        size_t index = it - mMeshes[aGroup].begin();
        mMeshes[aGroup].erase(it);

        mMaterialMaps[aGroup].erase(mMaterialMaps[aGroup].begin() + index);
        if(aGroup == 0 && mAlternateMaterialMaps.size() > index)
        {
            /// @todo Need to make sure which mesh group is the one alternate material map is pointing to
            mAlternateMaterialMaps.erase(mAlternateMaterialMaps.begin() + index);
        }
        // Keep trailer consistent with current meshes
        RecomputeTrailerFromMeshes();
    }
}
