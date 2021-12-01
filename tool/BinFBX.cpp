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

#include "BinFBX.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <fstream>
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
            mData = std::array<float,2>{*reinterpret_cast<const float*>(&(*it)), *reinterpret_cast<const float*>(&(*it)+sizeof(float))};
            it += sizeof(float) * 2;
            break;
        case Color:
            mData = std::array<float,4>{*reinterpret_cast<const float*>(&(*it)), *reinterpret_cast<const float*>(&(*it)+sizeof(float))};
            it += sizeof(float) * 4;
            break;
        case Vector:
            mData = std::array<float,3>{*reinterpret_cast<const float*>(&(*it)), *reinterpret_cast<const float*>(&(*it)+sizeof(float))};
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

    Mesh::Mesh(std::vector<uint8_t>::const_iterator& it)
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
        mMaterialMap.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mMaterialMap.emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
            it += sizeof(uint32_t);
        }

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mAlternaleMaterialMaps.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            std::string name =
            {
                reinterpret_cast<const std::string::value_type*>(&(*it)+sizeof(int32_t)),
                static_cast<std::string::size_type>(*reinterpret_cast<const int32_t*>(&(*it)))
            };
            it += sizeof(int32_t) + *reinterpret_cast<const int32_t*>(&(*it));

            int32_t c = *reinterpret_cast<const int32_t*>(&(*it));
            it += sizeof(int32_t);
            std::vector<uint32_t> v;
            v.reserve(c);
            for (int32_t j = 0; j < c; ++j)
            {
                v.emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
                it += sizeof(uint32_t);
            }
            mAlternaleMaterialMaps.emplace_back(std::move(name), std::move(v));
        }

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown9.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mUnknown9.emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
            it += sizeof(float);
        }

        // Meshes
        for(auto& m : mMeshes)
        {
            count = *reinterpret_cast<const int32_t*>(&(*it));
            it += sizeof(int32_t);
            m.reserve(count);
            for (int32_t i = 0; i < count; ++i)
            {
                m.emplace_back(it);
            }
        }

        // Unknowns
        mUnknown10 = *reinterpret_cast<const uint32_t*>(&(*it)); // This is always 0, so it could be really an array
        it += sizeof(uint32_t);

        mUnknown11 = *reinterpret_cast<const float*>(&(*it));
        it += sizeof(float);

        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mUnknown12.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mUnknown12.emplace_back(*reinterpret_cast<const uint32_t*>(&(*it)));
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
        out.close();
    }
}
