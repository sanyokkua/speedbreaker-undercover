// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Clears drawn as rectangles, apart from Vulkan, so tests/draw_clear_test.cpp
// can check them (renderer.cpp, PropagateDrawClear).
//
// EDRAM is one memory, but the renderer keeps each layout (base, format,
// pitch) as an image of its own. Xbox 360 Direct3D clears a surface with a
// draw: one rectangle, no blending, depth ALWAYS and written, stencil ALWAYS
// and REPLACE, the value from the vertices. It draws that rectangle at 4x
// MSAA and half the pitch when it can: a 640x368 4x rectangle covers the
// same EDRAM tiles as 1280x736 at 1x (a 4x pixel is 2x2 samples), at a
// quarter of the pixels. Order of the Phoenix clears its 1280x736 scene (1x,
// pitch 1280) that way each frame: the clear landed in the pitch-640 4x
// image only, the scene's colour and reversed depth (GEQUAL) were never
// cleared, moving geometry failed the depth test against old frames, and
// the bloom added onto what stayed, frame after frame, to yellow and white.
//
// So a draw that is a clear (Classify) also fills every other layout at the
// same EDRAM base whose rows are the same EDRAM rows (SameRows), over the
// tiles the rectangle covered (MapRect). A clear writes one value, so the
// copy is exact whatever the layouts' sample order.
#pragma once

#include <algorithm>
#include <cstdint>

namespace gpu::draw_clear
{
    // MSAA as the surface register has it: 0 1x, 1 2x (rows doubled), 2 4x
    // (rows and columns doubled).
    inline uint32_t SamplesX(uint32_t msaa) { return msaa >= 2 ? 2u : 1u; }
    inline uint32_t SamplesY(uint32_t msaa) { return msaa >= 1 ? 2u : 1u; }

    // EDRAM tiles a row of tiles of this layout takes (80x16 samples a tile).
    inline uint32_t TilesPerRow(uint32_t pitch, uint32_t msaa, bool is64bpp)
    {
        return (pitch * SamplesX(msaa) + 79) / 80 * (is64bpp ? 2u : 1u);
    }

    // Two layouts at one base whose pixels map onto each other by scale: the
    // same tiles in a row (so the same EDRAM row of tiles holds the same
    // rows of samples in both).
    inline bool SameRows(uint32_t pitchA, uint32_t msaaA, uint32_t pitchB, uint32_t msaaB, bool is64bpp)
    {
        return TilesPerRow(pitchA, msaaA, is64bpp) == TilesPerRow(pitchB, msaaB, is64bpp);
    }

    struct Rect
    {
        int32_t x0, y0, x1, y1;
    };

    // A rectangle of `from` pixels as the pixels of `to` over the same
    // samples (rounded out: a partly covered pixel of `to` is included only
    // when the rectangle covers all its samples, rounded in).
    inline Rect MapRect(const Rect& r, uint32_t msaaFrom, uint32_t msaaTo)
    {
        const int32_t fx = int32_t(SamplesX(msaaFrom)), fy = int32_t(SamplesY(msaaFrom));
        const int32_t tx = int32_t(SamplesX(msaaTo)), ty = int32_t(SamplesY(msaaTo));
        auto up = [](int32_t v, int32_t d) { return (v + d - 1) / d; };
        Rect o{ up(r.x0 * fx, tx), up(r.y0 * fy, ty), r.x1 * fx / tx, r.y1 * fy / ty };
        if (o.x1 < o.x0)
            o.x1 = o.x0;
        if (o.y1 < o.y0)
            o.y1 = o.y0;
        return o;
    }

    inline constexpr uint32_t kEdramTiles = 2048;  // 10 MB of 5120-byte tiles

    // The part of `r` (pixels of a layout at `base`, `pitch`, `msaa`) the
    // console's clear can have written: the pitch's columns and the rows
    // left in EDRAM after `base`. Untiled rendering draws tile 0 over the
    // whole target and its scissor is the whole target, so a clear drawn
    // without a viewport transform (Half-Blood Prince's 320x736 4x clear:
    // VTE 0x300, scissor 8192) reached the renderer as the target's 1280x2048:
    // the copy took the stale columns past the pitch with it, squeezed into
    // the other layout's width, and the scene showed only in its first 320
    // columns, old frames beyond.
    inline Rect EdramBounds(Rect r, uint32_t base, uint32_t pitch, uint32_t msaa, bool is64bpp)
    {
        const uint32_t perRow = TilesPerRow(pitch, msaa, is64bpp);
        const uint32_t tileRows = base < kEdramTiles && perRow ? (kEdramTiles - base) / perRow : 0;
        const int32_t rows = int32_t(tileRows * 16 / SamplesY(msaa));
        r.x1 = std::min(r.x1, int32_t(pitch));
        r.y1 = std::min(r.y1, rows);
        r.x0 = std::min(r.x0, r.x1);
        r.y0 = std::min(r.y0, r.y1);
        return r;
    }

    // The draw's state that decides whether it is a clear.
    struct Draw
    {
        bool rectangleList = false;
        bool pixelShaderSamples = false;  // fetches a texture
        bool pixelShaderKills = false;
        bool pixelShaderWritesDepth = false;
        bool alphaTest = false;
        uint32_t colorWriteMask = 0;      // RB_COLOR_MASK, 4 bits a target, only targets attached
        uint32_t blendControl[4] = {};    // RB_BLENDCONTROL0-3
        bool depthAttached = false;
        uint32_t depthControl = 0;        // RB_DEPTHCONTROL
        uint32_t stencilWriteMask = 0;    // RB_STENCILREFMASK bits 16-23
    };

    struct Cleared
    {
        uint32_t colorTargets = 0;  // bit i: colour target i written with all of its components, unblended
        bool depth = false;
        bool stencil = false;
        bool Any() const { return colorTargets || depth; }
    };

    inline constexpr uint32_t kBlendOff = 0x00010001;  // ONE, ADD, ZERO for colour and alpha

    inline Cleared Classify(const Draw& d)
    {
        Cleared c;
        if (!d.rectangleList || d.pixelShaderSamples || d.pixelShaderKills || d.pixelShaderWritesDepth || d.alphaTest)
            return c;
        for (uint32_t i = 0; i < 4; i++)
            if (((d.colorWriteMask >> (4 * i)) & 0xF) == 0xF && (d.blendControl[i] & 0x1FFF1FFF) == kBlendOff)
                c.colorTargets |= 1u << i;
        if (d.depthAttached)
        {
            const uint32_t dc = d.depthControl;
            const bool zEnable = (dc >> 1) & 1, zWrite = (dc >> 2) & 1, zAlways = ((dc >> 4) & 7) == 7;
            c.depth = zEnable && zWrite && zAlways;
            // Stencil: ALWAYS and REPLACE on the depth pass (with depth
            // ALWAYS the only outcome), all bits written. Otherwise left as
            // it is in the other layouts.
            const bool stencilEnable = dc & 1;
            const uint32_t func = (dc >> 8) & 7, zpass = (dc >> 14) & 7;
            c.stencil = c.depth && stencilEnable && func == 7 && zpass == 2 && (d.stencilWriteMask & 0xFF) == 0xFF;
        }
        return c;
    }
}
