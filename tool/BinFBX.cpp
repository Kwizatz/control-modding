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

    BinFBX::BinFBX(const std::vector<uint8_t>& aBuffer)
    {
        const Header* header = reinterpret_cast<const Header*>(aBuffer.data());
        if (header->Magick != BinFBXMagick)
        {
            throw std::runtime_error("Invalid BinFBX file.");
        }
        mIndexSize = header->IndexSize;
        std::vector<uint8_t>::const_iterator it = aBuffer.begin() + sizeof(Header);
        mVertexBuffers[0] = std::vector<uint8_t>(it, it += header->VertexBufferSizes[0]);
        mVertexBuffers[1] = std::vector<uint8_t>(it, it += header->VertexBufferSizes[1]);
        mIndexBuffer      = std::vector<uint8_t>(it, it += (header->IndexCount * mIndexSize));
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

        mUnknown8 = *reinterpret_cast<const float*>(&(*it));

        // Materials
        it += sizeof(uint32_t);
        count = *reinterpret_cast<const int32_t*>(&(*it));
        it += sizeof(int32_t);
        mMaterials.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            mMaterials.emplace_back(it);
        }
    }
}
