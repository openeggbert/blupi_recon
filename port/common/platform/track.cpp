#include "platform/track.h"

// The decoders' implementations, compiled once here, from memory only.
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "third_party/stb_vorbis/stb_vorbis.c"
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_WCHAR
#include "third_party/dr_flac/dr_flac.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>

namespace blupi::music
{
namespace
{
// A WAV file: converted whole to stereo float when it is opened (as SDL reads it).
class WavTrack final : public Track
{
public:
    explicit WavTrack(std::vector<float> pcm, int frequency) : m_pcm(std::move(pcm)) { rate = frequency; }
    size_t read(std::vector<float>& out, size_t frames) override
    {
        const size_t left = (m_pcm.size() - m_at) / 2, n = std::min(frames, left);
        out.insert(out.end(), m_pcm.begin() + (ptrdiff_t)m_at, m_pcm.begin() + (ptrdiff_t)(m_at + n * 2));
        m_at += n * 2;
        return n;
    }

private:
    std::vector<float> m_pcm;
    size_t m_at = 0;
};

// Ogg Vorbis (stb_vorbis): decoded block by block. stb_vorbis fills channels a file lacks with
// silence, so mono is read as one channel and doubled; channels beyond two are dropped.
class VorbisTrack final : public Track
{
public:
    VorbisTrack(std::vector<uint8_t> data, stb_vorbis* vorbis, int frequency, int channels)
        : m_data(std::move(data)), m_vorbis(vorbis), m_mono(channels == 1) { rate = frequency; }
    ~VorbisTrack() override { stb_vorbis_close(m_vorbis); }
    size_t read(std::vector<float>& out, size_t frames) override
    {
        const int channels = m_mono ? 1 : 2;
        m_block.resize(frames * channels);
        const int got = stb_vorbis_get_samples_float_interleaved(m_vorbis, channels, m_block.data(), (int)(frames * channels));
        const size_t n = (size_t)std::max(got, 0);
        for (size_t i = 0; i < n; i++)
        {
            out.push_back(m_block[i * channels]);
            out.push_back(m_block[i * channels + channels - 1]);
        }
        return n;
    }

private:
    std::vector<uint8_t> m_data;   // stb_vorbis reads from it while the track plays
    stb_vorbis* m_vorbis;
    bool m_mono;
    std::vector<float> m_block;
};

// FLAC (dr_flac): decoded block by block; mono is doubled, further channels beyond two are dropped.
class FlacTrack final : public Track
{
public:
    FlacTrack(std::vector<uint8_t> data, drflac* flac) : m_data(std::move(data)), m_flac(flac) { rate = (int)flac->sampleRate; }
    ~FlacTrack() override { drflac_close(m_flac); }
    size_t read(std::vector<float>& out, size_t frames) override
    {
        const unsigned channels = m_flac->channels;
        m_block.resize(frames * channels);
        const size_t n = (size_t)drflac_read_pcm_frames_f32(m_flac, frames, m_block.data());
        for (size_t i = 0; i < n; i++)
        {
            const float left = m_block[i * channels];
            out.push_back(left);
            out.push_back(channels > 1 ? m_block[i * channels + 1] : left);
        }
        return n;
    }

private:
    std::vector<uint8_t> m_data;   // dr_flac reads from it while the track plays
    drflac* m_flac;
    std::vector<float> m_block;
};

std::unique_ptr<Track> open_wav(const uint8_t* data, size_t size)
{
    SDL_AudioSpec spec;
    Uint8* raw = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(data, size), true, &spec, &raw, &length)) return nullptr;
    const SDL_AudioSpec want{SDL_AUDIO_F32, 2, spec.freq};
    Uint8* converted = nullptr;
    int convertedLength = 0;
    const bool ok = SDL_ConvertAudioSamples(&spec, raw, (int)length, &want, &converted, &convertedLength);
    SDL_free(raw);
    if (!ok || spec.freq <= 0) return nullptr;
    std::vector<float> pcm((const float*)converted, (const float*)(converted + convertedLength));
    SDL_free(converted);
    return std::make_unique<WavTrack>(std::move(pcm), spec.freq);
}

std::unique_ptr<Track> open_vorbis(const uint8_t* data, size_t size)
{
    std::vector<uint8_t> copy(data, data + size);
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_memory(copy.data(), (int)copy.size(), &error, nullptr);
    if (vorbis == nullptr) return nullptr;
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    if (info.sample_rate == 0 || info.channels < 1)
    {
        stb_vorbis_close(vorbis);
        return nullptr;
    }
    return std::make_unique<VorbisTrack>(std::move(copy), vorbis, (int)info.sample_rate, info.channels);
}

std::unique_ptr<Track> open_flac(const uint8_t* data, size_t size)
{
    std::vector<uint8_t> copy(data, data + size);
    drflac* flac = drflac_open_memory(copy.data(), copy.size(), nullptr);
    if (flac == nullptr) return nullptr;
    if (flac->sampleRate == 0 || flac->channels < 1)
    {
        drflac_close(flac);
        return nullptr;
    }
    return std::make_unique<FlacTrack>(std::move(copy), flac);
}
}  // namespace

std::unique_ptr<Track> open_track(const uint8_t* data, size_t size)
{
    if (data == nullptr || size < 4) return nullptr;
    if (std::memcmp(data, "OggS", 4) == 0) return open_vorbis(data, size);
    if (std::memcmp(data, "fLaC", 4) == 0) return open_flac(data, size);
    return open_wav(data, size);
}
}  // namespace blupi::music
