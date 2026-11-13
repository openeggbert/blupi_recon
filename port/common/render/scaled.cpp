#include "render/scaled.h"

#include <cstring>

namespace blupi::render
{
namespace
{
Rect scaled(const Rect& r, int k)
{
    return Rect{r.left * k, r.top * k, r.right * k, r.bottom * k};
}
}  // namespace

Surface upscale(const Surface& s, int scale)
{
    Surface out(s.width() * scale, s.height() * scale, s.bpp());
    const size_t pixel = (size_t)s.bpp() / 8;
    for (int y = 0; y < s.height(); y++)
    {
        uint8_t* first = out.row(y * scale);
        const uint8_t* in = s.row(y);
        for (int x = 0; x < s.width(); x++)
            for (int i = 0; i < scale; i++) std::memcpy(first + ((size_t)x * scale + i) * pixel, in + (size_t)x * pixel, pixel);
        for (int i = 1; i < scale; i++) std::memcpy(out.row(y * scale + i), first, (size_t)out.width() * pixel);
    }
    return out;
}

ScaledDevice::ScaledDevice(Device* next, const SoftwareDevice* reference, int scale)
    : m_next(next), m_reference(reference), m_scale(scale < 1 ? 1 : scale)
{
}

void ScaledDevice::set_next_upload(SurfaceId id, std::shared_ptr<const Surface> image)
{
    m_pendingId = id;
    m_pending = std::move(image);
}

void ScaledDevice::submit(const Command& c)
{
    m_next->submit(c);
    const int k = m_scale;
    switch (c.kind)
    {
    case Command::Create:
        m_mirror.submit(make_create(c.dst, c.width * k, c.height * k, c.bpp));
        break;
    case Command::Destroy:
    case Command::Palette:
        m_mirror.submit(c);
        break;
    case Command::Upload:
        if (m_pending && m_pendingId == c.dst && m_pending->width() == c.pixels->width() * k &&
            m_pending->height() == c.pixels->height() * k && m_pending->bpp() == c.pixels->bpp())
            m_mirror.submit(make_upload(c.dst, *m_pending));
        else
            m_mirror.submit(make_upload(c.dst, upscale(*c.pixels, k)));
        m_pending.reset();
        m_pendingId = -1;
        break;
    case Command::Blit:
    {
        Command m = c;
        m.dstRect.left *= k;
        m.dstRect.top *= k;
        m.srcRect = scaled(c.srcRect, k);
        m_mirror.submit(m);
        break;
    }
    case Command::Stretch:
    {
        // The reference result, enlarged: the stretch's own sampling at the higher resolution would
        // not be an enlargement of it (the editor's minimap, DrawMap).
        const Surface* done = m_reference->surface(c.dst);
        const Surface* mirror = m_mirror.surface(c.dst);
        if (done == nullptr || mirror == nullptr) break;
        Rect r = c.dstRect;
        if (r.left < 0) r.left = 0;
        if (r.top < 0) r.top = 0;
        if (r.right > done->width()) r.right = done->width();
        if (r.bottom > done->height()) r.bottom = done->height();
        if (r.width() <= 0 || r.height() <= 0) break;
        Surface part(r.width(), r.height(), done->bpp());
        blit(part, 0, 0, *done, r, ColorKey());
        const Surface big = upscale(part, k);
        m_mirror.submit(make_create(-2, big.width(), big.height(), big.bpp()));
        m_mirror.submit(make_upload(-2, big));
        m_mirror.submit(make_blit(c.dst, r.left * k, r.top * k, -2, Rect{0, 0, big.width(), big.height()}, ColorKey()));
        m_mirror.submit(make_destroy(-2));
        break;
    }
    }
}

long ScaledDevice::mismatches(SurfaceId id) const
{
    const Surface* ref = m_reference->surface(id);
    const Surface* big = m_mirror.surface(id);
    if (ref == nullptr || big == nullptr) return ref == big ? 0 : -1;
    if (big->width() != ref->width() * m_scale || big->height() != ref->height() * m_scale) return -1;
    long count = 0;
    for (int y = 0; y < big->height(); y++)
        for (int x = 0; x < big->width(); x++)
            if (big->pixel(x, y) != ref->pixel(x / m_scale, y / m_scale)) count++;
    return count;
}
}  // namespace blupi::render
