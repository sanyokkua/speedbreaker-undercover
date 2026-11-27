// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The display scaler's shape for the front buffer. A game may render a front
// buffer narrower than the display and have the console's scaler stretch it
// (Harry Potter and the Deathly Hallows - Part 1: 1152x720, scaled to
// 1280x720 by VdInitializeScalerCommandBuffer, source 0x048002D0, output
// 0x050002D0). The game draws its 16:9 view into those 1152 columns, so the
// picture must be shown 16:9, not at the buffer's own 1.6:1 (10% too narrow,
// with side bars). Header only: tests/scaler_test.cpp checks it.
#pragma once
#include <cstdint>

namespace gpu::scaler
{
    struct Shape
    {
        uint32_t width = 0, height = 0;
    };

    // VdInitializeScalerCommandBuffer packs each size as width << 16 | height.
    inline Shape Unpack(uint32_t wh) { return { wh >> 16, wh & 0xFFFF }; }

    // The shape a front buffer of frontW x frontH is shown at: the scaler's
    // output when its source is exactly that buffer (the case the console
    // stretches), else the buffer's own (no scaler set, or a source rect that
    // is part of the buffer: left as it was).
    inline Shape DisplayShape(uint32_t frontW, uint32_t frontH, uint32_t sourceWH, uint32_t outputWH)
    {
        const Shape source = Unpack(sourceWH), output = Unpack(outputWH);
        if (source.width == frontW && source.height == frontH && output.width && output.height)
            return output;
        return { frontW, frontH };
    }

    // The game's last scaler setup (gpu/imports_video.cpp), applied to a
    // front buffer of that size. Any thread.
    Shape CurrentDisplayShape(uint32_t frontW, uint32_t frontH);
}
