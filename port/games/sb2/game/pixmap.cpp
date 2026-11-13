#include "render/display_capture.h"
// SB2 CPixmap adapter, derived from port/games/sb1/game/pixmap.cpp
// and games/sb2/src/pixmap.cpp; historical clipping/blit bodies retained.
// Port replacement of games/sb2/src/pixmap.cpp (historical: Planet Blupi-derived
// DirectDraw implementation of CPixmap).
//
// Keeps the historical class declaration (src/pixmap.h) and the historical
// method bodies: the clipping arithmetic, icon tables, colour-key juggling and
// the software mouse cursor are copied from src/pixmap.cpp. The DirectDraw
// and GDI calls are replaced by commands to a rendering device
// (port/common/render/device.h). What DirectDraw decided (blit validity, the colour
// key in force) is decided here from a CPU-side registry, so the device only
// executes. The reference device is the software renderer, which reproduces
// the original pixels in 8 bpp and RGB565 (port/docs/ORACLE.md).
//
// The DirectDraw handle types named by src/pixmap.h are opaque to the
// historical code; here a surface handle is a registry entry.

#include <stdlib.h>
#include <stdio.h>
#include "def.h"
#include "pixmap.h"
#include "misc.h"

#include "game/port_game.h"
#include "game/member_access.h"
#include "platform/host.h"
#include "platform/vfs.h"
#include "render/bmp.h"
#include "render/device.h"
#include "render/surface.h"
#include "render/scaled.h"
#include "render/hd_pack.h"

#include <cstring>
#include <memory>
#include <unordered_map>

#include "pixmap/pixmap_port.h"   // the shared devices, registry, helpers and loaders

extern short table_icon_object[];
extern short table_icon_element[];
extern short table_icon_blupi[];
extern short table_icon_explo[];

// SB2 atlas bounds and neighbouring values read statically from Eggbert2.exe
// .data (both editions have the same complete atlas records).
namespace
{
struct IconTable
{
	const short* table;
	int          size;        // values, count included (tests/test_layout.cpp)
	short        before[6];   // the original's values at index -6..-1
	short        after[6];    // and at size..size+5
};

const IconTable s_iconTables[] =
{
    { table_icon_blupi, 2017, { 17742, 18770, 67, 0, 0, 0 }, { 0, 0, 0, 441, 504, 687 } },
    { table_icon_object, 2647, { 32, 49, 27, 0, 0, 0 }, { 0, 289, 308, 153, 9, 20 } },
    { table_icon_element, 1735, { 559, 18, 0, 28, 64, 0 }, { 0, 100, 157, 0, 48, 51 } },
    { table_icon_explo, 601, { 153, 7, 14, 44, 41, 0 }, { 0, 0, 0, 30, 0, 30 } },
};

short IconValue(const short* table, int index)
{
	for ( const IconTable& t : s_iconTables )
	{
		if ( t.table != table )  continue;
		if ( index >= 0 && index < t.size )  return t.table[index];
		if ( index < 0 && index >= -6 )
		{
			// The only edition difference in these neighbourhoods: Blupi 2's
			// preceding string/padding starts four bytes earlier than Eggbert's.
			static const short blupiBefore[] = {24402, 17735, 17742, 18770, 67, 0};
			if (table == table_icon_blupi && PortEdition() == "blupi") return blupiBefore[index + 6];
			return t.before[index + 6];
		}
		if ( index >= t.size && index < t.size + 6 )  return t.after[index - t.size];
		static bool reported = false;
		if (!reported) { blupi::host::log_info("SB2: icon read beyond modelled atlas neighbourhood; drawing omitted."); reported = true; }
		return 0;
	}
	return 0;
}
}  // namespace

/////////////////////////////////////////////////////////////////////////////


// Constructeur.

CPixmap::CPixmap()
{
	int		i;

	m_bFullScreen  = FALSE;
	m_bTrueColorBack   = FALSE;
	m_bTrueColorDecor = FALSE;
	m_bBenchmarkSuccess = FALSE;
	m_mouseType    = MOUSETYPEGRA;
	m_bDebug       = TRUE;
	m_bPalette     = TRUE;

	m_mouseSprite  = SPRITE_WAIT;
	MouseHotSpot();
	m_mousePos.x   = LXIMAGE/2;
	m_mousePos.y   = LYIMAGE/2;
	m_mouseBackPos = m_mousePos;
	m_bMouseBack   = FALSE;
	m_bMouseShow   = TRUE;
	m_bBackDisplayed = FALSE;

	m_lpDD         = NULL;
	m_lpDDSPrimary = NULL;
	m_lpDDSBack    = NULL;
	m_lpDDSMouse   = NULL;
	m_lpDDPal      = NULL;
	m_lpClipper    = NULL;

	for ( i=0 ; i<MAXIMAGE ; i++ )
	{
		m_lpDDSurface[i] = NULL;
	}

	// initialize special effects structure
	ZeroMemory(&m_DDbltfx, sizeof(m_DDbltfx));
	m_DDbltfx.dwSize = sizeof(m_DDbltfx);
}

// Destructeur.

CPixmap::~CPixmap()
{
	ReleaseAll();
}

void CPixmap::ReleaseAll()
{
	int		i;

	if ( m_lpDDSPrimary == s_primary )
	{
		s_primary = NULL;
		s_back = NULL;
		PortPixmapWideRelease();
	}
	delete m_lpDDSPrimary;
	delete m_lpDDSBack;
	delete m_lpDDSMouse;
	delete m_lpDDPal;
	for ( i=0 ; i<MAXIMAGE ; i++ )
	{
		delete m_lpDDSurface[i];
		m_lpDDSurface[i] = NULL;
	}
	delete m_lpClipper;
	delete m_lpDD;
	m_lpDDSPrimary = m_lpDDSBack = m_lpDDSMouse = NULL;
	m_lpDDPal = NULL;
	m_lpClipper = NULL;
	m_lpDD = NULL;
}


void CPixmap::SetDebug(BOOL bDebug)
{
	m_bDebug = bDebug;
}


// Crée l'objet DirectDraw principal.
// Retourne FALSE en cas d'erreur.
//
// Port: the screen is the port's 640x480 logical frame. Its format is the
// original exclusive-mode format: 16 bpp when bTrueColor or bTrueColorDecor, else 8 bpp with
// palette (the original windowed mode used the desktop format instead).

