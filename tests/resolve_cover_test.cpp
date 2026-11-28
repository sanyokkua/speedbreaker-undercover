// Tests for gpu::resolve_cover (runtime/gpu/resolve_cover.h): the pages and
// textures a 32-bit resolve writes whole, checked against the resolve
// shader's own tiled address of every pixel; and renderer.cpp using them.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/resolve_cover_test.cpp -o build/resolve_cover_test
//   build/resolve_cover_test runtime/gpu/renderer.cpp
#include <gpu/resolve_cover.h>

#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace gpu::resolve_cover;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// The resolve shader's tiled2D (renderer.cpp's kResolveGlsl), 32-bit pixels.
static uint32_t Tiled2D(uint32_t x, uint32_t y, uint32_t pitch, uint32_t bppLog2)
{
    uint32_t outer = (((y >> 5) * (pitch >> 5)) + (x >> 5)) << 6;
    uint32_t inner = (((y >> 1) & 7u) << 3) | (x & 7u);
    uint32_t oi = (outer | inner) << bppLog2;
    uint32_t bank = (y >> 4) & 1u;
    uint32_t pipe = ((x >> 3) & 3u) ^ (((y >> 3) & 1u) << 1);
    return ((y & 1u) << 4) | (pipe << 6) | (bank << 11) | (oi & 0xFu) | (((oi >> 4) & 1u) << 5) |
        (((oi >> 5) & 7u) << 8) | ((oi >> 8) << 12);
}

// Every page the rectangle writes, with how many bytes of it.
static void CheckAgainstShader(uint32_t destBase, uint32_t pitch, Rect r)
{
    std::vector<uint32_t> bytes(1u << 16, 0);
    const uint32_t first = destBase >> 12;
    for (int32_t y = r.y0; y < r.y1; y++)
        for (int32_t x = r.x0; x < r.x1; x++)
        {
            uint32_t page = (destBase + Tiled2D(uint32_t(x), uint32_t(y), pitch, 2)) >> 12;
            if (page - first < bytes.size())
                bytes[page - first] += 4;
        }
    for (uint32_t i = 0; i < 4096; i++)
    {
        const bool whole = bytes[i] == 4096;
        CHECK(PageCovered(destBase, pitch, r, first + i) == whole, "pitch %u rect (%d,%d)-(%d,%d) page +%u: %u bytes written", pitch, r.x0,
            r.y0, r.x1, r.y1, i, bytes[i]);
        if (g_failures > 20)
            return;
    }
}

static void TestPages()
{
    // Deathly Hallows Part 1's three shadow cascades in the 3072x1024 map
    // (each from its own destination base, 32 tiles apart), the scene's
    // 1152x720 copy (its last tile row half covered), a strip.
    CheckAgainstShader(0x09B27000, 3072, { 0, 0, 1024, 1024 });
    CheckAgainstShader(0x09B47000, 3072, { 0, 0, 1024, 1024 });
    CheckAgainstShader(0x08E37000, 1152, { 0, 0, 1152, 720 });
    CheckAgainstShader(0x08E37000, 1152, { 0, 360, 1152, 720 });
    CheckAgainstShader(0x0A6CD000, 96, { 0, 0, 72, 48 });
    CheckAgainstShader(0x09B27000, 640, { 40, 8, 600, 400 });
    // Not page-aligned, odd pitch, below the base: nothing whole.
    CHECK(!PageCovered(0x09B27100, 1152, { 0, 0, 1152, 720 }, 0x09B27), "unaligned base");
    CHECK(!PageCovered(0x09B27000, 1150, { 0, 0, 1152, 720 }, 0x09B27), "pitch not whole tiles");
    CHECK(!PageCovered(0x09B27000, 1152, { 0, 0, 1152, 720 }, 0x09B26), "a page before the base");
}

static void TestTextures()
{
    CHECK(TextureCovered({ 0, 0, 1152, 720 }, 1152, 720), "the scene's copy");
    CHECK(TextureCovered({ 0, 0, 288, 184 }, 288, 180), "a bloom level (its rectangle 8-row aligned)");
    CHECK(!TextureCovered({ 0, 0, 1152, 712 }, 1152, 720), "rows short");
    CHECK(!TextureCovered({ 8, 0, 1152, 720 }, 1152, 720), "a column short");
    CHECK(!TextureCovered({ 0, 0, 1024, 1024 }, 3072, 1024), "one cascade of the map");
    CHECK(!TextureCovered({ 0, 0, 16, 16 }, 0, 0), "an empty texture");
}

static void TestRenderer(const char* path)
{
    std::ifstream in(path);
    if (!in)
    {
        printf("  (renderer.cpp not given: source checks skipped)\n");
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string src = ss.str();
    CHECK(src.find("resolve_cover::PageCovered(") != std::string::npos, "Resolve skips filling whole pages");
    CHECK(src.find("resolve_cover::TextureCovered(") != std::string::npos, "Resolve stores into textures it covers whole");
    CHECK(src.find("NFSMW_RESOLVE_COVER") != std::string::npos, "the switch");
}

int main(int argc, char** argv)
{
    TestPages();
    TestTextures();
    TestRenderer(argc > 1 ? argv[1] : "runtime/gpu/renderer.cpp");
    if (g_failures)
    {
        printf("resolve_cover_test: %d failures\n", g_failures);
        return 1;
    }
    printf("resolve_cover_test: all passed\n");
    return 0;
}
