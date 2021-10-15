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
}
#endif