BOOL CPixmap::Create(HWND hwnd, POINT dim,
					 BOOL bFullScreen, BOOL bTrueColor, BOOL bTrueColorDecor, int mouseType)
{
	POINT				pos;
	int					depth;

	m_hWnd           = hwnd;
	m_bFullScreen    = bFullScreen;
	m_bTrueColorBack = bTrueColor;
	m_bTrueColorDecor = bTrueColorDecor;
	m_mouseType      = mouseType;
	m_dim            = dim;

	if ( m_mouseType == MOUSETYPEGRA )
	{
		// Cache définitivement la vilaine souris Windows.
		ShowCursor(FALSE);

		pos = m_mousePos;
		ClientToScreen(m_hWnd, &pos);
		SetCursorPos(pos.x, pos.y);  // met la souris au centre
	}

	m_clipRect.left   = 0;
	m_clipRect.top    = 0;
	m_clipRect.right  = dim.x;
	m_clipRect.bottom = dim.y;

	depth = bTrueColor || bTrueColorDecor ? 16 : 8;
	m_lpDD         = new IDirectDraw;
	m_lpDDSPrimary = new IDirectDrawSurface(blupi::render::kScreen, dim.x, dim.y, depth);
	m_lpDDSBack    = new IDirectDrawSurface(blupi::render::kBackBuffer, dim.x, dim.y, depth);
	m_lpDDSMouse   = new IDirectDrawSurface(blupi::render::kMouseSave, 64, 64, depth);

	s_primary = m_lpDDSPrimary;
	s_back = m_lpDDSBack;
	return TRUE;
}

// Libère les bitmaps.

BOOL CPixmap::Flush()
{
	return TRUE;
}

BOOL CPixmap::GetTrueColorBack()
{
	return m_bTrueColorBack;
}

BOOL CPixmap::GetTrueColorDecor()
{
	return m_bTrueColorDecor;
}

// Restitue les bitmaps.

BOOL CPixmap::Restore()
{
	RestoreAll();
	return TRUE;
}

// Initialise la palette système.
// Port: there is no shared system palette; in 8 bpp the screen palette is
// the system palette, in 16 bpp there is none (as on a true-colour desktop).

BOOL CPixmap::InitSysPalette()
{
	int			i;

	if ( !m_bFullScreen )
	{
		m_bPalette = !m_bTrueColorBack;
	}

	for ( i=0 ; i<256 ; i++ )
	{
		Rgb c = palette_of(m_lpDDPal)[i];
		m_sysPal[i].peRed   = c.r;
		m_sysPal[i].peGreen = c.g;
		m_sysPal[i].peBlue  = c.b;
		m_sysPal[i].peFlags = 0;
	}
	return TRUE;
}

// Indique si l'on utilise une palette.

BOOL CPixmap::IsPalette()
{
	return m_bPalette;
}


// Rempli une zone rectangulaire avec une couleur uniforme.

void CPixmap::Fill(RECT rect, COLORREF color)
{
	// à faire si nécessaire ...
}


// Restore all lost objects.
// Port: software surfaces are never lost.

HRESULT CPixmap::RestoreAll()
{
	return DD_OK;
}

// Effectue un appel BltFast.
// Les modes sont 0=transparent, 1=opaque.

HRESULT CPixmap::BltFast(int chDst, int channel,
						 POINT dst, RECT rcRect, int mode)
{
	int			limit;

	// Effectue un peu de clipping.
	if ( dst.x < m_clipRect.left )
	{
		rcRect.left += m_clipRect.left-dst.x;
		dst.x = m_clipRect.left;
	}
	limit = (m_clipRect.right-dst.x)+rcRect.left;
	if ( rcRect.right > limit )
	{
		rcRect.right = limit;
	}
	if ( dst.y < m_clipRect.top )
	{
		rcRect.top += m_clipRect.top-dst.y;
		dst.y = m_clipRect.top;
	}
	limit = (m_clipRect.bottom-dst.y)+rcRect.top;
	if ( rcRect.bottom > limit )
	{
		rcRect.bottom = limit;
	}

	if ( rcRect.left >= rcRect.right ||
		 rcRect.top  >= rcRect.bottom )  return DD_OK;

	if ( chDst < 0 )
	{
		return blt_fast(m_lpDDSBack, dst.x, dst.y, m_lpDDSurface[channel], rcRect, mode == 0);
	}
	return blt_fast(m_lpDDSurface[chDst], dst.x, dst.y, m_lpDDSurface[channel], rcRect, mode == 0);
}

// Effectue un appel BltFast.
// Les modes sont 0=transparent, 1=opaque.

HRESULT CPixmap::BltFast(LPDIRECTDRAWSURFACE lpDD,
						 int channel, POINT dst, RECT rcRect, int mode)
{
	return blt_fast(lpDD, dst.x, dst.y, m_lpDDSurface[channel], rcRect, mode == 0);
}


BOOL CPixmap::DrawMap(int channel, RECT src, RECT dest)
{
	if ( channel < 0 || channel >= MAXIMAGE ) return FALSE;
	if ( m_lpDDSurface[channel] == NULL ) return FALSE;
	return blt_stretch(m_lpDDSurface[channel], dest, m_lpDDSBack, src);
}

// Sauve toute la palette de couleurs.

BOOL CPixmap::SavePalette()
{
	int		i;

	if ( m_lpDDPal == NULL )  return FALSE;

	for ( i=0 ; i<256 ; i++ )
	{
		m_pal[i].peRed   = m_lpDDPal->entries[i].r;
		m_pal[i].peGreen = m_lpDDPal->entries[i].g;
		m_pal[i].peBlue  = m_lpDDPal->entries[i].b;
		m_pal[i].peFlags = 0;
	}
	return TRUE;
}

// Restitue toute la palette de couleurs.

BOOL CPixmap::RestorePalette()
{
	int		i;

	if ( m_lpDDPal == NULL )  return FALSE;

	for ( i=0 ; i<256 ; i++ )
	{
		m_lpDDPal->entries[i] = Rgb{m_pal[i].peRed, m_pal[i].peGreen, m_pal[i].peBlue};
	}
	submit_palette(m_lpDDPal);
	s_dirty = true;
	return TRUE;
}

// Cherche une couleur dans la palette principale.
// En mode plein écran, il faut chercher dans la palette
// correspondant aux images (obtenue avec SavePalette),
// alors qu'en mode fenêtre, il faut chercher dans la
// palette système (obtenue avec InitSysPalette) !!!

