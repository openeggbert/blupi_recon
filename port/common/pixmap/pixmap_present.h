// The port's presentation helpers of CPixmap (port_game.h), shared by both games' pixmap.cpp
// (docs/CONSOLIDATION_PLAN.md, stage 4 rank 4): copying and presenting the screen and the wide view,
// screenshots, the screen hash, the wide view's surfaces and cursor, the touch controls' button images.
// Until 2026-11-13 each game had an identical copy.
//
// Included once by each game's pixmap.cpp after CPixmap's methods (compiled in that game's context;
// pixmap/pixmap_port.h before).
#ifndef BLUPI_PIXMAP_PRESENT_H
#define BLUPI_PIXMAP_PRESENT_H

/////////////////////////////////////////////////////////////////////////////
// Port presentation helpers (port_game.h). Host paths, not game paths.

#undef fopen

bool PortPixmapHasPrimary()
{
	return s_primary != NULL;
}

bool PortPixmapCopyScreen(std::vector<uint32_t>& argb, int& width, int& height)
{
	const Surface* s = s_reference.surface(blupi::render::kScreen);
	if ( s_primary == NULL || s == NULL )  return false;
	width  = s->width();
	height = s->height();
	argb.resize((size_t)width * height);
	blupi::render::to_argb8888(*s, s_reference.palette(), argb.data(), width);
	return true;
}

static bool copy_presented_wide(std::vector<uint32_t>& argb, int& width, int& height);

// What is presented: the screen, mirrored at --hd-scale when that is on.
static bool copy_presented(blupi::render::SurfaceId id, std::vector<uint32_t>& argb, int& width, int& height)
{
	const Surface* s = s_hd != NULL ? s_hd->surface(id) : s_reference.surface(id);
	const blupi::render::Palette& pal = s_hd != NULL ? s_hd->palette() : s_reference.palette();
	if ( s == NULL )  return false;
	width  = s->width();
	height = s->height();
	argb.resize((size_t)width * height);
	blupi::render::to_argb8888(*s, pal, argb.data(), width);
	return true;
}

void PortPixmapPresentIfDirty()
{
	static std::vector<uint32_t> frame;
	int width, height;

	if ( PortPixmapWidePresentIfDirty() )  return;

	if ( !s_dirty )  return;
	s_dirty = false;
	if ( s_primary != NULL && copy_presented(blupi::render::kScreen, frame, width, height) )
	{
		blupi::host::present(frame.data(), width);
	}
}

uint64_t PortPixmapScreenHash()
{
	std::vector<uint32_t> frame;
	int width, height;
	uint64_t hash = 1469598103934665603ULL;

	if ( !PortPixmapCopyScreen(frame, width, height) )  return 0;
	for ( uint32_t px : frame )
	{
		for ( int i=0 ; i<3 ; i++ )
		{
			hash ^= (px >> (i*8)) & 0xff;
			hash *= 1099511628211ULL;
		}
	}
	return hash;
}

bool PortPixmapWriteScreenBmp(const char* path)
{
	std::vector<uint32_t> frame;
	int width, height;

	// The view, else the screen; as presented (--hd-scale included).
	if ( !copy_presented_wide(frame, width, height) &&
		 !(s_primary != NULL && copy_presented(blupi::render::kScreen, frame, width, height)) )  return false;
	FILE* file = fopen(path, "wb");
	if ( file == NULL )  return false;
	const size_t stride = ((size_t)width * 3 + 3) & ~(size_t)3;   // rows padded to 4 bytes
	uint32_t imageSize = (uint32_t)(stride * height);
	uint8_t header[54] = {'B', 'M'};
	uint32_t fileSize = 54 + imageSize;
	auto put32 = [&](int at, uint32_t v) { for ( int i=0 ; i<4 ; i++ ) header[at+i] = (uint8_t)(v >> (i*8)); };
	put32(2, fileSize);
	put32(10, 54);
	put32(14, 40);
	put32(18, (uint32_t)width);
	put32(22, (uint32_t)height);
	header[26] = 1;
	header[28] = 24;
	put32(34, imageSize);
	fwrite(header, 1, sizeof(header), file);
	std::vector<uint8_t> line(stride, 0);
	for ( int y=height-1 ; y>=0 ; y-- )
	{
		for ( int x=0 ; x<width ; x++ )
		{
			uint32_t px = frame[(size_t)y*width + x];
			line[x*3+0] = (uint8_t)(px);
			line[x*3+1] = (uint8_t)(px >> 8);
			line[x*3+2] = (uint8_t)(px >> 16);
		}
		fwrite(line.data(), 1, line.size(), file);
	}
	fclose(file);
	return true;
}


