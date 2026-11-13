// A CD audio track stand-in (platform/music.h play_track): a WAV, Ogg Vorbis or FLAC file read as
// stereo float frames at the file's own rate. WAV goes through SDL (any format SDL reads); Ogg
// Vorbis through stb_vorbis and FLAC through dr_flac (third_party/), decoded block by block while
// the track plays, so a compressed track is never held decoded in full.
#ifndef BLUPI_PLATFORM_TRACK_H
#define BLUPI_PLATFORM_TRACK_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace blupi::music
{
class Track
{
public:
    virtual ~Track() = default;
    // Appends up to `frames` stereo frames (interleaved left, right) to `out`; returns how many. Fewer
    // than asked means the end of the track.
    virtual size_t read(std::vector<float>& out, size_t frames) = 0;
    int rate = 0;   // frames per second
};

// The track in `data` (copied), recognised by its contents: "OggS" Ogg Vorbis, "fLaC" FLAC, else
// WAV. Null when it is none of them or cannot be read.
std::unique_ptr<Track> open_track(const uint8_t* data, size_t size);
}  // namespace blupi::music

#endif
