// Tests for gpu::tile_predicate (runtime/gpu/tile_predicate.h): which
// predicated PM4 packets run under untiled rendering of predicated tiling.
// Half-Blood Prince gives each scene draw a bin mask of the tiles it touches
// (2 tiles at 2x MSAA, the left and right halves of 1280x720: tile 0 selects
// bins 0x80000003, tile 1 0xC; draws 0xA2 left only, 0xA8 right only, 0xAA
// both). The untiled renderer draws everything in tile 0 and skips tile 1's
// draws, so a right-half-only draw was never drawn: the tree, reeds and bank
// at the right of the Burrow went missing (the pond's grey wedge), objects
// blinked out as they crossed the middle. The command processor and the
// renderer are checked in the sources given as arguments. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/tile_predicate_test.cpp -o build/tile_predicate_test
//   build/tile_predicate_test runtime/gpu/command_processor.cpp runtime/gpu/renderer.cpp
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/tile_predicate.h"

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
    using gpu::tile_predicate::Runs;
    constexpr uint64_t kTile0 = 0x80000003, kTile1 = 0xC;
    constexpr uint64_t kLeft = 0xA2, kRight = 0xA8, kBoth = 0xAA;

    // The console: a packet runs in the tiles its mask selects.
    Check(Runs(kLeft, kTile0, true, false), "left draw in tile 0 (per-tile rendering)");
    Check(!Runs(kRight, kTile0, true, false), "right draw not in tile 0 (per-tile rendering)");
    Check(Runs(kRight, kTile1, true, false), "right draw in tile 1 (per-tile rendering)");
    Check(!Runs(kLeft, kTile1, true, false), "left draw not in tile 1 (per-tile rendering)");

    // Untiled: tile 0 draws every tile's scene draws (tile 1 skips them in
    // the renderer), so a right-half-only draw runs in tile 0.
    Check(Runs(kRight, kTile0, true, true), "right draw drawn in tile 0 when untiled");
    Check(Runs(kLeft, kTile0, true, true), "left draw drawn in tile 0 when untiled");
    Check(Runs(kBoth, kTile0, true, true), "draw in both tiles drawn in tile 0 when untiled");
    // A draw in no tile (empty mask) stays skipped.
    Check(!Runs(0, kTile0, true, true), "draw with an empty bin mask skipped");
    // Per-tile packets that are not scene draws (the window offset's
    // SET_CONSTANT, the tile's resolve, its WAIT_REG_MEM) keep the console's rule.
    Check(!Runs(0xC, kTile0, false, true), "tile 1's state or resolve not run in tile 0");
    Check(Runs(0x3, kTile0, false, true), "tile 0's state or resolve run in tile 0");
    Check(!Runs(0x3, kTile1, false, true), "tile 0's resolve not run in tile 1");

    // The command processor decides with Runs; the renderer's answer for
    // "this tile draws every tile" is the tile it doesn't skip (TileShift 0,0).
    if (argc > 2)
    {
        const std::string cp = Read(argv[1]), rd = Read(argv[2]);
        Check(cp.find("tile_predicate::Runs(s_binMask, s_binSelect,") != std::string::npos,
            "command_processor.cpp: predicated packets decided by tile_predicate::Runs");
        Check(cp.find("renderer::DrawsEveryTile()") != std::string::npos,
            "command_processor.cpp: asks the renderer whether this tile draws every tile");
        Check(cp.find("\"NFSMW_TILE_PREDICATE\"") != std::string::npos,
            "command_processor.cpp: NFSMW_TILE_PREDICATE=0 keeps the old rule (A/B)");
        Check(cp.find("\"NFSMW_LOG_PREDICATE\"") != std::string::npos,
            "command_processor.cpp: NFSMW_LOG_PREDICATE logs the predicated packets' masks");
        const size_t def = rd.find("bool DrawsEveryTile()");
        Check(def != std::string::npos, "renderer.cpp: DrawsEveryTile defined");
        if (def != std::string::npos)
        {
            const std::string body = rd.substr(def, 900);
            Check(body.find("TileShift()") != std::string::npos, "renderer.cpp: DrawsEveryTile uses TileShift");
            Check(body.find("s_untiled") != std::string::npos, "renderer.cpp: DrawsEveryTile only when untiled");
        }
    }
    else
        Check(false, "give command_processor.cpp and renderer.cpp as arguments");

    if (s_failures == 0)
        std::printf("tile_predicate_test: all passed\n");
    return s_failures == 0 ? 0 : 1;
}
