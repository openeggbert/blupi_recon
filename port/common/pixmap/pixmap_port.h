// CPixmap's port support shared by both games' pixmap.cpp (port/docs/ARCHITECTURE.md;
// docs/CONSOLIDATION_PLAN.md, stage 4 rank 4): the rendering devices (reference, command check,
// capture, higher-resolution mirror), the DirectDraw registry entries, the blit and colour-key
// helpers, and the image and palette loaders (DDLoadBitmap, DDLoadPalette). Until 2026-11-13 each
// game had an identical copy.
//
// Included once by games/sb1/game/pixmap.cpp and games/sb2/game/pixmap.cpp after their includes, so it
// is compiled in each game's own context (its def.h and pixmap.h); everything here is static to that
// file. CPixmap's policy (which images, clipping, the icon tables) stays in the game's file.
#ifndef BLUPI_PIXMAP_PORT_H
#define BLUPI_PIXMAP_PORT_H

#include "render/bmp.h"
#include "render/device.h"
#include "render/display_capture.h"
#include "render/hd_pack.h"
#include "render/scaled.h"
#include "render/surface.h"

#include <cstring>
#include <memory>
#include <unordered_map>

using blupi::render::Surface;
using blupi::render::Rgb;
using blupi::render::SurfaceId;

// Rendering devices: the reference (software) device always exists; in
// command-check mode a recorder sits in front of it and every frame's
// commands are replayed on a second, independent device (ROADMAP.md stage 1).
static blupi::render::SoftwareDevice  s_reference;
static blupi::render::Device*         s_device = &s_reference;
static blupi::render::Recorder*       s_recorder = NULL;
static blupi::render::SoftwareDevice* s_shadow = NULL;
static long s_checkFrames = 0;
static long s_checkMismatches = 0;
static long s_checkFirstMismatch = -1;

static void submit(const blupi::render::Command& command)
{
	s_device->submit(command);
}

// Higher-resolution images (render/scaled.h, port/docs/HD.md): the commands mirrored at --hd-scale,
// with the pack's images where it has them. Presentation only; the game and its traces use
// s_reference as before.
static blupi::render::ScaledDevice* s_hd = NULL;
static std::string s_hdPack;
static bool s_hdCheck = false;
static long s_hdLoaded = 0, s_hdCheckFrames = 0, s_hdCheckMismatches = 0, s_hdFirstMismatch = -1;

bool PortPixmapEnableHd(int scale, const std::string& pack, bool check)
{
	if ( scale < 2 || s_hd != NULL )  return false;
	s_hd = new blupi::render::ScaledDevice(s_device, &s_reference, scale);
	s_device = s_hd;
	s_hdPack = pack;
	s_hdCheck = check;
	return true;
}

int PortPixmapHdScale()
{
	return s_hd != NULL ? s_hd->scale() : 1;
}

// --hd-check (test): at every Display, the mirrored back buffer and screen must be exactly the
// enlargement of the faithful ones (true without a pack, or with a pack of enlarged originals).
static void PortHdCheck()
{
	if ( s_hd == NULL || !s_hdCheck )  return;
	const long n = s_hd->mismatches(blupi::render::kBackBuffer) + s_hd->mismatches(blupi::render::kScreen);
	if ( n != 0 )
	{
		if ( s_hdFirstMismatch < 0 )  s_hdFirstMismatch = s_hdCheckFrames;
		s_hdCheckMismatches++;
	}
	s_hdCheckFrames++;
}

bool PortPixmapHdCheckResult(long& frames, long& mismatches, long& firstMismatch, long& loaded)
{
	frames = s_hdCheckFrames;
	mismatches = s_hdCheckMismatches;
	firstMismatch = s_hdFirstMismatch;
	loaded = s_hdLoaded;
	return s_hd != NULL && s_hdCheck;
}

struct IDirectDraw
{
};

// Registry entry of one surface. Creating and deleting it creates and
// destroys the device surface.
struct IDirectDrawSurface
{
	IDirectDrawSurface(SurfaceId surfaceId, int w, int h, int depth)
		: id(surfaceId), width(w), height(h), bpp(depth)
	{
		submit(blupi::render::make_create(id, width, height, bpp));
	}
	~IDirectDrawSurface()
	{
		submit(blupi::render::make_destroy(id));
	}

