#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>

// clang-format off
#define STBI_MALLOC(sz)         PLY_HEAP.alloc(sz)
#define STBI_REALLOC(p, newsz)  PLY_HEAP.realloc(p, newsz)
#define STBI_FREE(p)            PLY_HEAP.free(p)
// clang-format om

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb/stb_image.h"

namespace flap {

void premultiplySRGB(image::Image& dst) {
    PLY_ASSERT(dst.stride >= dst.width * 4);
    char* dstRow = dst.data;
    char* dstRowEnd = dstRow + dst.stride * dst.height;
    while (dstRow < dstRowEnd) {
        u32* d = (u32*) dstRow;
        u32* dEnd = d + dst.width;
        while (d < dEnd) {
            Float4 orig = ((Int4<u8>*) d)->to<Float4>() * (1.f / 255.f);
            Float3 linearPremultipliedColor = fromSRGB(orig.asFloat3()) * orig.a();
            Float4 result = {toSRGB(linearPremultipliedColor), 1.f - orig.a()};
            *(Int4<u8>*) d = (result * 255.f + 0.5f).to<Int4<u8>>();
            d++;
        }
        dstRow += dst.stride;
    }
}

image::OwnImage loadPNG(StringView src, bool premultiply) {
    s32 w = 0;
    s32 h = 0;
    s32 numChannels = 0;
    stbi_set_flip_vertically_on_load(true);
    u8* data = stbi_load_from_memory((const stbi_uc*) src.bytes, src.numBytes, &w, &h, &numChannels, 0);
    if (!data)
        return {};

    image::Format fmt = image::Format::Byte;
    if (numChannels == 4) {
        fmt = image::Format::RGBA;
    } else if (numChannels != 1) {
        PLY_ASSERT(0);
    }
    u8 bytespp = image::Image::FormatToBPP[(u32) fmt];

    image::OwnImage result;
    result.data = (char*) data;
    result.stride = w * bytespp;
    result.width = w;
    result.height = h;
    result.bytespp = bytespp;
    result.format = fmt;

    if (premultiply && numChannels == 4) {
        premultiplySRGB(result);
    }
    return result;
}

} // namespace ply
