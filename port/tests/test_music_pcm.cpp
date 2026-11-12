// CD audio stand-in (platform/music.h play_wav): a WAV track plays once at the output rate, without a
// SoundFont, and reports its end like a MIDI song (SB2's PlayCDAudio posts MM_MCINOTIFY from it).

#include "platform/music.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;

static void check(bool ok, const char* what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failures++;
}

// A mono 16-bit WAV: 0.1 s of a 440 Hz tone at 8000 Hz.
static std::vector<uint8_t> tone()
{
	const int rate = 8000, count = 800;
	std::vector<uint8_t> w(44 + count * 2);
	auto u32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) w[at + i] = uint8_t(v >> (8 * i)); };
	auto u16 = [&](size_t at, uint16_t v) { w[at] = uint8_t(v); w[at + 1] = uint8_t(v >> 8); };
	memcpy(&w[0], "RIFF", 4); u32(4, 36 + count * 2); memcpy(&w[8], "WAVEfmt ", 8);
	u32(16, 16); u16(20, 1); u16(22, 1); u32(24, rate); u32(28, rate * 2); u16(32, 2); u16(34, 16);
	memcpy(&w[36], "data", 4); u32(40, count * 2);
	for (int i = 0; i < count; i++) u16(44 + i * 2, (uint16_t)(int16_t)(16000 * std::sin(2 * 3.14159265 * 440 * i / rate)));
	return w;
}

int main()
{
	if (!SDL_Init(0)) { printf("SDL_Init failed\n"); return 1; }
	check(!blupi::music::has_soundfont(), "no SoundFont loaded");
	const std::vector<uint8_t> bad(16, 0);
	check(blupi::music::play_wav(bad.data(), bad.size()) == 0, "not a WAV: no track");

	const std::vector<uint8_t> wav = tone();
	unsigned id = blupi::music::play_wav(wav.data(), wav.size());
	check(id != 0, "a WAV track starts without a SoundFont");
	blupi::music::set_level(1.0f);
	std::vector<float> out(2 * 1200, 0.0f);
	blupi::music::render_mix(out.data(), 1200, 48000);   // 25 ms of the 100 ms track
	float peak = 0;
	for (float v : out) peak = std::fmax(peak, std::fabs(v));
	check(peak > 0.3f && peak < 0.6f, "the tone is mixed at its level (16000/32768)");
	check(blupi::music::take_finished() == 0, "not finished after 25 ms");

	int blocks = 0;
	unsigned finished = 0;
	while (finished == 0 && blocks < 100)
	{
		std::fill(out.begin(), out.end(), 0.0f);
		blupi::music::render_mix(out.data(), 1200, 48000);
		finished = blupi::music::take_finished();
		blocks++;
	}
	check(finished == id, "the end of the track is reported with its id");
	check(blocks >= 3 && blocks <= 4, "it ends after 100 ms at the output rate (resampled 8000 -> 48000 Hz)");
	check(!blupi::music::stop(id), "stopping a finished track reports it was not playing");

	unsigned second = blupi::music::play_wav(wav.data(), wav.size());
	check(second != 0 && second != id, "a second track gets a new id");
	check(blupi::music::stop(second), "stopping a playing track reports it was playing");
	std::fill(out.begin(), out.end(), 0.0f);
	blupi::music::render_mix(out.data(), 1200, 48000);
	bool silent = true;
	for (float v : out) silent = silent && v == 0.0f;
	check(silent, "nothing is mixed after the stop");

	blupi::music::shutdown();
	SDL_Quit();
	printf("%s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}