// Widescreen view (ROADMAP.md stage 8).
// A target wider than the game's back buffer. While it is being drawn,
// CPixmap's back surface is the target, so the historical drawing code
// (QuickIcon, DrawPart, texts) draws into it with its own clipping; the game's
// back buffer and screen are left alone.

namespace
{
#define WIDE_PIXMAP_MEMBERS(X) \
	X(m_lpDDSBack) X(m_lpDDSurface) X(m_clipRect) X(m_mousePos) X(m_mouseHotSpot) \
	X(m_mouseType) X(m_mouseSprite) X(m_bMouseShow) X(MouseRectSprite)
BLUPI_MEMBER_ACCESS(PixmapAccess, CPixmap, WIDE_PIXMAP_MEMBERS)
}  // namespace

static int                 s_wideWidth = 0;      // 0: the host shows the screen
static IDirectDrawSurface* s_wide = NULL;
static IDirectDrawSurface* s_wideSavedBack = NULL;
static RECT                s_wideSavedClip;
static bool                s_wideDirty = false;

void PortPixmapSetWide(int width)
{
	s_wideWidth = width;
}

bool PortPixmapWideBegin(CPixmap* pixmap)
{
	IDirectDrawSurface*& back = pixmap->*PixmapAccess::m_lpDDSBack;
	if ( s_wideWidth <= 0 || back == NULL || s_wideSavedBack != NULL )  return false;
	// A fresh surface each time: black where nothing is drawn.
	delete s_wide;
	s_wide = new IDirectDrawSurface(blupi::render::kWideView, s_wideWidth, back->height, back->bpp);
	s_wideSavedBack = back;
	s_wideSavedClip = pixmap->*PixmapAccess::m_clipRect;
	back = s_wide;
	pixmap->*PixmapAccess::m_clipRect = RECT{0, 0, s_wide->width, s_wide->height};
	return true;
}

void PortPixmapWideEnd(CPixmap* pixmap)
{
	if ( s_wideSavedBack == NULL )  return;
	pixmap->*PixmapAccess::m_lpDDSBack = s_wideSavedBack;
	pixmap->*PixmapAccess::m_clipRect = s_wideSavedClip;
	s_wideSavedBack = NULL;
}

void PortPixmapWideSubmit(const std::vector<blupi::render::Command>& commands, int dx)
{
	if ( s_wide == NULL )  return;
	for ( blupi::render::Command c : commands )
	{
		if ( (c.kind == blupi::render::Command::Blit || c.kind == blupi::render::Command::Stretch) &&
			 c.dst == blupi::render::kBackBuffer )
		{
			c.dst = s_wide->id;
			c.dstRect.left  += dx;
			c.dstRect.right += dx;
		}
		submit(c);
	}
}

void PortPixmapWideBlitBack(int dx)
{
	if ( s_wide == NULL || s_back == NULL )  return;
	submit(blupi::render::make_blit(s_wide->id, dx, 0, s_back->id,
									blupi::render::Rect{0, 0, s_back->width, s_back->height},
									blupi::render::ColorKey()));
}

// CPixmap::MouseBackDraw without the save of what is under the sprite (the
// view is drawn again for every presentation), in the target's bounds.
void PortPixmapWideCursor(CPixmap* pixmap, int dx, const POINT* at)
{
	IDirectDrawSurface* sprite = (pixmap->*PixmapAccess::m_lpDDSurface)[CHPIXMAP_CURSOR];
	POINT dst;
	RECT  rcRect;

	if ( s_wide == NULL || sprite == NULL || s_cursorHidden )  return;
	if ( pixmap->*PixmapAccess::m_mouseType != MOUSETYPEGRA )  return;
	if ( pixmap->*PixmapAccess::m_mouseSprite == SPRITE_EMPTY )  return;
	if ( pixmap->*PixmapAccess::m_mouseSprite == SPRITE_DISABLE )  return;
	if ( !(pixmap->*PixmapAccess::m_bMouseShow) )  return;

	const POINT mouse = at != NULL ? *at : pixmap->*PixmapAccess::m_mousePos;
	dst.x = mouse.x - (pixmap->*PixmapAccess::m_mouseHotSpot).x;
	dst.y = mouse.y - (pixmap->*PixmapAccess::m_mouseHotSpot).y;
	(pixmap->*PixmapAccess::MouseRectSprite)(&rcRect, &dst);
	dst.x += dx;

	if ( dst.x < 0 )
	{
		rcRect.left -= dst.x;
		dst.x = 0;
	}
	if ( rcRect.right-rcRect.left+dst.x > s_wide->width )
	{
		rcRect.right = rcRect.left-dst.x+s_wide->width;
	}
	if ( dst.y < 0 )
	{
		rcRect.top -= dst.y;
		dst.y = 0;
	}
	if ( rcRect.bottom-rcRect.top+dst.y > s_wide->height )
	{
		rcRect.bottom = rcRect.top-dst.y+s_wide->height;
	}
	blt_fast(s_wide, dst.x, dst.y, sprite, rcRect, TRUE);
}