	SurfaceId id;
	int       width, height, bpp;
	blupi::render::ColorKey key;              // DDCKEY_SRCBLT in force
	std::shared_ptr<const Surface> image;     // pixels as loaded (CPU queries)
};

struct IDirectDrawPalette
{
	blupi::render::Palette entries;
};

struct IDirectDrawClipper
{
};

// DirectDraw results used by the historical method bodies.
#define DD_OK         ((HRESULT)0)
#define DDERR_GENERIC ((HRESULT)0x80004005L)

// Shared cross-build oracle (host paths, independent of the game VFS).
static blupi::render::DisplayCapture s_displayCapture;
void PortPixmapSetDisplayCapture(FILE* log, const char* dumpDir, long dumpEvery)
{
    s_displayCapture.configure(log, dumpDir, dumpEvery);
}

// Command check: replays the commands recorded since the last Display on the
// shadow device and compares the back buffer, the screen and the palette.
static bool same_pixels(const Surface* a, const Surface* b)
{
	if ( a == NULL || b == NULL )  return a == b;
	if ( a->width() != b->width() || a->height() != b->height() || a->bpp() != b->bpp() )  return false;
	const size_t rowBytes = (size_t)a->width() * (a->bpp() / 8);
	for ( int y=0 ; y<a->height() ; y++ )
	{
		if ( memcmp(a->row(y), b->row(y), rowBytes) != 0 )  return false;
	}
	return true;
}

static void PortCommandCheck()
{
	if ( s_recorder == NULL )  return;
	blupi::render::replay(s_recorder->commands(), *s_shadow);
	s_recorder->commands().clear();
	bool same = same_pixels(s_reference.surface(blupi::render::kBackBuffer), s_shadow->surface(blupi::render::kBackBuffer)) &&
				same_pixels(s_reference.surface(blupi::render::kScreen), s_shadow->surface(blupi::render::kScreen)) &&
				memcmp(s_reference.palette(), s_shadow->palette(), sizeof(blupi::render::Palette)) == 0;
	if ( !same )
	{
		if ( s_checkFirstMismatch < 0 )  s_checkFirstMismatch = s_checkFrames;
		s_checkMismatches++;
	}
	s_checkFrames++;
}

void PortPixmapEnableCommandCheck()
{
	if ( s_recorder != NULL )  return;
	s_recorder = new blupi::render::Recorder(s_device);
	s_shadow = new blupi::render::SoftwareDevice;
	s_device = s_recorder;
}

bool PortPixmapCommandCheckResult(long& frames, long& mismatches, long& firstMismatch)
{
	frames = s_checkFrames;
	mismatches = s_checkMismatches;
	firstMismatch = s_checkFirstMismatch;
	return s_recorder != NULL;
}

// Command capture (scene gate, ROADMAP.md stage 3): commands submitted between
// Begin and End are collected; with execute=false they are not drawn.
struct CaptureFrame
{
	blupi::render::Device*   previous;
	blupi::render::Recorder* recorder;
};
static std::vector<CaptureFrame> s_captures;

void PortPixmapCaptureBegin(bool execute)
{
	CaptureFrame frame = { s_device, new blupi::render::Recorder(execute ? s_device : NULL) };
	s_captures.push_back(frame);
	s_device = frame.recorder;
}

void PortPixmapCaptureEnd(std::vector<blupi::render::Command>& out)
{
	CaptureFrame frame = s_captures.back();
	s_captures.pop_back();
	out = std::move(frame.recorder->commands());
	s_device = frame.previous;
	delete frame.recorder;
}

// Replays captured commands (native frame rendering, ROADMAP.md stage 4).
void PortPixmapSubmit(const std::vector<blupi::render::Command>& commands)
{
	for ( const blupi::render::Command& c : commands )  submit(c);
}

// The one screen and its palette, for presentation by the host loop.
static IDirectDrawSurface* s_primary = NULL;
static bool s_dirty = false;
static blupi::render::Palette s_defaultPalette;

