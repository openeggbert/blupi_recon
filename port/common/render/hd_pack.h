// A pack of higher-resolution images (port/docs/HD.md): a folder with the game's image folders and
// file names (IMAGE08/OBJECT.BLP, IMAGE16/DECOR000.BLP, Image/...), any case, each image a BMP (.blp
// or .bmp) exactly `scale` times the original's width and height. No SDL, no Win32 names.
#ifndef BLUPI_RENDER_HD_PACK_H
#define BLUPI_RENDER_HD_PACK_H

#include "render/bmp.h"

#include <string>

namespace blupi::render
{
enum class PackResult { Loaded, Missing, WrongSize, Unreadable };

// The pack's image for the game image `gamePath` (as the game names it, "image16\\object.blp"),
// decoded; it must be scale x origWidth by scale x origHeight. `found` names the file when there is one.
PackResult load_pack_image(const std::string& root, const std::string& gamePath, int scale, int origWidth,
                           int origHeight, Bitmap& out, std::string& found);
}  // namespace blupi::render

#endif
