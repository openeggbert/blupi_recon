// The original programs' Windows cursors, for MouseType 2 and 3 (the game's ChangeSprite loads
// IDC_ARROW ... IDC_EMPTY from its resources). The files are the historical resources
// src/res/*.cur (Speedy Blupi 2's resource scripts name the same files), compiled into the program
// at build time. No Win32 names here.
#ifndef BLUPI_PLATFORM_CURSOR_H
#define BLUPI_PLATFORM_CURSOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace blupi::host
{
// In the order of the original ChangeSprite's LoadCursor calls (SPRITE_ARROW 1 ... SPRITE_EMPTY 12).
enum class GameCursor { Arrow, Pointer, ArrowU, ArrowD, ArrowL, ArrowR, ArrowUL, ArrowUR, ArrowDL, ArrowDR, Wait, Empty, Count };

// A decoded cursor: 0xAARRGGBB pixels, top row first.
struct CursorImage
{
    int width = 0, height = 0, hotX = 0, hotY = 0;
    std::vector<uint32_t> argb;
};

// Decodes the first image of a Windows .cur (or .ico, hotspot 0,0) file: 1, 4 and 8 bits per pixel
// with their colour table and AND mask, 24 bits with the mask, 32 bits with alpha (or the mask when
// the alpha is all zero). A pixel the AND mask makes transparent but the image sets would invert the
// screen under Windows; it is drawn opaque black (none of the games' cursors has one).
bool decode_cur(const uint8_t* data, size_t size, CursorImage& out);

// The resource the original loaded for `cursor`, decoded once (an empty image if it failed), and the
// name of its file in src/res.
const CursorImage& game_cursor_image(GameCursor cursor);
const char* game_cursor_file(GameCursor cursor);

// `image` enlarged `scale` times by pixel repetition, its hotspot with it.
CursorImage scale_cursor(const CursorImage& image, int scale);
}  // namespace blupi::host

#endif