int CPixmap::SearchColor(int red, int green, int blue)
{
	int		i, j, delta, min;

	if ( m_bFullScreen )
	{
		for ( i=0 ; i<256 ; i++ )
		{
			if ( red   == m_pal[i].peRed   &&
				 green == m_pal[i].peGreen &&
				 blue  == m_pal[i].peBlue  )  return i;
		}

		// Cherche la couleur la plus proche.
		min = 10000;
		j   = -1;
		for ( i=0 ; i<256 ; i++ )
		{
			delta = abs(red   - m_pal[i].peRed  )+
					abs(green - m_pal[i].peGreen)+
					abs(blue  - m_pal[i].peBlue );

			if ( delta < min )
			{
				min = delta;
				j = i;
			}
		}
	}
	else
	{
		if ( m_bPalette )
		{
			for ( i=0 ; i<256 ; i++ )
			{
				if ( red   == m_sysPal[i].peRed   &&
					 green == m_sysPal[i].peGreen &&
					 blue  == m_sysPal[i].peBlue  )  return i;
			}

			// Cherche la couleur la plus proche.
			min = 10000;
			j   = -1;
			for ( i=0 ; i<256 ; i++ )
			{
				delta = abs(red   - m_sysPal[i].peRed  )+
						abs(green - m_sysPal[i].peGreen)+
						abs(blue  - m_sysPal[i].peBlue );

				if ( delta < min )
				{
					min = delta;
					j = i;
				}
			}
		}
		else
		{
			j  =  (blue >>3)     &0x001F;
			j |= ((green>>2)<< 5)&0x07E0;
			j |= ((red  >>3)<<11)&0xF800;  // mode 5-6-5
		}
	}
	return j;
}


// Cache une image contenant des icônes.

BOOL CPixmap::Cache(int channel, char *pFilename, POINT totalDim, POINT iconDim,
					BOOL bUsePalette)
{
	IDirectDrawPalette*	pal;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;

	if ( m_lpDDSurface[channel] != NULL )
	{
		Flush(channel);
	}

	// Create and set the palette.
	if ( bUsePalette )
	{
		if ( m_bDebug )  OutputDebug("Use palette\n");
		pal = load_palette(pFilename);
		if ( pal )
		{
			if ( m_bDebug )  OutputDebug("Set palette\n");
			delete m_lpDDPal;
			m_lpDDPal = pal;
			submit_palette(m_lpDDPal);
			s_dirty = true;
		}
	}

	// Create the offscreen surface, by loading our bitmap.
	m_lpDDSurface[channel] = load_bitmap(channel, pFilename, m_lpDDSBack->bpp, m_lpDDPal);

	if ( m_lpDDSurface[channel] == NULL )
	{
		OutputDebug("Fatal error: DDLoadBitmap\n");
		return FALSE;
	}

	// Set the color key to white
	if ( m_bDebug )  OutputDebug("DDSetColorKey\n");
	set_color_key(m_lpDDSurface[channel], m_lpDDPal, RGB(255,255,255), RGB(255,255,255));  // blanc

	strcpy(m_filename[channel], pFilename);

	m_totalDim[channel] = totalDim;
	m_iconDim[channel]  = iconDim;

	return TRUE;
}

// Cache une image globale.

BOOL CPixmap::Cache(int channel, char *pFilename, POINT totalDim, BOOL bUsePalette)
{
	POINT		iconDim;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;

	iconDim.x = 0;
	iconDim.y = 0;

	return Cache(channel, pFilename, totalDim, iconDim, bUsePalette);
}

// Purge une image.

void CPixmap::Flush(int channel)
{
	if ( channel < 0 || channel >= MAXIMAGE )  return;
	if (  m_lpDDSurface[channel] == NULL )     return;

	delete m_lpDDSurface[channel];
	m_lpDDSurface[channel]= NULL;
}

// Définition de la couleur transparente.

void CPixmap::SetTransparent(int channel, COLORREF color)
{
	if ( channel < 0 || channel >= MAXIMAGE )  return;
	if (  m_lpDDSurface[channel] == NULL )     return;

	set_color_key(m_lpDDSurface[channel], m_lpDDPal, color, color);
	m_colorSurface[2*channel+0] = color;
	m_colorSurface[2*channel+1] = color;
}

// Définition de la couleur transparente.

void CPixmap::SetTransparent2(int channel, COLORREF color1, COLORREF color2)
{
	if ( channel < 0 || channel >= MAXIMAGE )  return;
	if (  m_lpDDSurface[channel] == NULL )     return;

	set_color_key(m_lpDDSurface[channel], m_lpDDPal, color1, color2);
	m_colorSurface[2*channel+0] = color1;
	m_colorSurface[2*channel+1] = color2;
}


// Modifie la région de clipping.

void CPixmap::SetClipping(RECT clip)
{
	m_clipRect = clip;
}

// Retourne la région de clipping.

RECT CPixmap::GetClipping()
{
	return m_clipRect;
}


// Teste si un point fait partie d'une icône.

void CPixmap::QuickIcon(int channel, int rank, POINT pos)
{
    int nbx, nby;
    RECT rect;
    int limit;

    if ( channel == 1 )
    {
        if ( rank >= IconValue(table_icon_object, 0) ) return;
        rect.left = IconValue(table_icon_object, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_object, rank*6+5);
        rect.top = IconValue(table_icon_object, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_object, rank*6+6);
        pos.x += IconValue(table_icon_object, rank*6+3);
        pos.y += IconValue(table_icon_object, rank*6+4);
    }
    else if ( channel == 10 )
    {
        if ( rank >= IconValue(table_icon_element, 0) ) return;
        rect.left = IconValue(table_icon_element, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_element, rank*6+5);
        rect.top = IconValue(table_icon_element, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_element, rank*6+6);
        pos.x += IconValue(table_icon_element, rank*6+3);
        pos.y += IconValue(table_icon_element, rank*6+4);
    }
    else if ( channel == 2 || channel == 11 || channel == 12 || channel == 13 )
    {
        if ( rank >= IconValue(table_icon_blupi, 0) ) return;
        rect.left = IconValue(table_icon_blupi, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_blupi, rank*6+5);
        rect.top = IconValue(table_icon_blupi, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_blupi, rank*6+6);
        pos.x += IconValue(table_icon_blupi, rank*6+3);
        pos.y += IconValue(table_icon_blupi, rank*6+4);
    }
    else if ( channel == 9 )
    {
        if ( rank >= IconValue(table_icon_explo, 0) ) return;
        rect.left = IconValue(table_icon_explo, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_explo, rank*6+5);
        rect.top = IconValue(table_icon_explo, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_explo, rank*6+6);
        pos.x += IconValue(table_icon_explo, rank*6+3);
        pos.y += IconValue(table_icon_explo, rank*6+4);
    }
    else
    {
        nbx = m_totalDim[channel].x / m_iconDim[channel].x;
        nby = m_totalDim[channel].y / m_iconDim[channel].y;
        if ( rank < 0 || rank >= nbx*nby ) return;
        rect.left   = (rank%nbx)*m_iconDim[channel].x;
        rect.top    = (rank/nbx)*m_iconDim[channel].y;
        rect.right  = rect.left + m_iconDim[channel].x;
        rect.bottom = rect.top  + m_iconDim[channel].y;
    }

	// Effectue un peu de clipping.
	if ( pos.x < m_clipRect.left )
	{
		rect.left += m_clipRect.left-pos.x;
		pos.x = m_clipRect.left;
	}
	limit = (m_clipRect.right-pos.x)+rect.left;
	if ( rect.right > limit )
	{
		rect.right = limit;
	}
	if ( pos.y < m_clipRect.top )
	{
		rect.top += m_clipRect.top-pos.y;
		pos.y = m_clipRect.top;
	}
	limit = (m_clipRect.bottom-pos.y)+rect.top;
	if ( rect.bottom > limit )
	{
		rect.bottom = limit;
	}

	if ( rect.left >= rect.right ||
		 rect.top  >= rect.bottom )  return;

	blt_fast(m_lpDDSBack, pos.x, pos.y, m_lpDDSurface[channel], rect, TRUE);
}


