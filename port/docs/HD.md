# Higher-resolution images (after 1.5.2)

The port can show the games at two to four times their resolution, with images from a pack of
higher-resolution images, for example the 2x and 4x sheets re-rendered from the original 3D models
(`speedyblupi-hd`). The games still compute in their 640x480 pixels: the simulation, the traces, the
goldens and everything the game reads back from its images are those of the faithful frame. Only the
presentation is larger.

## Using it

```bash
blupi-sdl3 --hd-pack <pack>/2x                  # scale 2, the pack's images where it has them
blupi-sdl3 --hd-scale 4 --hd-pack <pack>/4x     # scale 4
blupi-sdl3 --hd-scale 2                         # scale 2 without a pack: the originals enlarged
```

`blupi2-sdl3` and the edition programs take the same options. The settings file
(`blupi-sdl3.json`, ...) keeps them as `hdScale` (1 to 4; 1 is off) and `hdPack`. The window is then
best `--scale` times as large as the scale, or full screen.

**Display options:** the screen has an **Images** setting at the right of its big panel: Original, 2x,
3x or 4x. It is saved at once and used from the next start ("next start" shows while it differs from
the running one). The pack folder stays in the settings file (`hdPack`) or on the command line. Touch
screens do not show the setting.

**What a pack holds:** the game's image folders and file names (`IMAGE08/OBJECT.BLP`,
`IMAGE16/DECOR000.BLP`, the June 1998 demo's `IMAGE/...`), in any case, as uncompressed BMP files
(`.blp` or `.bmp`, 8 bits with a colour table or 24 bits), each exactly `scale` times the original's width
and height. An image of another size, or one that is not such a BMP, is logged once and the original is
shown enlarged; a missing image is the original enlarged. The colour keys are the game's own: the sprite
sheets' transparent colour is pure blue, and in 16-bit mode every colour with R < 8, G < 4 and B >= 248
counts as that blue. In 8-bit mode the images are reduced to the game's palette as the originals are.
The pack layout is the same as for the Free API builds (`freeapi/README.md`), but the port does not read
alpha channels (32-bit BMP) or PNG files.

## How it works

`common/render/scaled.*`: CPixmap's commands (`common/render/device.h`) go first to the reference device,
unchanged, then are mirrored on surfaces `scale` times larger, with every rectangle `scale` times larger.

- **An image upload** takes the pack's image when CPixmap offered one while loading that file
  (`offer_pack_image` in each game's `pixmap.cpp`, `common/render/hd_pack.*`), converted to the display
  format exactly as the original image is (`convert_bitmap`); else the original, enlarged by pixel
  repetition.
- **A stretch** (the editor's minimap) is done by the reference device and its result enlarged.
- **What is presented:** the mirrored screen or wide view (`copy_presented`); the host's texture is
  `scale` times the view (`host::set_frame_scale`), and the mouse and layout stay in 640x480 coordinates.
  Screenshots (`--screenshot`) show what is presented. The traces' screen hash and the display capture
  stay on the faithful frame.

Since every mirrored command is the faithful one scaled, without a pack (or with a pack of enlarged
originals) every mirrored frame is exactly the enlargement of the faithful frame. `--hd-check` checks this
at every Display of the back buffer and the screen.

## Tests

`ctest -R '^hd_'` (`tests/hd_check.cmake`, with game data):

| Test | Run | Pack |
|---|---|---|
| `hd_sb1_tour16_x3` | Speedy Blupi, `tests/golden/tour.input`, 16 bpp, scale 3 | none |
| `hd_sb1_editor8_x2` | the editor session (minimap stretch), 8 bpp, scale 2 | none |
| `hd_sb1_pack16_x2`, `hd_sb1_pack8_x2` | the attract mode, 1,500 frames, scale 2 | synthetic |
| `hd_sb2_tour16_x2` | Speedy Eggbert 2, `tests/golden/sb2_tour.input`, scale 2 | none |
| `hd_sb2_pack16_x2` | its attract mode, 1,500 frames, scale 2 | synthetic |

The synthetic packs (`tools/make_hd_test_pack.py`, made once per build tree) hold the images those runs
load, enlarged by pixel repetition: every frame must still be the enlargement, now with the images from
the pack (17 for each game). All pass (2026-11-13).

[EVIDENCE] Real packs, 2026-11-13 (`speedyblupi-hd`, `games/<game>/<N>x`): Speedy Blupi's 2x pack (17
images from the pack, none refused) and 4x pack (none refused; 1,300 frames in 17 s headless, 0.3 GB of
memory), and Speedy Eggbert 2's 2x pack, shown in the attract mode and on the title screen; a windowed
Speedy Blupi at scale 2 under Xvfb presented the pack's detail. Some tiles of those packs are enlargements
of the dithered originals (the pack's "fallback" icons, `manifest/build_games.csv`), and look so.

**Not done:** choosing the pack folder on the Display options screen, packs on Android and in the
browser (the code is there, but neither has a way to bring a pack in yet), alpha channels.
