#include "platform/music.h"

#define TSF_IMPLEMENTATION
#include "third_party/tinysoundfont/tsf.h"
#define TML_IMPLEMENTATION
#include "third_party/tinysoundfont/tml.h"

#include <SDL3/SDL.h>

#include <mutex>
#include <vector>

namespace blupi::music
{
namespace
{
// Fixed output calibration of the synthesizer "device" (the 1998 game played
// through whatever MIDI synth Windows had, at that device's level). Not a user
// setting: the game's own music volume option drives set_level() through the
// historical midiOutSetVolume table. -6 dB keeps GeneralUser GS at full option
// volume below clipping (peak 0.87 on MUSIC003).
constexpr float k_gainDb = -6.0f;

std::mutex   g_lock;
tsf*         g_font = nullptr;
int          g_rate = 0;
tml_message* g_song = nullptr;
tml_message* g_cursor = nullptr;
double       g_msec = 0.0;
unsigned     g_length = 0;     // time of the last event, ms
unsigned     g_id = 0;         // current song, 0 = none
bool         g_playing = false;
unsigned     g_nextId = 0;
unsigned     g_finished = 0;
float        g_level = 1.0f;
std::vector<float> g_block;
// A WAV track (play_wav): stereo float frames at their own rate, read with
// linear interpolation at the output rate.
std::vector<float> g_pcm;
int          g_pcmRate = 0;
double       g_pcmPos = 0.0;
bool         g_pcmPlaying = false;

void reset_channels()
{
    tsf_reset(g_font);
    for (int ch = 0; ch < 16; ch++)
    {
        tsf_channel_set_presetnumber(g_font, ch, 0, ch == 9);
    }
}

void release_song()
{
    if (g_song) tml_free(g_song);
    g_song = nullptr;
    g_cursor = nullptr;
    g_playing = false;
    g_pcm.clear();
    g_pcmPlaying = false;
}

// Mixes the WAV track; at its end the play command completed.
void render_pcm(float* out, int frames, int rate)
{
    const double step = (double)g_pcmRate / rate;
    const size_t count = g_pcm.size() / 2;
    for (int i = 0; i < frames && g_pcmPlaying; i++)
    {
        const size_t k = (size_t)g_pcmPos;
        if (k >= count)
        {
            g_pcmPlaying = false;
            g_finished = g_id;
            break;
        }
        const double f = g_pcmPos - (double)k;
        const size_t next = k + 1 < count ? k + 1 : k;
        for (int c = 0; c < 2; c++)
        {
            const float a = g_pcm[k * 2 + c], b = g_pcm[next * 2 + c];
            out[(size_t)i * 2 + c] += (float)(a + (b - a) * f) * g_level;
        }
        g_pcmPos += step;
    }
}

void dispatch(const tml_message* m)
{
    switch (m->type)
    {
    case TML_PROGRAM_CHANGE: tsf_channel_set_presetnumber(g_font, m->channel, m->program, m->channel == 9); break;
    case TML_NOTE_ON:        tsf_channel_note_on(g_font, m->channel, m->key, m->velocity / 127.0f); break;
    case TML_NOTE_OFF:       tsf_channel_note_off(g_font, m->channel, m->key); break;
    case TML_PITCH_BEND:     tsf_channel_set_pitchwheel(g_font, m->channel, m->pitch_bend); break;
    case TML_CONTROL_CHANGE: tsf_channel_midi_control(g_font, m->channel, m->control, m->control_value); break;
    default: break;
    }
}
}  // namespace

bool load_soundfont(const std::string& path)
{
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (g_font) return true;
    }
    // Through SDL, so that an Android APK asset ("soundfont.sf2") loads too.
    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) return false;
    tsf* font = tsf_load_memory(data, (int)size);
    SDL_free(data);
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_font == nullptr) g_font = font;
    else if (font) tsf_close(font);
    return g_font != nullptr;
}