// Port: GDI GetPixel on the surface DC returned the stored pixel expanded to
// a COLORREF; the comparison against the key colours is historical.

BOOL CPixmap::IsIconPixel(int channel, int rank, POINT pos)
{
	int			nbx, nby;
	COLORREF	rgb;
	Rgb			c;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;
	if (  m_lpDDSurface[channel] == NULL )     return FALSE;

	if ( m_iconDim[channel].x == 0 ||
		 m_iconDim[channel].y == 0 )  return FALSE;

	nbx = m_totalDim[channel].x / m_iconDim[channel].x;
	nby = m_totalDim[channel].y / m_iconDim[channel].y;

	if ( rank < 0 || rank >= nbx*nby )  return FALSE;

	pos.x += (rank%nbx)*m_iconDim[channel].x;
	pos.y += (rank/nbx)*m_iconDim[channel].y;

	// Port: current pixels from the reference device, else the pixels as loaded.
	const Surface* s = s_reference.surface(m_lpDDSurface[channel]->id);
	if ( s == NULL )  s = m_lpDDSurface[channel]->image.get();
	if ( s == NULL )  return FALSE;
	if ( pos.x < 0 || pos.y < 0 || pos.x >= s->width() || pos.y >= s->height() )  return FALSE;
	c = blupi::render::from_native(*s, palette_of(m_lpDDPal), s->pixel(pos.x, pos.y));
	rgb = RGB(c.r, c.g, c.b);

	if ( rgb == m_colorSurface[2*channel+0] ||
		 rgb == m_colorSurface[2*channel+1] )  return FALSE;

	return TRUE;
}


// Dessine une partie d'image rectangulaire.
// Les modes sont 0=transparent, 1=opaque.

BOOL CPixmap::DrawIcon(int chDst, int channel, int rank, POINT pos,
                       int mode, BOOL bMask)
{
    int nbx, nby;
    RECT rect;
    HRESULT ddrval;
    COLORREF oldColor1, oldColor2;

    if ( channel == 1 )
    {
        if ( rank >= IconValue(table_icon_object, 0) ) return FALSE;
        rect.left = IconValue(table_icon_object, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_object, rank*6+5);
        rect.top = IconValue(table_icon_object, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_object, rank*6+6);
        pos.x += IconValue(table_icon_object, rank*6+3);
        pos.y += IconValue(table_icon_object, rank*6+4);
    }
    else if ( channel == 10 )
    {
        if ( rank >= IconValue(table_icon_element, 0) ) return FALSE;
        rect.left = IconValue(table_icon_element, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_element, rank*6+5);
        rect.top = IconValue(table_icon_element, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_element, rank*6+6);
        pos.x += IconValue(table_icon_element, rank*6+3);
        pos.y += IconValue(table_icon_element, rank*6+4);
    }
    else if ( channel == 2 || channel == 11 || channel == 12 || channel == 13 )
    {
        if ( rank >= IconValue(table_icon_blupi, 0) ) return FALSE;
        rect.left = IconValue(table_icon_blupi, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_blupi, rank*6+5);
        rect.top = IconValue(table_icon_blupi, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_blupi, rank*6+6);
        pos.x += IconValue(table_icon_blupi, rank*6+3);
        pos.y += IconValue(table_icon_blupi, rank*6+4);
    }
    else if ( channel == 9 )
    {
        if ( rank >= IconValue(table_icon_explo, 0) ) return FALSE;
        rect.left = IconValue(table_icon_explo, rank*6+1);
        rect.right = rect.left + IconValue(table_icon_explo, rank*6+5);
        rect.top = IconValue(table_icon_explo, rank*6+2);
        rect.bottom = rect.top + IconValue(table_icon_explo, rank*6+6);
        pos.x += IconValue(table_icon_explo, rank*6+3);
        pos.y += IconValue(table_icon_explo, rank*6+4);
    }
    else
    {
        if ( channel < 0 || channel >= MAXIMAGE ) return FALSE;
        if ( m_lpDDSurface[channel] == NULL ) return FALSE;
        if ( m_iconDim[channel].x == 0 || m_iconDim[channel].y == 0 ) return FALSE;
        nbx = m_totalDim[channel].x / m_iconDim[channel].x;
        nby = m_totalDim[channel].y / m_iconDim[channel].y;
        if ( rank < 0 || rank >= nbx*nby ) return FALSE;
        rect.left = (rank%nbx)*m_iconDim[channel].x;
        rect.top = (rank/nbx)*m_iconDim[channel].y;
        rect.right = rect.left + m_iconDim[channel].x;
        rect.bottom = rect.top + m_iconDim[channel].y;
    }

    oldColor1 = m_colorSurface[2*channel+0];
    oldColor2 = m_colorSurface[2*channel+1];
    if ( bMask ) SetTransparent(channel, 0x000000ff);
    ddrval = BltFast(chDst, channel, pos, rect, mode);
    if ( bMask ) SetTransparent2(channel, oldColor1, oldColor2);
    if ( ddrval != DD_OK ) return FALSE;
    return TRUE;
}