// Display-rate presentation (ROADMAP.md stage 5): the back buffer to the
// screen, without the per-Display instrumentation (command check, oracle log),
// which belongs to the game's own Display calls.
static IDirectDrawSurface* s_back = NULL;

void PortPixmapPresentBack()
{
	if ( s_primary == NULL || s_back == NULL )  return;
	submit(blupi::render::make_blit(s_primary->id, 0, 0, s_back->id,
									blupi::render::Rect{0, 0, s_back->width, s_back->height},
									blupi::render::ColorKey()));
	s_dirty = true;
}


static const blupi::render::Palette& palette_of(IDirectDrawPalette* pal)
{
	return pal ? pal->entries : s_defaultPalette;
}

static void submit_palette(IDirectDrawPalette* pal)
{
	submit(blupi::render::make_palette(palette_of(pal)));
}

static Rgb rgb_of(COLORREF color)
{
	return Rgb{GetRValue(color), GetGValue(color), GetBValue(color)};
}

static blupi::render::Rect rect_of(const RECT& r)
{
	return blupi::render::Rect{(int)r.left, (int)r.top, (int)r.right, (int)r.bottom};
}

// Pixel value of a colour in a surface format (DDColorMatch through GDI).
static uint32_t native_of(int bpp, const blupi::render::Palette& pal, Rgb c)
{
	return bpp == 8 ? blupi::render::nearest_index(pal, c) : blupi::render::rgb_to_565(c);
}

// IDirectDrawSurface::BltFast: invalid rectangles draw nothing and fail.
static HRESULT blt_fast(IDirectDrawSurface* dst, int x, int y, IDirectDrawSurface* src,
						const RECT& rcRect, BOOL bColorKey)
{
	if ( dst == NULL || src == NULL )  return DDERR_GENERIC;
	blupi::render::Rect r = rect_of(rcRect);
	if ( !blupi::render::blit_valid(dst->width, dst->height, dst->bpp, x, y,
									src->width, src->height, src->bpp, r) )  return DDERR_GENERIC;
	blupi::render::ColorKey key;
	if ( bColorKey )  key = src->key;
	submit(blupi::render::make_blit(dst->id, x, y, src->id, r, key));
	return DD_OK;
}

// IDirectDrawSurface::Blt with scaling.
static BOOL blt_stretch(IDirectDrawSurface* dst, const RECT& dest, IDirectDrawSurface* src, const RECT& source)
{
	if ( dst == NULL || src == NULL )  return FALSE;
	blupi::render::Rect d = rect_of(dest), s = rect_of(source);
	if ( !blupi::render::stretch_valid(dst->width, dst->height, dst->bpp, d,
									   src->width, src->height, src->bpp, s) )  return FALSE;
	submit(blupi::render::make_stretch(dst->id, d, src->id, s));
	return TRUE;
}

// DDColorMatch + IDirectDrawSurface::SetColorKey(DDCKEY_SRCBLT).
static void set_color_key(IDirectDrawSurface* pdds, IDirectDrawPalette* pal, COLORREF rgb1, COLORREF rgb2)
{
	if ( pdds == NULL )  return;
	const blupi::render::Palette& p = palette_of(pal);
	pdds->key.enabled = true;
	pdds->key.low = native_of(pdds->bpp, p, rgb_of(rgb1));
	pdds->key.high = native_of(pdds->bpp, p, rgb_of(rgb2));
}

static bool read_file(const char* filename, std::vector<uint8_t>& data)
{
	FILE* file = blupi::vfs::open(filename, "rb");
	if ( file == NULL )  return false;
	uint8_t buffer[65536];
	size_t n;
	while ( (n = fread(buffer, 1, sizeof(buffer), file)) > 0 )  data.insert(data.end(), buffer, buffer + n);
	fclose(file);
	return true;
}

