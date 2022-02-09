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

#include "BinFBX.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <unordered_map>
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

        memcpy(mUnknown0.data(), &(*it), sizeof(int32_t) * mUnknown0.size());
        it += sizeof(int32_t) * mUnknown0.size();

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
        out.write(reinterpret_cast<const char*>(mUnknown0.data()), sizeof(int32_t) * mUnknown0.size());
        size = static_cast<uint32_t>(mUniformVariables.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(int32_t));
        for (const auto& i : mUniformVariables)
        {
            i.Write(out);
        }
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

        mUnknown0 = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);

        memcpy(mBoundingSphere.data(), &(*it), sizeof(int32_t) *mBoundingSphere.size());
        it += sizeof(int32_t) * mBoundingSphere.size();

        memcpy(mBoundingBox.data(), &(*it), sizeof(int32_t) *mBoundingBox.size());
        it += sizeof(int32_t) * mBoundingBox.size();

        mUnknown1 = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);

        uint8_t count = *it++;
        mAttributeInfos.reserve(count);
        for (uint8_t i = 0; i < count; ++i)
        {
            mAttributeInfos.emplace_back(it);
        }

        mUnknown2 = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown3 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        mUnknown4 = *it++;

        mUnknown5 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        // Build local buffers
        std::tuple<size_t, size_t> vertex_sizes = GetVertexSizes();
        std::unordered_map<size_t, size_t> index_to_local{};
        index_to_local.reserve(mIndexSize * mTriangleCount * 3);
        mVertexBuffers[0].reserve(mTriangleCount * 3 * std::get<0>(vertex_sizes));
        mVertexBuffers[1].reserve(mTriangleCount * 3 * std::get<1>(vertex_sizes));
        mIndexBuffer.resize(mIndexSize * mTriangleCount * 3);
        const uint8_t* index_data = aIndexBuffer.data() + mIndexBufferOffset;
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
                    index_to_local[index] = index_to_local.size();
                    mVertexBuffers[0].insert(mVertexBuffers[0].end(), aVertexBuffers[0].begin() + (std::get<0>(vertex_sizes) * index), aVertexBuffers[0].begin() + (std::get<0>(vertex_sizes) * index) + std::get<0>(vertex_sizes));
                    mVertexBuffers[1].insert(mVertexBuffers[1].end(), aVertexBuffers[1].begin() + (std::get<1>(vertex_sizes) * index), aVertexBuffers[1].begin() + (std::get<1>(vertex_sizes) * index) + std::get<1>(vertex_sizes));
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
            case AttributeType::R32G32B32_FLOAT:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 12;
            break;
                case AttributeType::B8G8R8A8_UNORM:
                case AttributeType::R8G8B8A8_UINT:
                case AttributeType::R16G16_SINT:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 4;
            break;
                case AttributeType::R16G16B16A16_SINT:
                case AttributeType::R16G16B16A16_UINT:
                *((i.Index) ? &std::get<0>(result) : &std::get<1>(result)) += 8;
            break;
            }
        }
        return result;
    }

    void Mesh::Write(std::ofstream& out) const
    {
        out.write(reinterpret_cast<const char*>(&mLOD), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mVertexCount), sizeof(uint32_t)); 
        out.write(reinterpret_cast<const char*>(&mTriangleCount), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mVertexBufferOffsets.data()), sizeof(uint32_t) * mVertexBufferOffsets.size());
        out.write(reinterpret_cast<const char*>(&mIndexBufferOffset), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mUnknown0), sizeof(int32_t));
        out.write(reinterpret_cast<const char*>(mBoundingSphere.data()), sizeof(int32_t) * mBoundingSphere.size());
        out.write(reinterpret_cast<const char*>(mBoundingBox.data()), sizeof(int32_t) * mBoundingBox.size());
        out.write(reinterpret_cast<const char*>(&mUnknown1), sizeof(int32_t));
        uint8_t count = static_cast<uint8_t>(mAttributeInfos.size());
        out.write(reinterpret_cast<const char*>(&count), 1);
        out.write(reinterpret_cast<const char*>(mAttributeInfos.data()), sizeof(decltype(mAttributeInfos)::value_type) * mAttributeInfos.size());
        out.write(reinterpret_cast<const char*>(&mUnknown2), sizeof(int32_t));
        out.write(reinterpret_cast<const char*>(&mUnknown3), sizeof(float));
        out.write(reinterpret_cast<const char*>(&mUnknown4), 1);
        out.write(reinterpret_cast<const char*>(&mUnknown5), sizeof(float));
    }

    void Mesh::Dump() const
    {
        std::cout << "Mesh" << std::endl;
        std::cout << "LOD: " << mLOD << std::endl;
        std::cout << "VertexCount: " << mVertexCount << std::endl;
        std::cout << "TriangleCount: " << mTriangleCount << std::endl;
        std::cout << "VertexBufferOffsets: ";
        for (const auto& i : mVertexBufferOffsets)
        {
            std::cout << i << " ";
        }
        std::cout << std::endl;
        std::cout << "IndexBufferOffset: " << mIndexBufferOffset << std::endl;
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

        // Unknown Block
        memcpy(mUnknown0.data(), &(*it), sizeof(int32_t) * mUnknown0.size());
        it += sizeof(int32_t) * mUnknown0.size();

        mUnknown1 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown2.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mUnknown2.emplace_back(*reinterpret_cast<const float*>(&(*it)));
            it += sizeof(float);
        }

        mUnknown3 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        memcpy(mUnknown4.data(), &(*it), sizeof(float) * mUnknown4.size());
        it += sizeof(float) * mUnknown4.size();

        mUnknown5 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        memcpy(mUnknown6.data(), &(*it), sizeof(float) * mUnknown6.size());
        it += sizeof(float) * mUnknown6.size();

        memcpy(mUnknown7.data(), &(*it), sizeof(float) * mUnknown7.size());
        it += sizeof(float) * mUnknown7.size();

        mUnknown8 = *reinterpret_cast<const uint32_t*>(&(*it));
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
            it += sizeof(float);
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

        // Unknowns
        mUnknown9 = *reinterpret_cast<const uint32_t*>(&(*it)); 
        it += sizeof(uint32_t);
        mUnknown10 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown11.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mUnknown11.emplace_back(*reinterpret_cast<const float*>(&(*it)));
            it += sizeof(float);
        }
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

        // Block Of Unknowns
        out.write(reinterpret_cast<const char*>(mUnknown0.data()), sizeof(decltype(mUnknown0)::value_type) * mUnknown0.size());
        out.write(reinterpret_cast<const char*>(&mUnknown1), sizeof(float));

        size = static_cast<uint32_t>(mUnknown2.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mUnknown2.data()), sizeof(decltype(mUnknown2)::value_type) * mUnknown2.size());

        out.write(reinterpret_cast<const char*>(&mUnknown3), sizeof(float));
        out.write(reinterpret_cast<const char*>(mUnknown4.data()), sizeof(decltype(mUnknown4)::value_type) * mUnknown4.size());
        out.write(reinterpret_cast<const char*>(&mUnknown5), sizeof(float));
        out.write(reinterpret_cast<const char*>(mUnknown6.data()), sizeof(decltype(mUnknown6)::value_type) * mUnknown6.size());
        out.write(reinterpret_cast<const char*>(mUnknown7.data()), sizeof(decltype(mUnknown7)::value_type) * mUnknown7.size());
        out.write(reinterpret_cast<const char*>(&mUnknown8), sizeof(uint32_t));

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

        out.write(reinterpret_cast<const char*>(&mUnknown9), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(&mUnknown10), sizeof(float));

        size = static_cast<float>(mUnknown11.size());
        out.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
        out.write(reinterpret_cast<const char*>(mUnknown11.data()), sizeof(float) * mUnknown11.size());

        out.close();
    }

    void BinFBX::Dump() const
    {
        for(auto &i: mMeshes)
        {
            for(auto &j: i)
            {
                j.Dump();
            }
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
    }
}