BOOL CPixmap::DrawIconDemi(int chDst, int channel, int rank, POINT pos,
						   int mode, BOOL bMask)
{
	int			nbx, nby;
	RECT		rect;
	HRESULT		ddrval;
	COLORREF	oldColor1, oldColor2;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;
	if (  m_lpDDSurface[channel] == NULL )     return FALSE;

	if ( m_iconDim[channel].x == 0 ||
		 m_iconDim[channel].y == 0 )  return FALSE;

	nbx = m_totalDim[channel].x /  m_iconDim[channel].x;
	nby = m_totalDim[channel].y / (m_iconDim[channel].y/2);

	rank = (rank/32)*32+((rank%32)/2)+((rank%2)*16);

	if ( rank < 0 || rank >= nbx*nby )  return FALSE;

	rect.left   = (rank%nbx)* m_iconDim[channel].x;
	rect.top    = (rank/nbx)*(m_iconDim[channel].y/2);
	rect.right  = rect.left + m_iconDim[channel].x;
	rect.bottom = rect.top  +(m_iconDim[channel].y/2);

	oldColor1 = m_colorSurface[2*channel+0];
	oldColor2 = m_colorSurface[2*channel+1];
	if ( bMask )  SetTransparent(channel, 0x000000ff);  // blanc
	ddrval = BltFast(chDst, channel, pos, rect, mode);
	if ( bMask )  SetTransparent2(channel, oldColor1, oldColor2);

	if ( ddrval != DD_OK )  return FALSE;
	return TRUE;
}

// Dessine une partie d'image rectangulaire.
// Les modes sont 0=transparent, 1=opaque.

BOOL CPixmap::DrawIconPart(int chDst, int channel, int rank, POINT pos,
						   int startY, int endY,
						   int mode, BOOL bMask)
{
	int			nbx, nby;
	RECT		rect;
	HRESULT		ddrval;
	COLORREF	oldColor1, oldColor2;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;
	if (  m_lpDDSurface[channel] == NULL )     return FALSE;

	if ( m_iconDim[channel].x == 0 ||
		 m_iconDim[channel].y == 0 )  return FALSE;

	nbx = m_totalDim[channel].x / m_iconDim[channel].x;
	nby = m_totalDim[channel].y / m_iconDim[channel].y;

	if ( rank < 0 || rank >= nbx*nby )  return FALSE;

	rect.left   = (rank%nbx)*m_iconDim[channel].x;
	rect.top    = (rank/nbx)*m_iconDim[channel].y;
	rect.right  = rect.left + m_iconDim[channel].x;
	rect.bottom = rect.top  + endY;

	pos.y    += startY;
	rect.top += startY;

	oldColor1 = m_colorSurface[2*channel+0];
	oldColor2 = m_colorSurface[2*channel+1];
	if ( bMask )  SetTransparent(channel, 0x000000ff);  // blanc
	ddrval = BltFast(chDst, channel, pos, rect, mode);
	if ( bMask )  SetTransparent2(channel, oldColor1, oldColor2);

	if ( ddrval != DD_OK )  return FALSE;
	return TRUE;
}

// Dessine une partie d'image n'importe où.
// Les modes sont 0=transparent, 1=opaque.

BOOL CPixmap::DrawPart(int chDst, int channel, POINT dest, RECT rect,
                       int mode, BOOL bMask)
{
    HRESULT ddrval;
    COLORREF oldColor1, oldColor2;

    if ( channel < 0 || channel >= MAXIMAGE ) return FALSE;
    if ( m_lpDDSurface[channel] == NULL ) return FALSE;
    oldColor1 = m_colorSurface[2*channel+0];
    oldColor2 = m_colorSurface[2*channel+1];
    if ( bMask ) SetTransparent(channel, 0x000000ff);
    ddrval = BltFast(chDst, channel, dest, rect, mode);
    if ( bMask ) SetTransparent2(channel, oldColor1, oldColor2);
    if ( ddrval != DD_OK ) return FALSE;
    return TRUE;
}

BOOL CPixmap::DrawImage(int chDst, int channel, RECT rect, int mode)
{
	POINT		dst;
	HRESULT		ddrval;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;
	if (  m_lpDDSurface[channel] == NULL )     return FALSE;

	dst.x = rect.left;
	dst.y = rect.top;

	ddrval = BltFast(chDst, channel, dst, rect, mode);

	if ( ddrval != DD_OK )  return FALSE;

	if ( channel == CHBACK )
	{
		MouseBackSave();  // sauve ce qui sera sous la souris
		m_bBackDisplayed = FALSE;
	}

	return TRUE;
}


// Construit une icône en utilisant un masque.

BOOL CPixmap::BuildIconMask(int channelMask, int rankMask,
							int channel, int rankSrc, int rankDst)
{
	int			nbx, nby;
	POINT		posDst;
	RECT		rect;
	HRESULT		ddrval;

	if ( channel < 0 || channel >= MAXIMAGE )  return FALSE;
	if (  m_lpDDSurface[channel] == NULL )     return FALSE;

	if ( m_iconDim[channel].x == 0 ||
		 m_iconDim[channel].y == 0 )  return FALSE;

	nbx = m_totalDim[channel].x / m_iconDim[channel].x;
	nby = m_totalDim[channel].y / m_iconDim[channel].y;

	if ( rankSrc < 0 || rankSrc >= nbx*nby )  return FALSE;
	if ( rankDst < 0 || rankDst >= nbx*nby )  return FALSE;

	rect.left   = (rankSrc%nbx)*m_iconDim[channel].x;
	rect.top    = (rankSrc/nbx)*m_iconDim[channel].y;
	rect.right  = rect.left + m_iconDim[channel].x;
	rect.bottom = rect.top  + m_iconDim[channel].y;
	posDst.x    = (rankDst%nbx)*m_iconDim[channel].x;
	posDst.y    = (rankDst/nbx)*m_iconDim[channel].y;
	ddrval = BltFast(m_lpDDSurface[channel], channel, posDst, rect, 1);
	if ( ddrval != DD_OK )  return FALSE;

	if ( m_iconDim[channelMask].x == 0 ||
		 m_iconDim[channelMask].y == 0 )  return FALSE;

	nbx = m_totalDim[channelMask].x / m_iconDim[channelMask].x;
	nby = m_totalDim[channelMask].y / m_iconDim[channelMask].y;

	if ( rankMask < 0 || rankMask >= nbx*nby )  return FALSE;

	rect.left   = (rankMask%nbx)*m_iconDim[channelMask].x;
	rect.top    = (rankMask/nbx)*m_iconDim[channelMask].y;
	rect.right  = rect.left + m_iconDim[channelMask].x;
	rect.bottom = rect.top  + m_iconDim[channelMask].y;
	ddrval = BltFast(m_lpDDSurface[channel], channelMask, posDst, rect, 0);
	if ( ddrval != DD_OK )  return FALSE;

	return TRUE;
}


