// Tests for gpu::draw_clear (runtime/gpu/draw_clear.h): which draws are
// clears, which EDRAM layouts share rows, and how a rectangle maps between
// MSAA layouts; with Order of the Phoenix's scene clear as the game draws it
// (4x MSAA, pitch 640, over the 1x pitch-1280 scene). renderer.cpp is read
// to check Draw applies it.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/draw_clear_test.cpp -o build/draw_clear_test
//   build/draw_clear_test runtime/gpu/renderer.cpp
#include <gpu/draw_clear.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

using namespace gpu::draw_clear;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

// OotP's per-frame scene clear (draw log: init 00030088 rectangle list, surf
// 0a020280, cctl 0, bl0 00010001, dctl 00008777, cmask ffff).
static Draw OotpClear()
{
    Draw d;
    d.rectangleList = true;
    d.colorWriteMask = 0x000F;  // (cmask ffff, only target 0 attached)
    d.blendControl[0] = 0x00010001;
    d.depthAttached = true;
    d.depthControl = 0x00008777;
    d.stencilWriteMask = 0xFF;
    return d;
}

int main(int argc, char** argv)
{
    {
        Cleared c = Classify(OotpClear());
        CHECK(c.colorTargets == 1);
        CHECK(c.depth);
        CHECK(c.stencil);
    }
    {
        // The scene's own draws are not clears: GEQUAL depth, a texture, blending.
        Draw d = OotpClear();
        d.rectangleList = false;
        CHECK(!Classify(d).Any());
        d = OotpClear();
        d.depthControl = 0x00700766;  // GEQUAL
        CHECK(!Classify(d).depth);
        CHECK(Classify(d).colorTargets == 1);
        d = OotpClear();
        d.pixelShaderSamples = true;
        CHECK(!Classify(d).Any());
        d = OotpClear();
        d.blendControl[0] = 0x07060706;  // alpha blending
        CHECK(Classify(d).colorTargets == 0);
        d = OotpClear();
        d.colorWriteMask = 0x7;  // RGB only: alpha kept
        CHECK(Classify(d).colorTargets == 0);
        d = OotpClear();
        d.alphaTest = true;
        CHECK(!Classify(d).Any());
        d = OotpClear();
        d.pixelShaderKills = true;
        CHECK(!Classify(d).Any());
        // Depth cleared, stencil kept (disabled or KEEP): depth only.
        d = OotpClear();
        d.depthControl = 0x00008776;
        CHECK(Classify(d).depth && !Classify(d).stencil);
        d = OotpClear();
        d.depthControl = 0x00000777;  // stencil zpass KEEP
        CHECK(Classify(d).depth && !Classify(d).stencil);
        d = OotpClear();
        d.stencilWriteMask = 0x0F;
        CHECK(Classify(d).depth && !Classify(d).stencil);
        // No depth write: not a depth clear.
        d = OotpClear();
        d.depthControl = 0x00008773;
        CHECK(!Classify(d).depth && !Classify(d).stencil);
    }
    {
        // 4x at pitch 640 and 1x at pitch 1280: 16 tiles a row both.
        CHECK(TilesPerRow(640, 2, false) == 16);
        CHECK(TilesPerRow(1280, 0, false) == 16);
        CHECK(SameRows(640, 2, 1280, 0, false));
        // 2x at pitch 1280 is 16 tiles too (rows doubled only).
        CHECK(SameRows(1280, 1, 1280, 0, false));
        // The bloom's 320 pitch is not the scene's.
        CHECK(!SameRows(320, 0, 1280, 0, false));
        CHECK(!SameRows(640, 0, 1280, 0, false));
        // 64bpp doubles the tiles of both alike.
        CHECK(TilesPerRow(640, 2, true) == 32);
    }
    {
        // The 640x368 4x clear is the 1280x736 1x scene, and back.
        Rect r = MapRect({ 0, 0, 640, 368 }, 2, 0);
        CHECK(r.x0 == 0 && r.y0 == 0 && r.x1 == 1280 && r.y1 == 736);
        r = MapRect({ 0, 0, 1280, 736 }, 0, 2);
        CHECK(r.x0 == 0 && r.y0 == 0 && r.x1 == 640 && r.y1 == 368);
        // 2x -> 1x: rows only.
        r = MapRect({ 10, 10, 20, 20 }, 1, 0);
        CHECK(r.x0 == 10 && r.y0 == 20 && r.x1 == 20 && r.y1 == 40);
        // 1x -> 4x rounds in: a 4x pixel half covered is not cleared.
        r = MapRect({ 1, 1, 5, 5 }, 0, 2);
        CHECK(r.x0 == 1 && r.y0 == 1 && r.x1 == 2 && r.y1 == 2);
        r = MapRect({ 1, 1, 2, 2 }, 0, 2);
        CHECK(r.x1 >= r.x0 && r.y1 >= r.y0 && (r.x1 - r.x0) == 0);
    }
    {
        // Half-Blood Prince: a 320x736 4x clear (pitch 320, depth at base 736)
        // into the untiled 2x scene (pitch 640) reached the renderer as the
        // whole 1280x2048 target; only the pitch's columns and the rows left
        // in EDRAM are the clear's ((2048 - 736) / 8 tile rows of 16 sample
        // rows, 2 a 4x pixel).
        Rect r = EdramBounds({ 0, 0, 1280, 2048 }, 736, 320, 2, false);
        CHECK(r.x0 == 0 && r.y0 == 0 && r.x1 == 320 && r.y1 == 1312);
        r = MapRect(r, 2, 1);
        CHECK(r.x1 == 640 && r.y1 == 1312);
        // Already inside: unchanged; an untiled 1x 1280 clear keeps its width.
        r = EdramBounds({ 0, 0, 320, 736 }, 736, 320, 2, false);
        CHECK(r.x1 == 320 && r.y1 == 736);
        r = EdramBounds({ 0, 0, 1280, 720 }, 0, 1280, 0, false);
        CHECK(r.x1 == 1280 && r.y1 == 720);
        // A base past EDRAM covers nothing.
        r = EdramBounds({ 0, 0, 1280, 720 }, 2048, 1280, 0, false);
        CHECK(r.x1 == r.x0 || r.y1 == r.y0);
    }
    if (argc > 1)
    {
        std::ifstream f(argv[1]);
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string src = ss.str();
        CHECK(src.find("draw_clear::Classify(") != std::string::npos);
        CHECK(src.find("PropagateDrawClear(") != std::string::npos);
        CHECK(src.find("r = dc::EdramBounds(r, src->base, src->pitch, msaa, is64);") != std::string::npos);
        CHECK(src.find("HostRect(src->scale, sr.x0, sr.y0, sr.x1, sr.y1)") != std::string::npos);
    }
    if (g_failures)
    {
        printf("draw_clear_test: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("draw_clear_test: all passed\n");
    return 0;
}
