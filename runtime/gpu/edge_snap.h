// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Edge snap: full-screen quads at a scaled internal resolution with Direct3D
// 9 pixel centres (PA_SU_VTX_CNTL pix_center = kD3DZero). Vulkan-free:
// tests/edge_snap_test.cpp checks it alone; the vertex shader epilogue
// (gpu/shader_translator.cpp) applies SnapNdc, gpu/renderer.cpp sets the
// push constant from HalfPixelNdc.
//
// Under Direct3D 9 rules guest pixel i is centred on i and covers
// [i - 0.5, i + 0.5]; the renderer adds 0.5 to every position so the host's
// centres (at +0.5) line up. A quad drawn from x = 0 (a fade to black, a
// full-screen pass written without the -0.5 correction) has its left edge
// through the centre of column 0: the console's 1x rasteriser draws the
// whole column (a centre on a left or top edge is in), but at 2x or 3x the
// host pixels of column 0 left of its centre fall outside and keep what was
// there before. OotP's pause and loading fades left the scene showing in
// one host row and column along the top and left of a black screen
// (row 0 mean 90 of 255 in the 1x copy at 3x; 0 at 1x).
//
// The fix moves a screen-space vertex (w = 1) lying on the target's left or
// top edge in guest pixels onto the target's edge (D3D -0.5): the column
// and row are covered as the console covers them. Nothing else moves (3D
// vertices have w != 1; a quad that already starts at -0.5 is on the edge),
// and at 1x it is off: a centre on the edge is in there.
#pragma once
#include <cmath>
#include <cstdint>

namespace gpu::edge_snap
{
    // Half a guest pixel in NDC for a target `guestSize` pixels across (the
    // NDC span is 2), when the axis is scaled and positions use D3D 9 centres;
    // 0 turns the snap off.
    inline float HalfPixelNdc(bool d3dCentres, float scale, uint32_t guestSize)
    {
        return d3dCentres && scale > 1.0f && guestSize ? 1.0f / float(guestSize) : 0.0f;
    }

    // The shader's rule (shader_translator.cpp's epilogue), for tests: `ndc`
    // is the position after the guest viewport (with the +0.5), `w` its w.
    inline float SnapNdc(float ndc, float w, float halfPixel)
    {
        if (halfPixel > 0.0f && std::fabs(w - 1.0f) < 1e-5f && std::fabs(ndc + 1.0f - halfPixel) < halfPixel * 0.125f)
            return -1.0f;
        return ndc;
    }
}
