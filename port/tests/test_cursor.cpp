// MouseType 2 and 3 (platform/cursor.h): the original's cursor resources, decoded from the copies
// compiled into the program. The expected sizes, hotspots and pixel counts were read from
// src/res/*.cur with an independent decoder (Python, 2026-11-13): black and white are AND-mask-0
// pixels of colour index 0 and 1, transparent the AND-mask-1 pixels; none of the files has an
// inverting pixel. The arrow's first rows are compared pixel by pixel.
#include "platform/cursor.h"

#include <cstdio>
#include <cstring>
#include <vector>

using blupi::host::CursorImage;
using blupi::host::GameCursor;

static int failures = 0;
static void check(bool ok, const char* what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

int main()
{
    struct Expect { GameCursor cursor; const char* file; int hotX, hotY, black, white, transparent; };
    const Expect expected[] = {
        {GameCursor::Arrow,   "arrow.cur",    0,  0,  47,  58, 919},
        {GameCursor::Pointer, "cursor1.cur",  0,  0, 105,  97, 822},
        {GameCursor::ArrowU,  "arrowup.cur", 12,  0,  62, 106, 856},
        {GameCursor::ArrowD,  "arrowdow.cur",12, 13,  62, 106, 856},
        {GameCursor::ArrowL,  "arrowlef.cur", 0, 12,  62, 106, 856},
        {GameCursor::ArrowR,  "arrowrig.cur",13, 12,  62, 106, 856},
        {GameCursor::ArrowUL, "arrowul.cur",  0,  0,  62, 103, 859},
        {GameCursor::ArrowUR, "arrowur.cur", 31,  0,  62, 103, 859},
        {GameCursor::ArrowDL, "arrowdl.cur",  0, 31,  62, 103, 859},
        {GameCursor::ArrowDR, "arrowdr.cur", 31, 31,  62, 103, 859},
        {GameCursor::Wait,    "wait.cur",    15, 17, 225, 374, 425},
        {GameCursor::Empty,   "empty.cur",   15, 17,   0,   0, 1024},
    };
    for (const Expect& e : expected)
    {
        const CursorImage& image = blupi::host::game_cursor_image(e.cursor);
        int black = 0, white = 0, transparent = 0, other = 0;
        for (uint32_t p : image.argb)
        {
            if (p == 0xff000000u) black++;
            else if (p == 0xffffffffu) white++;
            else if (p >> 24 == 0) transparent++;
            else other++;
        }
        char what[160];
        snprintf(what, sizeof(what), "%s: 32x32, hotspot %d,%d, %d black, %d white, %d transparent",
                 e.file, e.hotX, e.hotY, e.black, e.white, e.transparent);
        check(std::strcmp(blupi::host::game_cursor_file(e.cursor), e.file) == 0 && image.width == 32 &&
              image.height == 32 && image.hotX == e.hotX && image.hotY == e.hotY && black == e.black &&
              white == e.white && transparent == e.transparent && other == 0, what);
    }

    // The arrow's top rows: '#' black, 'o' white, '.' transparent (the file's first rows, top-down).
    const char* rows[] = {"#...", "##..", "#o#.", "#oo#", "#ooo"};
    const CursorImage& arrow = blupi::host::game_cursor_image(GameCursor::Arrow);
    bool same = arrow.argb.size() == 32 * 32;
    for (int y = 0; same && y < 5; y++)
        for (int x = 0; same && x < 4; x++)
        {
            const uint32_t p = arrow.argb[(size_t)y * 32 + x];
            same = rows[y][x] == '#' ? p == 0xff000000u : rows[y][x] == 'o' ? p == 0xffffffffu : p >> 24 == 0;
        }
    check(same, "arrow.cur: the top-left corner of the arrow, pixel by pixel");

    const CursorImage big = blupi::host::scale_cursor(arrow, 2);
    check(big.width == 64 && big.height == 64 && big.hotX == 0 && big.hotY == 0 &&
          big.argb[0] == arrow.argb[0] && big.argb[65] == arrow.argb[0] && big.argb[2 * 64 + 2] == arrow.argb[33] && big.argb[3 * 64 + 3] == arrow.argb[33],
          "scale 2: every pixel twice in each direction");
    const CursorImage& wait = blupi::host::game_cursor_image(GameCursor::Wait);
    const CursorImage wait3 = blupi::host::scale_cursor(wait, 3);
    check(wait3.width == 96 && wait3.hotX == 45 && wait3.hotY == 51, "scale 3: the hotspot is scaled too");

    CursorImage none;
    const unsigned char bad[22] = {0, 0, 2, 0, 1, 0};
    check(!blupi::host::decode_cur(bad, sizeof(bad), none) && none.argb.empty(), "a truncated file is refused");
    printf("%s\n", failures == 0 ? "all checks passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}
