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