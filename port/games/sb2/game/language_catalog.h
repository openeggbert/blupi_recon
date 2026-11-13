// Speedy Blupi 2 and Speedy Eggbert 2: the data of the shared language service (common/ui/language_service.cpp,
// port/docs/LANGUAGES.md). Port-owned.
#ifndef BLUPI_GAME_LANGUAGE_CATALOG_H
#define BLUPI_GAME_LANGUAGE_CATALOG_H

#include <cstddef>

namespace blupi::language_catalog
{
struct BuiltinPack
{
	const char* file;
	const unsigned char* data;
	size_t size;
};
#include "sb2_language_packs.inc"   // k_builtinPacks[] (cmake/embed_packs.cmake)

// The game's id in a language pack ("game = ...").
constexpr const char* kGame = "sb2";
// The variant until a pack is applied, and Blupi's English pack (Eggbert's is "en").
constexpr const char* kDefaultVariant = "en";
constexpr const char* kBlupiEnglish = "en";
// Whether variant "de" asks for the German build's help-screen layout (LANGUAGE_DE).
constexpr bool kGermanLayout = false;   // SB2 German changes resources only
// An edition program's edition, whatever the data (port/docs/EDITIONS.md); NULL: from the title image.
constexpr const char* kForcedEdition = nullptr;

struct KnownBuild
{
	unsigned long long hash;   // FNV-1a 64 of the whole file
	long size;
	const char* edition;
	const char* code;
};
// SB2 executables, read statically (2026-10-23), FNV-1a 64 and size only.
inline const KnownBuild k_builds[] = {
	{0xb6a77f6d19a2108aULL, 602624, "eggbert", "en"},
	{0x3415a27364573f80ULL, 587264, "blupi", "en"},
	{0xa7b2d94bfbf344f2ULL, 589824, "blupi", "de"},
	{0xbea90abfcff97a9fULL, 589312, "blupi", "fr"},
};

// The editions. IMAGE08\INIT.BLP, the title image, tells them apart; the version is the one their
// title screen shows (docs/SB2.md); the credit is games/sb2/src/event.cpp's DrawButtons _EGAMES block.
struct Title
{
	unsigned long long hash;
	const char* edition;
	const char* version;
	const char* credit;   // a line the title screen adds, else ""
};
inline const Title k_titles[] = {
	{0x303f9019259f1abaULL, "blupi", "Version 2.2", ""},
	{0x8fb3f4b9330a6370ULL, "eggbert", "Version 1.0", "by www.epsitec.com"},
};
}  // namespace blupi::language_catalog

#endif
