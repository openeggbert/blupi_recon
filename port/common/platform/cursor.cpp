#include "platform/cursor.h"

#include <array>

namespace blupi::host
{
namespace
{
// src/res/*.cur, embedded by CMakeLists.txt (cmake/embed_bytes.cmake).
const uint8_t k_arrow[]    = {
#include "cursor_arrow.inc"
};
const uint8_t k_pointer[]  = {
#include "cursor_cursor1.inc"
};
const uint8_t k_arrowU[]   = {
#include "cursor_arrowup.inc"
};
const uint8_t k_arrowD[]   = {
#include "cursor_arrowdow.inc"
};
const uint8_t k_arrowL[]   = {
#include "cursor_arrowlef.inc"
};
const uint8_t k_arrowR[]   = {
#include "cursor_arrowrig.inc"
};
const uint8_t k_arrowUL[]  = {
#include "cursor_arrowul.inc"
};
const uint8_t k_arrowUR[]  = {
#include "cursor_arrowur.inc"
};
const uint8_t k_arrowDL[]  = {
#include "cursor_arrowdl.inc"
};
const uint8_t k_arrowDR[]  = {
#include "cursor_arrowdr.inc"
};
const uint8_t k_wait[]     = {
#include "cursor_wait.inc"
};
const uint8_t k_empty[]    = {
#include "cursor_empty.inc"
};

struct Resource { const char* file; const uint8_t* data; size_t size; };
#define BLUPI_CURSOR(file, bytes) Resource{file, bytes, sizeof(bytes)}
const std::array<Resource, (size_t)GameCursor::Count> k_resources = {
    BLUPI_CURSOR("arrow.cur", k_arrow),     BLUPI_CURSOR("cursor1.cur", k_pointer),
    BLUPI_CURSOR("arrowup.cur", k_arrowU),  BLUPI_CURSOR("arrowdow.cur", k_arrowD),
    BLUPI_CURSOR("arrowlef.cur", k_arrowL), BLUPI_CURSOR("arrowrig.cur", k_arrowR),
    BLUPI_CURSOR("arrowul.cur", k_arrowUL), BLUPI_CURSOR("arrowur.cur", k_arrowUR),
    BLUPI_CURSOR("arrowdl.cur", k_arrowDL), BLUPI_CURSOR("arrowdr.cur", k_arrowDR),
    BLUPI_CURSOR("wait.cur", k_wait),       BLUPI_CURSOR("empty.cur", k_empty),
};
#undef BLUPI_CURSOR

uint32_t u16(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t u32(const uint8_t* p) { return u16(p) | u16(p + 2) << 16; }
}  // namespace

bool decode_cur(const uint8_t* data, size_t size, CursorImage& out)
{
    out = CursorImage();
    if (data == nullptr || size < 6 + 16 || u16(data) != 0) return false;
    const uint32_t type = u16(data + 2), count = u16(data + 4);
    if ((type != 1 && type != 2) || count == 0) return false;
    const uint8_t* entry = data + 6;
    const uint32_t offset = u32(entry + 12), length = u32(entry + 8);
    if (offset >= size || length > size - offset || length < 40) return false;
    const uint8_t* dib = data + offset;
    const uint32_t header = u32(dib);
    const int32_t width = (int32_t)u32(dib + 4), doubled = (int32_t)u32(dib + 8);
    const uint32_t bpp = u16(dib + 14), compression = u32(dib + 16);
    if (header < 40 || header > length || width <= 0 || width > 256 || doubled <= 0 || doubled % 2 ||
        doubled / 2 > 256 || compression != 0 || (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32))
        return false;
    const int height = doubled / 2;
    uint32_t colours = bpp <= 8 ? u32(dib + 32) : 0;
    if (bpp <= 8 && (colours == 0 || colours > (1u << bpp))) colours = 1u << bpp;
    const size_t table = header, image = table + (size_t)colours * 4;
    const size_t stride = (((size_t)width * bpp + 31) / 32) * 4, maskStride = (((size_t)width + 31) / 32) * 4;
    const size_t mask = image + stride * (size_t)height;
    if (mask + maskStride * (size_t)height > length) return false;

    out.width = width;
    out.height = height;
    out.hotX = type == 2 ? (int)u16(entry + 4) : 0;
    out.hotY = type == 2 ? (int)u16(entry + 6) : 0;
    out.argb.assign((size_t)width * height, 0);
    bool anyAlpha = false;
    if (bpp == 32)
        for (int y = 0; y < height && !anyAlpha; y++)
            for (int x = 0; x < width && !anyAlpha; x++) anyAlpha = dib[image + stride * y + x * 4 + 3] != 0;
    for (int y = 0; y < height; y++)
    {
        const uint8_t* row = dib + image + stride * (size_t)(height - 1 - y);   // bottom-up
        const uint8_t* andRow = dib + mask + maskStride * (size_t)(height - 1 - y);
        for (int x = 0; x < width; x++)
        {
            const bool transparent = (andRow[x / 8] >> (7 - x % 8)) & 1;
            uint32_t rgb = 0, value = 0;
            uint32_t alpha = 0xff;
            if (bpp <= 8)
            {
                const size_t bit = (size_t)x * bpp;
                value = (row[bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
                if (value >= colours) value = 0;
                const uint8_t* c = dib + table + value * 4;   // blue, green, red, reserved
                rgb = uint32_t(c[2]) << 16 | uint32_t(c[1]) << 8 | c[0];
            }
            else
            {
                const uint8_t* c = row + (size_t)x * (bpp / 8);
                rgb = uint32_t(c[2]) << 16 | uint32_t(c[1]) << 8 | c[0];
                value = rgb;
                if (bpp == 32 && anyAlpha) alpha = c[3];
            }
            uint32_t pixel;
            if (bpp == 32 && anyAlpha) pixel = alpha << 24 | rgb;
            else if (!transparent) pixel = 0xff000000u | rgb;
            else pixel = value != 0 ? 0xff000000u : 0;   // an inverting pixel: black
            out.argb[(size_t)y * width + x] = pixel;
        }
    }
    return true;
}

const CursorImage& game_cursor_image(GameCursor cursor)
{
    static std::array<CursorImage, (size_t)GameCursor::Count> images;
    static std::array<bool, (size_t)GameCursor::Count> decoded{};
    const size_t i = (size_t)cursor;
    static const CursorImage none;
    if (i >= images.size()) return none;
    if (!decoded[i])
    {
        decode_cur(k_resources[i].data, k_resources[i].size, images[i]);
        decoded[i] = true;
    }
    return images[i];
}

const char* game_cursor_file(GameCursor cursor)
{
    const size_t i = (size_t)cursor;
    return i < k_resources.size() ? k_resources[i].file : "";
}

CursorImage scale_cursor(const CursorImage& image, int scale)
{
    if (scale <= 1) return image;
    CursorImage out;
    out.width = image.width * scale;
    out.height = image.height * scale;
    out.hotX = image.hotX * scale;
    out.hotY = image.hotY * scale;
    out.argb.resize((size_t)out.width * out.height);
    for (int y = 0; y < out.height; y++)
        for (int x = 0; x < out.width; x++)
            out.argb[(size_t)y * out.width + x] = image.argb[(size_t)(y / scale) * image.width + x / scale];
    return out;
}
}  // namespace blupi::host