// Affiche le pixmap à l'écran.
// Retourne FALSE en cas d'erreur.
// Port: the "screen" is the primary surface, presented by the host loop.

BOOL CPixmap::Display()
{
	RECT		MapRect;

	m_bBackDisplayed = TRUE;

	MapRect.left   = 0;
	MapRect.top    = 0;
	MapRect.right  = m_dim.x;
	MapRect.bottom = m_dim.y;

	if ( blt_fast(m_lpDDSPrimary, 0, 0, m_lpDDSBack, MapRect, FALSE) != DD_OK )  return FALSE;
	s_dirty = true;
	PortCommandCheck();
	PortHdCheck();
	s_displayCapture.write(*s_reference.surface(blupi::render::kBackBuffer), s_reference.palette());
	return TRUE;
}


// Positionne la souris et change le lutin.

void CPixmap::SetMousePosSprite(POINT pos, int sprite, BOOL bDemoPlay)
{
	if ( m_mousePos.x == pos.x &&
		 m_mousePos.y == pos.y &&
		 m_mouseSprite == sprite )  return;

	m_mousePos    = pos;
	m_mouseSprite = sprite;
	MouseHotSpot();

	if ( !bDemoPlay )
	{
		MouseUpdate();
	}
}

// Positionne la souris.

void CPixmap::SetMousePos(POINT pos, BOOL bDemoPlay)
{
	if ( m_mousePos.x == pos.x &&
		 m_mousePos.y == pos.y )  return;

	m_mousePos = pos;

	if ( !bDemoPlay )
	{
		MouseUpdate();
	}
}

// Change le lutin de la souris.

void CPixmap::SetMouseSprite(int sprite, BOOL bDemoPlay)
{
	if ( m_mouseSprite == sprite )  return;

	m_mouseSprite = sprite;
	MouseHotSpot();

	if ( !bDemoPlay )
	{
		MouseUpdate();
	}
}

// Montre ou cache la souris.

void CPixmap::MouseShow(BOOL bShow)
{
	m_bMouseShow = bShow;
}

// Met à jour le dessin de la souris.

void CPixmap::MouseUpdate()
{
	RECT	oldRect, newRect, rcRect;

	if ( m_lpDDSurface[CHPIXMAP_CURSOR] == NULL )  return;
	if ( m_mouseType != MOUSETYPEGRA )  return;
	if ( m_mouseSprite == SPRITE_EMPTY )  return;
	if ( m_mouseSprite == SPRITE_DISABLE )  return;
	if ( !m_bMouseShow )  return;

	oldRect.left   = m_mouseBackPos.x;
	oldRect.top    = m_mouseBackPos.y;
	oldRect.right  = m_mouseBackPos.x + DIMBLUPIX;
	oldRect.bottom = m_mouseBackPos.y + DIMBLUPIY;

	newRect.left   = m_mousePos.x - m_mouseHotSpot.x;
	newRect.top    = m_mousePos.y - m_mouseHotSpot.y;
	newRect.right  = newRect.left + DIMBLUPIX;
	newRect.bottom = newRect.top  + DIMBLUPIY;

	MouseBackRestore();  // enlève la souris dans m_lpDDSBack
	MouseBackDraw();     // dessine la souris dans m_lpDDSBack

	if ( m_bBackDisplayed )
	{
		if ( IntersectRect(&rcRect, &oldRect, &newRect) )
		{
			UnionRect(&rcRect, &oldRect, &newRect);
			MouseQuickDraw(rcRect);
		}
		else
		{
			MouseQuickDraw(oldRect);
			MouseQuickDraw(newRect);
		}
	}
}

// Dessine rapidement la souris dans l'écran.
// Il s'agit en fait de dessiner un petit morceau rectangulaire
// de m_lpDDSBack dans l'écran.

BOOL CPixmap::MouseQuickDraw(RECT rect)
{
	if ( rect.left   < 0       )  rect.left   = 0;
	if ( rect.right  > LXIMAGE )  rect.right  = LXIMAGE;
	if ( rect.top    < 0       )  rect.top    = 0;
	if ( rect.bottom > LYIMAGE )  rect.bottom = LYIMAGE;

	if ( blt_fast(m_lpDDSPrimary, rect.left, rect.top, m_lpDDSBack, rect, FALSE) != DD_OK )  return FALSE;
	s_dirty = true;
	return TRUE;
}

// Invalide la copie sous la souris.

void CPixmap::MouseInvalidate()
{
	m_bMouseBack = FALSE;
}

// Enlève la souris dans m_lpDDSBack.

void CPixmap::MouseBackClear()
{
	if ( m_mouseType != MOUSETYPEGRA )  return;
	if ( m_mouseSprite == SPRITE_DISABLE )  return;
	MouseBackRestore();  // enlève la souris dans m_lpDDSBack
}

// Dessine la souris dans m_lpDDSBack.

// Port: touch screens show no cursor; a tap is the click itself.
static bool s_cursorHidden = false;

void PortPixmapHideCursor(bool hidden)
{
	s_cursorHidden = hidden;
}

void CPixmap::MouseBackDraw()
{
	POINT	dst;
	RECT	rcRect;

	if ( s_cursorHidden )  return;   // port (touch screens)
	if ( m_lpDDSurface[CHPIXMAP_CURSOR] == NULL )  return;
	if ( m_mouseType != MOUSETYPEGRA )  return;
	if ( m_mouseSprite == SPRITE_EMPTY )  return;
	if ( m_mouseSprite == SPRITE_DISABLE )  return;
	if ( !m_bMouseShow )  return;

	MouseBackSave();  // sauve ce qui sera sous la souris

	dst.x  = m_mousePos.x - m_mouseHotSpot.x;
	dst.y  = m_mousePos.y - m_mouseHotSpot.y;
	MouseRectSprite(&rcRect, &dst);

	if ( dst.x < 0 )
	{
		rcRect.left -= dst.x;
		dst.x = 0;
	}
	if ( rcRect.right-rcRect.left+dst.x > LXIMAGE )
	{
		rcRect.right = rcRect.left-dst.x+LXIMAGE;
	}
	if ( dst.y < 0 )
	{
		rcRect.top -= dst.y;
		dst.y = 0;
	}
	if ( rcRect.bottom-rcRect.top+dst.y > LYIMAGE )
	{
		rcRect.bottom = rcRect.top-dst.y+LYIMAGE;
	}

	// Dessine le lutin dans m_lpDDSBack.
	BltFast(m_lpDDSBack, CHPIXMAP_CURSOR, dst, rcRect, 0);
}

