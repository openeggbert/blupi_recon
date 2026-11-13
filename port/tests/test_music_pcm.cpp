// CD audio stand-in (platform/music.h play_track): a WAV, Ogg Vorbis or FLAC track plays once at the
// output rate, without a SoundFont, and reports its end like a MIDI song (SB2's PlayCDAudio posts
// MM_MCINOTIFY from it). argv[1] is tests/audio: tone.ogg and tone.flac hold the WAV test's tone
// (440 Hz, 8000 Hz mono, 0.1 s, level 0.488: FFmpeg's sine is 1/8 times 3.904), stereo.ogg and stereo.flac 0.25 s at 44100 Hz stereo,
// all made with FFmpeg 7.1:
//   ffmpeg -f lavfi -i sine=frequency=440:sample_rate=8000:duration=0.1 -af volume=3.904 -ac 1 \
//          -c:a libvorbis -q:a 4 tone.ogg            (-sample_fmt s16 -c:a flac tone.flac)
//   ffmpeg -f lavfi -i sine=frequency=440:sample_rate=44100:duration=0.25 -ac 2 \
//          -c:a libvorbis stereo.ogg                 (-sample_fmt s16 -c:a flac stereo.flac)

#include "platform/music.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool ok, const char* what)
{
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) failures++;
}

// A mono 16-bit WAV: `count` frames of a 440 Hz tone at 8000 Hz (0.1 s by default).
static std::vector<uint8_t> tone(int count = 800)
{
	const int rate = 8000;
	std::vector<uint8_t> w(44 + count * 2);
	auto u32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; i++) w[at + i] = uint8_t(v >> (8 * i)); };
	auto u16 = [&](size_t at, uint16_t v) { w[at] = uint8_t(v); w[at + 1] = uint8_t(v >> 8); };
	memcpy(&w[0], "RIFF", 4); u32(4, 36 + count * 2); memcpy(&w[8], "WAVEfmt ", 8);
	u32(16, 16); u16(20, 1); u16(22, 1); u32(24, rate); u32(28, rate * 2); u16(32, 2); u16(34, 16);
	memcpy(&w[36], "data", 4); u32(40, count * 2);
	for (int i = 0; i < count; i++) u16(44 + i * 2, (uint16_t)(int16_t)(16000 * std::sin(2 * 3.14159265 * 440 * i / rate)));
	return w;
}

