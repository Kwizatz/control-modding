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
namespace ControlModding
{
    BinFBX::BinFBX(const std::vector<uint8_t>& aBuffer)
    {
        const Header* header = reinterpret_cast<const Header*>(aBuffer.data());
        if (header->Magick != BinFBXMagick)
        {
            throw std::runtime_error("Invalid BinFBX file.");
        }
        mIndexSize = header->IndexSize;
        auto it = aBuffer.begin() + sizeof(Header);
        mVertexBuffers[0] = std::vector<uint8_t>(it, it + header->VertexBufferSizes[0]);
        it += header->VertexBufferSizes[0];
        mVertexBuffers[1] = std::vector<uint8_t>(it, it + header->VertexBufferSizes[1]);
        it += header->VertexBufferSizes[1];
        mIndexBuffer      = std::vector<uint8_t>(it, it + (header->IndexCount * mIndexSize));
        it += header->IndexCount * mIndexSize;
    }
}