void PortPixmapWidePresent()
{
	s_wideDirty = true;
}

bool PortPixmapCopyWide(std::vector<uint32_t>& argb, int& width, int& height)
{
	if ( s_wideWidth <= 0 || s_wide == NULL )  return false;
	const Surface* s = s_reference.surface(s_wide->id);
	if ( s == NULL )  return false;
	width  = s->width();
	height = s->height();
	argb.resize((size_t)width * height);
	blupi::render::to_argb8888(*s, s_reference.palette(), argb.data(), width);
	return true;
}

static bool copy_presented_wide(std::vector<uint32_t>& argb, int& width, int& height)
{
	return s_wideWidth > 0 && s_wide != NULL && copy_presented(s_wide->id, argb, width, height);
}

bool PortPixmapWidePresentIfDirty()
{
	static std::vector<uint32_t> frame;
	int width, height;

	if ( s_wideWidth <= 0 )  return false;
	if ( !s_wideDirty )  return true;
	s_wideDirty = false;
	if ( copy_presented_wide(frame, width, height) )  blupi::host::present(frame.data(), width);
	return true;
}

void PortPixmapWideRelease()
{
	delete s_wide;
	s_wide = NULL;
	s_wideSavedBack = NULL;
}

bool PortPixmapWideRegionEqualsBack(int x)
{
	if ( s_wide == NULL || s_back == NULL )  return false;
	const Surface* wide = s_reference.surface(s_wide->id);
	const Surface* back = s_reference.surface(s_back->id);
	if ( wide == NULL || back == NULL || x < 0 || x + back->width() > wide->width() )  return false;
	const size_t pixel = (size_t)back->bpp() / 8;
	for ( int y=0 ; y<back->height() ; y++ )
	{
		if ( memcmp(wide->row(y) + x * pixel, back->row(y), back->width() * pixel) != 0 )  return false;
	}
	return true;
}


// Touch screens (port/common/platform/touch.h): a game button as an image for the
// on-screen controls. The button background (CHBUTTON icon 0 released, 1
// pressed) with an icon on it (the phase table's icon number + 6, as
// CButton::Draw draws it), from the sheet as loaded; the sheet's colour key
// becomes transparency. 40x40, 0xAARRGGBB.
bool PortPixmapButtonImage(CPixmap* pixmap, int iconMenu, bool pressed, std::vector<uint32_t>& argb)
{
	IDirectDrawSurface* sheet = (pixmap->*PixmapAccess::m_lpDDSurface)[CHBUTTON];
	if ( sheet == NULL || sheet->image == NULL )  return false;
	const Surface& img = *sheet->image;
	const int size = DIMBUTTONX, perRow = img.width() / size;
	if ( perRow <= 0 )  return false;

	std::vector<uint32_t> all((size_t)img.width() * img.height());
	blupi::render::to_argb8888(img, s_reference.palette(), all.data(), img.width());
	auto keyed = [&](int x, int y)
	{
		if ( !sheet->key.enabled )  return false;
		const uint8_t* row = img.row(y);
		uint32_t v = img.bpp() == 8 ? row[x] : (uint32_t)(row[x*2] | (row[x*2+1] << 8));
		return v >= sheet->key.low && v <= sheet->key.high;
	};

	argb.assign((size_t)size * size, 0);
	const int layers[2] = { pressed ? 1 : 0, iconMenu + 6 };
	for ( int icon : layers )
	{
		const int ox = (icon % perRow) * size, oy = (icon / perRow) * size;
		if ( oy + size > img.height() )  return false;
		for ( int y=0 ; y<size ; y++ )
		{
			for ( int x=0 ; x<size ; x++ )
			{
				if ( keyed(ox + x, oy + y) )  continue;
				argb[(size_t)y * size + x] = all[(size_t)(oy + y) * img.width() + ox + x] | 0xFF000000u;
			}
		}
	}
	return true;
}

#endif
