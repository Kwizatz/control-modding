#ifndef CONTROL_MODDING_BINFBX_H
#define CONTROL_MODDING_BINFBX_H

#include <cstdint>

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
        uint32_t AttributeBufferSize;
        uint32_t VertexBufferSize;
        uint32_t IndexCount;
        uint32_t IndexSize;
        uint8_t  BuffersStart[4]; // This is not a real field, it is just a hack to access the buffer directy without pointer math.
    };
    struct AttributeInfo{
        uint8_t Zero; // Always 0?
        uint8_t BufferLocation; // 0x0 = AttributeBuffer, 0x1 = VertexBuffer, Something else entirely for the last set of 4 bytes
        uint8_t Type; // 0x4 = B8G8R8A8_UNORM, 0x7 = R16G16_SINT, 0x8 = R16G16B16A16_SINT, 0xd = R16G16B16A16_UINT, 0x5 =  R8G8B8A8_UINT
        uint8_t Usage; // 0x1 = Normal, 0x2 = TexCoord, 0x3 = Tangent, 0x5 = Index, 0x6 = Weight
    };
}
#endif
