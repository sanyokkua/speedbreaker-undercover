// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Pixel parameter generation (ps_param_gen, Direct3D 9's vPos / VFACE /
// SPRITETEXCOORD). With SQ_PROGRAM_CNTL::param_gen set, the console writes
// the pixel's parameters into pixel shader register
// SQ_CONTEXT_MISC::param_gen_pos, over the interpolator there (the rules are
// in xenos/registers.h at param_gen_pos, as Xenia implements them):
//   |x|, |y|  the pixel's position in the render target, whole numbers
//             (0, 1, 2: the XNA assembler maps vPos straight to it);
//   sign x    negative for a back face;
//   sign y    negative for a point (never here: points are not expanded);
//   z, w      a point sprite's coordinates, 0 for other primitives.
// The runtime left the register as the interpolator (0 when the vertex
// shader writes none there), as if every pixel sat in the target's corner.
// Deathly Hallows - Part 1's lit passes at the Burrow (register 11, after
// the vertex shader's 11 interpolators) read it: spot lights lit the tent
// walls evenly, and light spots and shadow edges slid with the camera.
//
// Scaled rendering: the host pixel's own whole number over the render scale
// ((gl_FragCoord - 0.5) / scale), so 1x gives the console's whole numbers
// exactly, and at 2x guest pixel 3's host pixels read 3 and 3.5: each its
// own place (a screen-space texture resolved at 2x is read per host pixel),
// never negative (the sign is the face).
// Header only, Vulkan-free; tests/param_gen_test.cpp checks it.
#pragma once

#include <cstdint>

namespace gpu::param_gen
{
    // No parameter register (param gen off, or a register past the 16
    // interpolators: Xenia drops it then).
    constexpr uint32_t kOff = 0xFFu;

    // The pixel shader register that receives the parameters, or kOff.
    // programCntl is SQ_PROGRAM_CNTL (param_gen is bit 18), contextMisc
    // SQ_CONTEXT_MISC (param_gen_pos is bits 8-15).
    inline uint32_t Register(uint32_t programCntl, uint32_t contextMisc)
    {
        if (!((programCntl >> 18) & 1u))
            return kOff;
        const uint32_t pos = (contextMisc >> 8) & 0xFFu;
        return pos < 16 ? pos : kOff;
    }

    // The value written, as the shader computes it (paramGen() in the
    // translator's GLSL): fragX/fragY are the host fragment centre
    // (gl_FragCoord, .5 centres), guestPerHostX/Y the inverse render scale.
    struct Value
    {
        float x, y, z, w;
    };
    inline Value Compute(float fragX, float fragY, float guestPerHostX, float guestPerHostY, bool frontFacing)
    {
        Value v{ (fragX - 0.5f) * guestPerHostX, (fragY - 0.5f) * guestPerHostY, 0.0f, 0.0f };
        // A sign bit, not a negation: x is 0 in column 0 and -0 must still
        // read as a back face (shaders test it with rcpc / cndge).
        if (!frontFacing)
            v.x = -v.x;
        return v;
    }
}
