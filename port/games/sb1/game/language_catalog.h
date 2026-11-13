// Speedy Blupi 1 and Speedy Eggbert 1 (and the edition programs): the data of the shared language service (common/ui/language_service.cpp,
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
#include "language_packs.inc"   // k_builtinPacks[] (cmake/embed_packs.cmake)

// The game's id in a language pack ("game = ...").
constexpr const char* kGame = "sb1";
// The variant until a pack is applied, and Blupi's English pack (Eggbert's is "en").
constexpr const char* kDefaultVariant = "us";
constexpr const char* kBlupiEnglish = "us";
// Whether variant "de" asks for the German build's help-screen layout (LANGUAGE_DE).
constexpr bool kGermanLayout = true;
// An edition program's edition, whatever the data (port/docs/EDITIONS.md); NULL: from the title image.
#if BLUPI_EDITION_KEY == 1 || BLUPI_EDITION_KEY == 2
constexpr const char* kForcedEdition = "eggbert";   // the eGames Demo and Valueware programs
#elif BLUPI_EDITION_KEY == 3
constexpr const char* kForcedEdition = "blupi";     // the June 1998 demo program
#else
constexpr const char* kForcedEdition = nullptr;
#endif

struct KnownBuild
{
	unsigned long long hash;   // FNV-1a 64 of the whole file
	long size;
	const char* edition;
	const char* code;
};
// The executables of the editions and languages we know (2026-10-22).
inline const KnownBuild k_builds[] = {
	{0xd640855283598e8aULL, 535552, "blupi", "us"},
	{0x610b8c906f647304ULL, 538112, "blupi", "de"},
	{0xbef9df9c1ae3cf57ULL, 538112, "blupi", "fr"},
	{0xf8f0339277cb092dULL, 534528, "eggbert", "en"},
	{0xce7ac17f5f5dcd4cULL, 537088, "eggbert", "de"},
	{0xf7a88c99973366f9ULL, 537088, "eggbert", "es"},
	{0xd0c7f8ae9ceb08c7ULL, 537088, "eggbert", "fr"},
	{0xce181f1e1c988a20ULL, 536576, "eggbert", "it"},
	{0x459defd64dd9b032ULL, 536576, "eggbert", "pt"},
};

// The editions. IMAGE08\INIT.BLP, the title image, tells them apart; the version is the one their
// title screen shows (docs/EGGBERT.md).
struct Title
{
	unsigned long long hash;
	const char* edition;
	const char* version;
	const char* credit;   // a line the title screen adds, else ""
};
inline const Title k_titles[] = {
	{0xe9654ea706e78a9bULL, "blupi", "Version 1.0", ""},
	{0x847cfb0febbd955aULL, "eggbert", "Version 1.1", ""},
};
}  // namespace blupi::language_catalog

#endif
