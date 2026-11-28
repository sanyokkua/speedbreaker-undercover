// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// What a 32-bit resolve writes whole (gpu/renderer.cpp's Resolve; header
// only, tests/resolve_cover_test.cpp).
//
// A 32-bit tiled destination keeps each 32x32 tile in one 4 KB block:
// Tiled2D's address holds the tile's index (row of tiles x tiles a row +
// column) in bits 12 and up and the pixel's place in the tile below them. So
// with a page-aligned destination, guest page `destPage + i` is exactly tile
// i, and a resolve whose rectangle holds that tile's 32x32 pixels rewrites
// every byte of the page:
//   - PageCovered: a scaled resolve then needn't fill the page's fresh scaled
//     slot from the 1x data first (FillFreshSlots): it stores every sample
//     of it. Deathly Hallows Part 1's shadow cascades (1024x1024 each, three
//     side by side in a 3072x1024 map whose memory the post-processing
//     buffers reuse) refilled 366 slots a frame (~0.4 ms at 2x).
//   - TextureCovered: a texture of exactly this surface (its first texel the
//     destination's 0,0, the same pitch) whose texels all lie inside the
//     rectangle holds this resolve's words alone afterwards, whatever wrote
//     its memory before: the resolve may store into it (Resolve's fused and
//     hosted textures) although its memory was written since its load.
#pragma once

#include <cstdint>

namespace gpu::resolve_cover
{
    struct Rect { int32_t x0, y0, x1, y1; };  // destination pixels, [x0, x1) x [y0, y1)

    // Page `page` (guest address >> 12) of a 32-bit resolve to `destBase`
    // (pitch `destPitch`, a multiple of 32) is all inside `r`.
    constexpr bool PageCovered(uint32_t destBase, uint32_t destPitch, const Rect& r, uint32_t page)
    {
        if ((destBase & 0xFFFu) != 0 || destPitch < 32 || (destPitch & 31u) != 0 || page < (destBase >> 12))
            return false;
        const uint32_t tile = page - (destBase >> 12), tilesPerRow = destPitch >> 5;
        const int64_t x = int64_t(tile % tilesPerRow) * 32, y = int64_t(tile / tilesPerRow) * 32;
        return r.x0 <= x && x + 32 <= r.x1 && r.y0 <= y && y + 32 <= r.y1;
    }

    // A texture `width` x `height` (texels) at the destination's 0,0, of its
    // pitch: every texel is inside `r`.
    constexpr bool TextureCovered(const Rect& r, uint32_t width, uint32_t height)
    {
        return width != 0 && height != 0 && r.x0 <= 0 && r.y0 <= 0 && int64_t(r.x1) >= int64_t(width) && int64_t(r.y1) >= int64_t(height);
    }
}
