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

    UniformVariable::UniformVariable(std::vector<uint8_t>::const_iterator& it)
    {
        // IOU an implementation
    }
    Material::Material(std::vector<uint8_t>::const_iterator& it)
    {
        // IOU an implementation
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
        for (int32_t i = 0; i < count; ++i)
        {
            mJoints.emplace_back(it);
        }
    }
}
