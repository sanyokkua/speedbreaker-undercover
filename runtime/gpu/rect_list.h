// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Rectangle lists (primitive type 8): three corners a rectangle, the GPU
// makes the fourth. The vertex shader (gpu/shader_translator.cpp) draws each
// rectangle as two triangles of 3 vertices: the given ones (v0 v1 v2), then
// the two ends of the diagonal with the fourth corner between them. The
// fourth corner is the corner at the right angle reflected across the
// diagonal. Vulkan-free: tests/rect_list_test.cpp checks it alone and reads
// the shader for the same rule.
//
// Direct3D gives the three corners in either of two orders (Xenia handles
// both): the right angle at v0 (0,0 w,0 0,h; Order of the Phoenix's clears)
// or at v1 (0,0 w,0 w,h; Half-Blood Prince's clears and resolves). The
// shader used to assume v0 (v3 = v1 + v2 - v0, triangles 012 and 132):
// Half-Blood Prince's per-frame scene clear (a 4x MSAA rectangle at half
// pitch, gpu/draw_clear.h) then drew the parallelogram 0,0 w,0 w,h 2w,h and
// left a triangle of the scene's depth and colour uncleared, and the menus'
// 3D backdrop showed only in a diagonal panel; with only the 4th corner
// fixed, triangles 012 and 132 still missed the triangle left of both
// diagonals (the second triangle must share the first one's diagonal).
//
// The diagonal is the longest edge (in any order and any axis scale: an
// axis-aligned rectangle stays one); a tie (a degenerate rectangle) keeps v0.
#pragma once

namespace gpu::rect_list
{
    // The corner at the right angle: 0, 1 or 2.
    inline unsigned RightAngleCorner(const float v0[2], const float v1[2], const float v2[2])
    {
        auto d = [](const float* a, const float* b) { return (b[0] - a[0]) * (b[0] - a[0]) + (b[1] - a[1]) * (b[1] - a[1]); };
        const float d01 = d(v0, v1), d02 = d(v0, v2), d12 = d(v1, v2);
        if (d02 > d01 && d02 > d12)
            return 1;
        if (d01 > d02 && d01 > d12)
            return 2;
        return 0;
    }

    // The 6 vertices drawn for a rectangle (two triangles, the same winding):
    // v0 v1 v2, then diagonal end a, the fourth corner, diagonal end b, where
    // a = corner + 1 and b = corner + 2 (mod 3), the fourth = a + b - corner.
    inline void Expand(const float v[3][2], float out[6][2])
    {
        const unsigned c = RightAngleCorner(v[0], v[1], v[2]), a = (c + 1) % 3, b = (c + 2) % 3;
        for (int k = 0; k < 3; k++)
        {
            out[k][0] = v[k][0];
            out[k][1] = v[k][1];
        }
        for (int i = 0; i < 2; i++)
        {
            out[3][i] = v[a][i];
            out[4][i] = v[a][i] + v[b][i] - v[c][i];
            out[5][i] = v[b][i];
        }
    }
}