// Sauve le fond sous la souris.
// m_lpDDSMouse <- m_lpDDSBack

void CPixmap::MouseBackSave()
{
	POINT		dst;
	RECT		rcRect;

	if ( m_lpDDSurface[CHPIXMAP_CURSOR] == NULL )  return;
	if ( m_mouseType != MOUSETYPEGRA )  return;
	if ( m_mouseSprite == SPRITE_EMPTY )  return;
	if ( m_mouseSprite == SPRITE_DISABLE )  return;
	if ( !m_bMouseShow )  return;

	m_mouseBackPos.x = m_mousePos.x - m_mouseHotSpot.x;
	m_mouseBackPos.y = m_mousePos.y - m_mouseHotSpot.y;
	m_bMouseBack = TRUE;

	dst.x = 0;
	dst.y = 0;

	rcRect.left   = m_mouseBackPos.x;
	rcRect.top    = m_mouseBackPos.y;
	rcRect.right  = m_mouseBackPos.x + DIMBLUPIX;
	rcRect.bottom = m_mouseBackPos.y + DIMBLUPIY;

	if ( rcRect.left < 0 )
	{
		dst.x -= rcRect.left;
		rcRect.left = 0;
	}
	if ( rcRect.right > LXIMAGE )
	{
		rcRect.right = LXIMAGE;
	}
	if ( rcRect.top < 0 )
	{
		dst.y -= rcRect.top;
		rcRect.top = 0;
	}
	if ( rcRect.bottom > LYIMAGE )
	{
		rcRect.bottom = LYIMAGE;
	}

	blt_fast(m_lpDDSMouse, dst.x, dst.y, m_lpDDSBack, rcRect, FALSE);
}

// Restitue le fond sous la souris.
// m_lpDDSBack <- m_lpDDSMouse

void CPixmap::MouseBackRestore()
{
	POINT		dst;
	RECT		rcRect;

	if ( m_lpDDSurface[CHPIXMAP_CURSOR] == NULL )  return;
	if ( !m_bMouseBack )  return;

	dst.x = m_mouseBackPos.x;
	dst.y = m_mouseBackPos.y;

	rcRect.left   = 0;
	rcRect.top    = 0;
	rcRect.right  = DIMBLUPIX;
	rcRect.bottom = DIMBLUPIY;

	if ( dst.x < 0 )
	{
		rcRect.left -= dst.x;
		dst.x = 0;
	}
	if ( dst.x+DIMBLUPIX > LXIMAGE )
	{
		rcRect.right -= (dst.x+DIMBLUPIX)-LXIMAGE;
	}
	if ( dst.y < 0 )
	{
		rcRect.top -= dst.y;
		dst.y = 0;
	}
	if ( dst.y+DIMBLUPIY > LYIMAGE )
	{
		rcRect.bottom -= (dst.y+DIMBLUPIY)-LYIMAGE;
	}

	blt_fast(m_lpDDSBack, dst.x, dst.y, m_lpDDSMouse, rcRect, FALSE);
}

// Returns the source rectangle and on-screen origin for the mouse sprite.
void CPixmap::MouseRectSprite(RECT *rcRect, POINT *pos)
{
    int rank;

    rank = 37;
    if ( m_mouseSprite == SPRITE_ARROW ) rank = 37;
    if ( m_mouseSprite == SPRITE_POINTER ) rank = 38;
    if ( m_mouseSprite == SPRITE_WAIT ) rank = 39;

    rcRect->left = IconValue(table_icon_element, rank*6+1);
    rcRect->right = rcRect->left + IconValue(table_icon_element, rank*6+5);
    rcRect->top = IconValue(table_icon_element, rank*6+2);
    rcRect->bottom = rcRect->top + IconValue(table_icon_element, rank*6+6);
    pos->x += IconValue(table_icon_element, rank*6+3);
    pos->y += IconValue(table_icon_element, rank*6+4);
}

void CPixmap::MouseHotSpot()
{
	int				rank;

	// First eight ints are the historical table. The following sixteen ints
	// were read past it by sprites 4..11 in Eggbert2.exe
	// (.data +0x555a8). Model them explicitly, without a C++ out-of-bounds read.
	static int table_mouse_hotspot[12*2] =
	{
		25, 0,
		30, 30,		// SPRITE_ARROW
		20, 15,		// SPRITE_POINTER
		30, 30,
		1954047348, 1886151214, 0, 1735745898,
		1818373733, 112, 1953789282, 808480367,
		1886151214, 0, 1819310181, 1818373743,
		112, 1835363429, 779382373, 7367778,
	};

	if ( m_mouseSprite >= SPRITE_ARROW &&
		 m_mouseSprite <= SPRITE_WAIT  )
	{
		rank = m_mouseSprite;

		m_mouseHotSpot.x = table_mouse_hotspot[rank*2];
		m_mouseHotSpot.y = table_mouse_hotspot[rank*2+1];
	}
	else
	{
		m_mouseHotSpot.x = 0;
		m_mouseHotSpot.y = 0;
	}
}


#include "pixmap/pixmap_present.h"   // the shared presentation helpers


BOOL CPixmap::BackgroundCache(int channel, char* pFilename, POINT totalDim, POINT iconDim, BOOL bUsePalette)
{	
	BOOL decor;
	char file[100];

	decor = strstr(pFilename, "blupi") != pFilename &&
		strstr(pFilename, "element") != pFilename &&
		strstr(pFilename, "explo") != pFilename &&
		strstr(pFilename, "object") != pFilename;
	if (!bUsePalette && ((decor && m_bTrueColorBack) || (!decor && m_bTrueColorDecor)))
	{
		strcpy(file, "image16\\");
		strcat(file, pFilename);
		if (Cache(channel, file, totalDim, iconDim, FALSE))
		{
			return TRUE;
		}
	}
	strcpy(file, "image08\\");
	strcat(file, pFilename);
	return Cache(channel, file, totalDim, iconDim, bUsePalette);
}


void CPixmap::SetBenchmarkSuccess(BOOL bSuccess)
{
	m_bBenchmarkSuccess = bSuccess;
}


void CPixmap::SetTrueColorBack(BOOL bTrueColor)
{
	m_bTrueColorBack = bTrueColor;
}


void CPixmap::SetTrueColorDecor(BOOL bTrueColorDecor)
{
	m_bTrueColorDecor = bTrueColorDecor;
}