static std::vector<uint8_t> file(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Plays a track to its end at 48000 Hz in blocks of 1200 frames: the number of blocks and, per
// channel, the peak level.
struct Played { unsigned id = 0; int blocks = 0; bool finished = false; float peak[2] = {0, 0}; };
static Played play_through(const std::vector<uint8_t>& data)
{
	Played p;
	p.id = blupi::music::play_track(data.data(), data.size());
	if (p.id == 0) return p;
	std::vector<float> out(2 * 1200);
	while (!p.finished && p.blocks < 200)
	{
		std::fill(out.begin(), out.end(), 0.0f);
		blupi::music::render_mix(out.data(), 1200, 48000);
		for (size_t i = 0; i < out.size(); i++) p.peak[i % 2] = std::fmax(p.peak[i % 2], std::fabs(out[i]));
		p.finished = blupi::music::take_finished() == p.id;
		p.blocks++;
	}
	return p;
}

int main(int argc, char** argv)
{
	if (!SDL_Init(0)) { printf("SDL_Init failed\n"); return 1; }
	const std::string audio = argc > 1 ? argv[1] : "";
	check(!blupi::music::has_soundfont(), "no SoundFont loaded");
	const std::vector<uint8_t> bad(16, 0);
	check(blupi::music::play_track(bad.data(), bad.size()) == 0, "not a WAV, Ogg Vorbis or FLAC file: no track");

	const std::vector<uint8_t> wav = tone();
	unsigned id = blupi::music::play_track(wav.data(), wav.size());
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

	unsigned second = blupi::music::play_track(wav.data(), wav.size());
	check(second != 0 && second != id, "a second track gets a new id");
	check(blupi::music::stop(second), "stopping a playing track reports it was playing");
	std::fill(out.begin(), out.end(), 0.0f);
	blupi::music::render_mix(out.data(), 1200, 48000);
	bool silent = true;
	for (float v : out) silent = silent && v == 0.0f;
	check(silent, "nothing is mixed after the stop");

	// A long WAV crosses the decoding window's blocks of 4096 frames: the output is exactly the
	// linear interpolation of the whole converted track at absolute positions, as before the window.
	{
		const std::vector<uint8_t> longWav = tone(10000);
		SDL_AudioSpec spec;
		Uint8* raw = nullptr;
		Uint32 length = 0;
		Uint8* converted = nullptr;
		int convertedLength = 0;
		SDL_LoadWAV_IO(SDL_IOFromConstMem(longWav.data(), longWav.size()), true, &spec, &raw, &length);
		const SDL_AudioSpec want{SDL_AUDIO_F32, 2, spec.freq};
		SDL_ConvertAudioSamples(&spec, raw, (int)length, &want, &converted, &convertedLength);
		SDL_free(raw);
		const std::vector<float> pcm((const float*)converted, (const float*)(converted + convertedLength));
		SDL_free(converted);
		std::vector<float> expected;
		const size_t count = pcm.size() / 2;
		for (double pos = 0.0; (size_t)pos < count; pos += 8000.0 / 48000)
		{
			const size_t k = (size_t)pos, next = k + 1 < count ? k + 1 : k;
			const double f = pos - (double)k;
			for (int c = 0; c < 2; c++)
				expected.push_back((float)(pcm[k * 2 + c] + (pcm[next * 2 + c] - pcm[k * 2 + c]) * f));
		}
		const unsigned longId = blupi::music::play_track(longWav.data(), longWav.size());
		std::vector<float> got;
		for (int b = 0; b < 60 && blupi::music::take_finished() != longId; b++)
		{
			std::fill(out.begin(), out.end(), 0.0f);
			blupi::music::render_mix(out.data(), 1200, 48000);
			got.insert(got.end(), out.begin(), out.end());
		}
		got.resize(std::min(got.size(), expected.size() + 2));
		bool same = got.size() >= expected.size();
		for (size_t i = 0; same && i < expected.size(); i++) same = got[i] == expected[i];
		check(longId != 0 && same, "a 10000-frame WAV gives the same samples across the decoding blocks");
	}

	if (audio.empty())
	{
		check(false, "the tests/audio folder is named on the command line");
	}
	else
	{
		for (const char* name : {"tone.ogg", "tone.flac"})
		{
			const Played p = play_through(file(audio + "/" + name));
			const std::string what = std::string(name) + ": ";
			check(p.id != 0, (what + "plays without a SoundFont").c_str());
			check(p.peak[0] > 0.4f && p.peak[0] < 0.6f && std::fabs(p.peak[0] - p.peak[1]) < 1e-6f,
				  (what + "the mono tone at its level (0.488) on both channels").c_str());
			check(p.finished && p.blocks >= 4 && p.blocks <= 5, (what + "ends after 0.1 s").c_str());
		}
		for (const char* name : {"stereo.ogg", "stereo.flac"})
		{
			const Played p = play_through(file(audio + "/" + name));
			const std::string what = std::string(name) + ": ";
			check(p.id != 0 && p.peak[0] > 0.05f && p.peak[1] > 0.05f, (what + "both channels play").c_str());
			check(p.finished && p.blocks >= 10 && p.blocks <= 11, (what + "ends after 0.25 s (44100 -> 48000 Hz)").c_str());
		}
		std::vector<uint8_t> cut = file(audio + "/tone.flac");
		cut.resize(40);
		check(blupi::music::play_track(cut.data(), cut.size()) == 0, "a FLAC file cut inside its header: no track");
		cut = file(audio + "/tone.ogg");
		cut.resize(60);
		check(blupi::music::play_track(cut.data(), cut.size()) == 0, "an Ogg file cut inside its headers: no track");
	}

	blupi::music::shutdown();
	SDL_Quit();
	printf("%s\n", failures == 0 ? "all checks passed" : "FAILED");
	return failures == 0 ? 0 : 1;
}