bool has_soundfont()
{
    std::lock_guard<std::mutex> guard(g_lock);
    return g_font != nullptr;
}

unsigned play(const uint8_t* data, size_t size)
{
    tml_message* song = tml_load_memory(data, (int)size);
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_font == nullptr || song == nullptr)
    {
        if (song) tml_free(song);
        return 0;
    }
    release_song();
    reset_channels();
    g_song = song;
    g_cursor = song;
    g_msec = 0.0;
    g_length = 0;
    tml_get_info(song, nullptr, nullptr, nullptr, nullptr, &g_length);
    g_playing = true;
    g_id = ++g_nextId;
    if (g_id == 0) g_id = ++g_nextId;
    return g_id;
}

unsigned play_wav(const uint8_t* data, size_t size)
{
    SDL_AudioSpec spec;
    Uint8* raw = nullptr;
    Uint32 length = 0;
    if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(data, size), true, &spec, &raw, &length)) return 0;
    const SDL_AudioSpec want{SDL_AUDIO_F32, 2, spec.freq};
    Uint8* converted = nullptr;
    int convertedLength = 0;
    const bool ok = SDL_ConvertAudioSamples(&spec, raw, (int)length, &want, &converted, &convertedLength);
    SDL_free(raw);
    if (!ok || spec.freq <= 0) return 0;
    std::vector<float> pcm((const float*)converted, (const float*)(converted + convertedLength));
    SDL_free(converted);
    std::lock_guard<std::mutex> guard(g_lock);
    release_song();
    if (g_font) reset_channels();
    g_pcm.swap(pcm);
    g_pcmRate = spec.freq;
    g_pcmPos = 0.0;
    g_pcmPlaying = true;
    g_id = ++g_nextId;
    if (g_id == 0) g_id = ++g_nextId;
    return g_id;
}

bool stop(unsigned id)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (id == 0 || id != g_id) return false;
    bool wasPlaying = g_playing || g_pcmPlaying;
    release_song();
    if (g_font) reset_channels();
    g_id = 0;
    if (g_finished == id) g_finished = 0;
    return wasPlaying;
}

unsigned take_finished()
{
    std::lock_guard<std::mutex> guard(g_lock);
    unsigned id = g_finished;
    g_finished = 0;
    return id;
}

void set_level(float level)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_level = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
}

void render_mix(float* out, int frames, int rate)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (g_pcmPlaying)
    {
        render_pcm(out, frames, rate);
        return;
    }
    if (g_font == nullptr || (!g_playing && tsf_active_voice_count(g_font) == 0)) return;
    if (rate != g_rate)
    {
        tsf_set_output(g_font, TSF_STEREO_INTERLEAVED, rate, k_gainDb);
        g_rate = rate;
    }
    g_block.resize((size_t)TSF_RENDER_EFFECTSAMPLEBLOCK * 2);
    for (int done = 0; done < frames; done += TSF_RENDER_EFFECTSAMPLEBLOCK)
    {
        int n = frames - done < TSF_RENDER_EFFECTSAMPLEBLOCK ? frames - done : TSF_RENDER_EFFECTSAMPLEBLOCK;
        if (g_playing)
        {
            g_msec += n * 1000.0 / rate;
            while (g_cursor && g_msec >= g_cursor->time)
            {
                dispatch(g_cursor);
                g_cursor = g_cursor->next;
            }
            // End of the sequence: the MCI sequencer completed the play command here.
            if (g_cursor == nullptr && g_msec >= g_length)
            {
                g_playing = false;
                g_finished = g_id;
            }
        }
        tsf_render_float(g_font, g_block.data(), n, 0);
        for (int i = 0; i < n * 2; i++) out[(size_t)done * 2 + i] += g_block[i] * g_level;
    }
}

void shutdown()
{
    std::lock_guard<std::mutex> guard(g_lock);
    release_song();
    if (g_font) tsf_close(g_font);
    g_font = nullptr;
    g_id = 0;
    g_rate = 0;
}
}  // namespace blupi::music
