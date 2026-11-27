// Tests for gpu::edge_snap (runtime/gpu/edge_snap.h): a full-screen quad drawn
// from Direct3D 9 x/y 0 covers every host pixel of a scaled target, as the
// console's 1x rasteriser covers every guest pixel; a quad from -0.5, 3D
// vertices and 1x are left alone. The shader's copy of the rule
// (shader_translator.cpp) and the renderer's push constant are checked in
// the sources given as arguments. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/edge_snap_test.cpp -o build/edge_snap_test
//   build/edge_snap_test runtime/gpu/shader_translator.cpp runtime/gpu/renderer.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/edge_snap.h"

namespace
{
    int s_failures = 0;
    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            std::printf("FAIL: %s\n", what);
            s_failures++;
        }
    }

    // Host pixels of a target `size` guest pixels across at `scale` that a
    // quad from guest D3D coordinate e0 to e1 (w `w`) covers, as the
    // renderer draws it: +0.5 (D3D centres), the viewport to NDC, the
    // shader's snap, then the host's top-left rule (a centre on the left
    // edge is in, on the right edge out). Returns the first and one past
    // the last covered host pixel.
    struct Span { int first, end; };
    Span Covered(float e0, float e1, float w, float scale, uint32_t size, bool snap)
    {
        const float half = snap ? gpu::edge_snap::HalfPixelNdc(true, scale, size) : 0.0f;
        auto toHost = [&](float e) {
            float ndc = 2.0f * (e + 0.5f) / float(size) - 1.0f;
            ndc = gpu::edge_snap::SnapNdc(ndc, w, half);
            return (ndc + 1.0f) * 0.5f * float(size) * scale;
        };
        const float l = toHost(e0), r = toHost(e1);
        const int hostSize = int(std::lround(float(size) * scale));
        int first = -1, end = -1;
        for (int j = 0; j < hostSize; j++)
        {
            const float c = float(j) + 0.5f;
            if (c >= l && c < r)
            {
                if (first < 0)
                    first = j;
                end = j + 1;
            }
        }
        return { first, end };
    }

    std::string Read(const char* path)
    {
        std::ifstream f(path);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }
}

int main(int argc, char** argv)
{
    using gpu::edge_snap::HalfPixelNdc;
    // Off at 1x, with OpenGL centres, or for an empty target.
    Check(HalfPixelNdc(true, 1.0f, 1280) == 0.0f, "off at 1x");
    Check(HalfPixelNdc(false, 3.0f, 1280) == 0.0f, "off with OpenGL centres");
    Check(HalfPixelNdc(true, 3.0f, 0) == 0.0f, "off for size 0");
    Check(HalfPixelNdc(true, 3.346f, 736) == 1.0f / 736.0f, "half a pixel in NDC");

    for (float scale : { 1.5f, 2.0f, 2.231f, 3.0f, 3.346f, 4.0f })
        for (uint32_t size : { 720u, 736u, 1280u })
        {
            const int hostSize = int(std::lround(float(size) * scale));
            // The bug: a fade from D3D 0 leaves the first host pixel(s) undrawn.
            const Span before = Covered(0.0f, float(size), 1.0f, scale, size, false);
            Check(before.first > 0, "a quad from 0 misses the first host pixel without the snap");
            // The fix: every host pixel.
            const Span after = Covered(0.0f, float(size), 1.0f, scale, size, true);
            Check(after.first == 0 && after.end == hostSize, "a quad from 0 covers the whole target with the snap");
            // A quad from -0.5 (the D3D 9 correction) is already whole and unchanged.
            const Span corrected = Covered(-0.5f, float(size) - 0.5f, 1.0f, scale, size, true);
            const Span correctedOff = Covered(-0.5f, float(size) - 0.5f, 1.0f, scale, size, false);
            Check(corrected.first == 0 && corrected.end == hostSize, "a quad from -0.5 covers the whole target");
            Check(corrected.first == correctedOff.first && corrected.end == correctedOff.end, "a quad from -0.5 is not moved");
            // 3D vertices (w != 1) never move; nor does an edge inside the target.
            Check(gpu::edge_snap::SnapNdc(-1.0f + 1.0f / float(size), 2.0f, HalfPixelNdc(true, scale, size)) == -1.0f + 1.0f / float(size),
                "a vertex with w 2 stays");
            const Span inner = Covered(10.0f, 20.0f, 1.0f, scale, size, true);
            const Span innerOff = Covered(10.0f, 20.0f, 1.0f, scale, size, false);
            Check(inner.first == innerOff.first && inner.end == innerOff.end, "a quad inside the target is not moved");
        }
    // At 1x the console's rule already covers column 0 (the snap is off there).
    {
        const Span s = Covered(0.0f, 1280.0f, 1.0f, 1.0f, 1280, false);
        Check(s.first == 0 && s.end == 1280, "1x covers the whole target from 0");
    }

    // The shader applies the same rule, and the renderer sets it per draw.
    if (argc >= 3)
    {
        const std::string shader = Read(argv[1]), renderer = Read(argv[2]);
        // Two floats right after constRing (offset 116, as the C++ struct): a
        // vec2 there would be aligned to 120 and read the wrong values.
        Check(shader.find("uint constRing;       // ring byte offset of this draw's XenosConstants, see constRel()\n    float edgeSnapX;") != std::string::npos,
            "shader declares edgeSnapX right after constRing");
        Check(shader.find("float edgeSnapY;") != std::string::npos, "shader declares edgeSnapY");
        Check(shader.find("vec2 edgeSnap") == std::string::npos, "no vec2 (8-byte alignment) in the push constants");
        Check(shader.find("abs(pos.x + 1.0 - pc.edgeSnapX) < pc.edgeSnapX * 0.125") != std::string::npos, "shader snaps x");
        Check(shader.find("abs(pos.y + 1.0 - pc.edgeSnapY) < pc.edgeSnapY * 0.125") != std::string::npos, "shader snaps y");
        Check(renderer.find("float edgeSnap[2];") != std::string::npos && renderer.find("static_assert(sizeof(DrawConstants) == 124);") != std::string::npos,
            "the C++ push constants end with the two floats (124 bytes)");
        Check(renderer.find("pc.edgeSnap[0] = edgeSnapOn ? gpu::edge_snap::HalfPixelNdc(d3dCentres, scaleX, uint32_t(w))") != std::string::npos,
            "renderer sets x");
        Check(renderer.find("pc.edgeSnap[1] = edgeSnapOn ? gpu::edge_snap::HalfPixelNdc(d3dCentres, scaleY, uint32_t(h))") != std::string::npos,
            "renderer sets y");
    }
    else
        Check(false, "sources not given");

    std::printf("%s\n", s_failures ? "edge_snap_test: FAILED" : "edge_snap_test: OK");
    return s_failures ? 1 : 0;
}
