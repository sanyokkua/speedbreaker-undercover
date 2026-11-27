// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Predicated tiling drawn untiled. Under MSAA the console renders a scene
// once per EDRAM tile: before each tile D3D sets the bin select (the tile's
// bins) and replays the scene's commands; a predicated packet runs when its
// bin mask (SET_BIN_MASK, set before it) shares a bin with the select. The
// renderer draws untiled (renderer.cpp TileShift): tile 0's draws cover the
// whole target and later tiles' draws are skipped. That holds only while
// every scene draw runs in tile 0. Half-Blood Prince gives each draw the
// mask of the tiles it touches (2x MSAA 1280x720 in two halves: tile 0
// selects 0x80000003, tile 1 0xC; draws 0xA2 left half, 0xA8 right half,
// 0xAA both): a right-half-only draw was skipped in tile 0 by its predicate
// and in tile 1 by the renderer, never drawn (the Burrow's tree, reeds and
// bank went missing and showed the sky as a grey wedge; objects blinked
// out as they crossed the middle of the screen).
//
// Rule: where the renderer draws every tile's draws (untiled, the tile it
// doesn't skip), a scene draw (DRAW_INDX / DRAW_INDX_2 in a colour or depth
// mode, not a resolve) runs when its mask selects any bin. Everything else,
// the per-tile state (window offset, scissor), resolves and waits, keeps
// the console's rule. Vulkan-free: tests/tile_predicate_test.cpp.
#pragma once

#include <cstdint>

namespace gpu::tile_predicate
{
    // Whether a predicated packet runs. sceneDraw: a draw in a colour or
    // depth mode; drawsEveryTile: the renderer draws all tiles' draws in
    // this tile (renderer::DrawsEveryTile).
    inline bool Runs(uint64_t binMask, uint64_t binSelect, bool sceneDraw, bool drawsEveryTile)
    {
        if ((binMask & binSelect) != 0)
            return true;
        return sceneDraw && drawsEveryTile && binMask != 0;
    }
}
