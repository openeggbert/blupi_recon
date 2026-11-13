// Higher-resolution images (port/docs/HD.md): the same rendering again, `scale` times larger.
//
// ScaledDevice sits in front of the reference device. Every command goes to the reference device
// unchanged, so the game, its CPU reads (IsIconPixel), the traces and the faithful frame stay exactly
// as they are; then the command is mirrored on surfaces `scale` times larger on each axis, with every
// rectangle `scale` times larger. An image upload takes the higher-resolution image of a pack when
// CPixmap offers one (set_next_upload), else the original enlarged by pixel repetition. A stretch is
// done by the reference device and its result enlarged, so with no pack every mirrored surface is
// exactly the enlargement of the reference surface (upscale), which hd_mismatches() checks.
// The mirrored surfaces only feed the presentation.
#ifndef BLUPI_RENDER_SCALED_H
#define BLUPI_RENDER_SCALED_H

#include "render/device.h"

#include <memory>

namespace blupi::render
{
// `s` enlarged `scale` times by pixel repetition.
Surface upscale(const Surface& s, int scale);

class ScaledDevice : public Device
{
public:
    // `next` receives every command first; `reference` is the device whose surfaces it ends in
    // (the stretch result is read back from it). Both outlive this device.
    ScaledDevice(Device* next, const SoftwareDevice* reference, int scale);
    void submit(const Command& command) override;

    // The pack's image for the next Upload of `id`, already `scale` times the original's size.
    void set_next_upload(SurfaceId id, std::shared_ptr<const Surface> image);

    int scale() const { return m_scale; }
    const Surface* surface(SurfaceId id) const { return m_mirror.surface(id); }
    const Palette& palette() const { return m_mirror.palette(); }
    // Pixels of surface `id` that differ from the enlargement of the reference surface (test).
    long mismatches(SurfaceId id) const;

private:
    Device* m_next;
    const SoftwareDevice* m_reference;
    int m_scale;
    SoftwareDevice m_mirror;
    SurfaceId m_pendingId = -1;
    std::shared_ptr<const Surface> m_pending;
};
}  // namespace blupi::render

#endif