// DDLoadPalette: the colour table of an 8 bpp .blp file.
static IDirectDrawPalette* load_palette(const char* filename)
{
	std::vector<uint8_t> data;
	blupi::render::Bitmap bmp;
	if ( !read_file(filename, data) )  return NULL;
	if ( !blupi::render::decode_bmp(data.data(), data.size(), bmp) || bmp.bpp != 8 )  return NULL;
	IDirectDrawPalette* pal = new IDirectDrawPalette;
	for ( int i=0 ; i<256 ; i++ )  pal->entries[i] = bmp.palette[i];
	// CreatePalette(DDPCAPS_8BIT) without DDPCAPS_ALLOW256: DirectDraw keeps
	// entries 0 and 255 as black and white. [EVIDENCE] cross-build oracle,
	// 8 bpp run 2026-10-21: the historical primary palette has exactly this.
	pal->entries[0] = Rgb{0, 0, 0};
	pal->entries[255] = Rgb{255, 255, 255};
	return pal;
}

// The image converted to the display format the way GDI did (8 bpp: nearest palette entry;
// 16 bpp: truncation to RGB565).
static Surface convert_bitmap(const blupi::render::Bitmap& bmp, int bpp, const blupi::render::Palette& p)
{
	Surface image(bmp.width, bmp.height, bpp);
	if ( bmp.bpp == 8 )
	{
		uint32_t lut[256];
		for ( int i=0 ; i<256 ; i++ )  lut[i] = native_of(bpp, p, bmp.palette[i]);
		for ( int y=0 ; y<bmp.height ; y++ )
			for ( int x=0 ; x<bmp.width ; x++ )
				image.set_pixel(x, y, lut[bmp.indices[(size_t)y * bmp.width + x]]);
	}
	else
	{
		std::unordered_map<uint32_t, uint32_t> cache;
		for ( int y=0 ; y<bmp.height ; y++ )
			for ( int x=0 ; x<bmp.width ; x++ )
			{
				Rgb c = bmp.rgb[(size_t)y * bmp.width + x];
				uint32_t keyed = (c.r << 16) | (c.g << 8) | c.b;
				auto it = cache.find(keyed);
				if ( it == cache.end() )  it = cache.emplace(keyed, native_of(bpp, p, c)).first;
				image.set_pixel(x, y, it->second);
			}
	}
	return image;
}

// The pack's image for `filename`, for the mirrored upload that follows (port/docs/HD.md).
static void offer_pack_image(SurfaceId id, const char* filename, int bpp, const blupi::render::Palette& p,
							 int width, int height)
{
	if ( s_hd == NULL || s_hdPack.empty() )  return;
	blupi::render::Bitmap big;
	std::string found;
	const blupi::render::PackResult r =
		blupi::render::load_pack_image(s_hdPack, filename, s_hd->scale(), width, height, big, found);
	if ( r == blupi::render::PackResult::Loaded )
	{
		s_hd->set_next_upload(id, std::make_shared<const Surface>(convert_bitmap(big, bpp, p)));
		s_hdLoaded++;
		char text[400];
		snprintf(text, sizeof(text), "HD pack: %s from %s", filename, found.c_str());
		blupi::host::log_debug(text);
	}
	else if ( r != blupi::render::PackResult::Missing )
	{
		char text[400];
		snprintf(text, sizeof(text), "HD pack: %s is %s (not %dx%d); the original is shown enlarged",
				 found.c_str(), r == blupi::render::PackResult::WrongSize ? "another size" : "not a readable BMP",
				 width * s_hd->scale(), height * s_hd->scale());
		blupi::host::log_info(text);
	}
}

// DDLoadBitmap: the image converted to the display format (convert_bitmap), uploaded to the
// device as surface `id`.
static IDirectDrawSurface* load_bitmap(SurfaceId id, const char* filename, int bpp, IDirectDrawPalette* pal)
{
	std::vector<uint8_t> data;
	blupi::render::Bitmap bmp;
	if ( !read_file(filename, data) )  return NULL;
	if ( !blupi::render::decode_bmp(data.data(), data.size(), bmp) )  return NULL;

	const blupi::render::Palette& p = palette_of(pal);
	Surface image = convert_bitmap(bmp, bpp, p);

	IDirectDrawSurface* pdds = new IDirectDrawSurface(id, bmp.width, bmp.height, bpp);
	auto shared = std::make_shared<const Surface>(std::move(image));
	pdds->image = shared;
	offer_pack_image(id, filename, bpp, p, bmp.width, bmp.height);
	submit(blupi::render::make_upload(id, *shared));
	return pdds;
}

#endif