BOOL CPixmap::CacheAll(BOOL cache, HWND hwnd, BOOL bFullScreen,
                     BOOL bTrueColor, BOOL bTrueColorDecor, int mouseType,
                     char* pFilename, int region)
{
    char filename[100];
    POINT dim, totalDim, iconDim;
    if (!cache)
    {
        ReleaseAll();
        hwnd = m_hWnd;
        bFullScreen = m_bFullScreen;
        bTrueColor = m_bTrueColorBack;
        bTrueColorDecor = m_bTrueColorDecor;
        mouseType = m_mouseType;
    }
    dim.x = LXIMAGE;
    dim.y = LYIMAGE;
    if (!Create(hwnd, dim, bFullScreen, bTrueColor, bTrueColorDecor, mouseType)) return FALSE;
    OutputDebug("Image: init\n");
    totalDim.x = LXIMAGE;
    totalDim.y = LYIMAGE;
    iconDim.x = 0;
    iconDim.y = 0;
    if (!BackgroundCache(0, "init.blp", totalDim, iconDim, TRUE)) return FALSE;
    OutputDebug("SavePalette\n");
    SavePalette();
    OutputDebug("InitSysPalette\n");
    InitSysPalette();
    SetDebug(FALSE);
    if (cache)
    {
        RECT rect = { 0, 0, LXIMAGE, LYIMAGE };
        DrawImage(-1, 0, rect, 1);
        Display();
    }
    else
    {
        totalDim.x = LXIMAGE;
        totalDim.y = LYIMAGE;
        iconDim.x = 0;
        iconDim.y = 0;
        if (!BackgroundCache(0, pFilename, totalDim, iconDim, FALSE)) return FALSE;
    }
	totalDim.x = 0x400;
	totalDim.y = 0x500;
	iconDim.x = 0x40;
	iconDim.y = 0x40;
	if (!BackgroundCache(1, "object.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(1, RGB(0, 0, 255));
	totalDim.x = 0x3c0;
	totalDim.y = 0x4b0;
	iconDim.x = 0x3c;
	iconDim.y = 0x3c;
	if (!BackgroundCache(2, "blupi000.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(2, RGB(0, 0, 255));
	totalDim.x = 0x3c0;
	totalDim.y = 0x4b0;
	iconDim.x = 0x3c;
	iconDim.y = 0x3c;
	if (!BackgroundCache(11, "blupi001.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(11, RGB(0, 0, 255));
	totalDim.x = 0x3c0;
	totalDim.y = 0x4b0;
	iconDim.x = 0x3c;
	iconDim.y = 0x3c;
	if (!BackgroundCache(12, "blupi002.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(12, RGB(0, 0, 255));
	totalDim.x = 0x3c0;
	totalDim.y = 0x4b0;
	iconDim.x = 0x3c;
	iconDim.y = 0x3c;
	if (!BackgroundCache(13, "blupi003.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(13, RGB(0, 0, 255));
	totalDim.x = 0x40;
	totalDim.y = 0x40;
	iconDim.x = 0x40;
	iconDim.y = 0x40;
	if (!BackgroundCache(14, "temp.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(14, RGB(0, 0, 255));
	totalDim.x = 0xd0;
	totalDim.y = 0xa0;
	iconDim.x = 0xd0;
	iconDim.y = 0xa0;
	if (!BackgroundCache(8, "map.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(8, RGB(0, 0, 255));
	totalDim.x = 0x3c0;
	totalDim.y = 0x4b0;
	iconDim.x = 0x3c;
	iconDim.y = 0x3c;
	if (!BackgroundCache(10, "element.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(10, RGB(0, 0, 255));
	totalDim.x = 0x800;
	totalDim.y = 0x280;
	iconDim.x = 0x80;
	iconDim.y = 0x80;
	if (!BackgroundCache(9, "explo.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(9, RGB(0, 0, 255));
	totalDim.x = LXIMAGE;
	totalDim.y = LYIMAGE;
	iconDim.x = 0;
	iconDim.y = 0;
	sprintf(filename, "decor%.3d.blp", region);
	if (!BackgroundCache(3, filename, totalDim, iconDim, FALSE)) return FALSE;
	totalDim.x = 0xf0;
	totalDim.y = 0x410;
	iconDim.x = 0x28;
	iconDim.y = 0x28;
	if (!BackgroundCache(4, "button00.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(4, RGB(0, 0, 255));
	totalDim.x = 0x7c;
	totalDim.y = 0x58;
	iconDim.x = 0x7c;
	iconDim.y = 0x16;
	if (!BackgroundCache(5, "jauge.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(5, RGB(0, 0, 255));
	totalDim.x = 0x100;
	totalDim.y = 0x180;
	iconDim.x = 0x10;
	iconDim.y = 0x10;
	if (!BackgroundCache(6, "text.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(6, RGB(0, 0, 255));
	totalDim.x = 0x100;
	totalDim.y = 0x60;
	iconDim.x = 0x10;
	iconDim.y = 0x0c;
	if (!BackgroundCache(7, "little.blp", totalDim, iconDim, FALSE))
		return FALSE;
	SetTransparent(7, RGB(0, 0, 255));
    Benchmark();
    return TRUE;
}


BOOL CPixmap::GetBenchmarkSuccess()
{
	return m_bBenchmarkSuccess;
}

int CPixmap::Benchmark()
{
	
	int        i, j, t1, t2, time;
	RECT    rect;
	POINT    dest;

	t1 = (int)(blupi::host::now_ns()/1000000);

	for (j = 0; j < 10; j++)
	{
		dest.x = 0;
		dest.y = 0;
		rect.left = 29;
		rect.right = 29 + LXIMAGE;
		rect.top = 29;
		rect.bottom = 29 + LYIMAGE;
		DrawPart(-1, CHDECOR, dest, rect, 1);

		dest.x = 13;  // position quelconque non-multiple de 16
		dest.y = 13;
		for (i = 0; i < 120; i++)
		{
			QuickIcon(1, 1, dest);
		}
	}

	t2 = (int)(blupi::host::now_ns()/1000000);

	if (t1 > t2)  t2 += 1000;
	time = t2 - t1;

	FILE* file = NULL;
	char        string[100];
	sprintf(string, "Benchmark = %d\r\n", time);
	// This copied SB2 method follows the host-path presentation helpers,
	// where fopen is undefined. Route its diagnostic write explicitly.
	file = blupi::vfs::open("data\\bench.blp", "wb");
	if (file == NULL)  return time;
	fwrite(string, strlen(string), 1, file);
	fclose(file);

	return time;
}
