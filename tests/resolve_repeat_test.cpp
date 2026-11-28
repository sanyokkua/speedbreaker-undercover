// Tests for gpu::resolve_repeat (runtime/gpu/resolve_repeat.h): which resolves
// repeat one already done; and renderer.cpp's Resolve using the book.
// From the repo root:
//   clang++ -std=c++20 -O2 -Iruntime tests/resolve_repeat_test.cpp -o build/resolve_repeat_test
//   build/resolve_repeat_test runtime/gpu/renderer.cpp
#include <gpu/resolve_repeat.h>

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace gpu::resolve_repeat;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Deathly Hallows Part 1's scene copies (Resolve's values at 1x).
static Key SceneColor()
{
    Key k;
    k.source = 1;
    k.sourceImage = 100;
    k.x1 = 1152;
    k.y1 = 720;
    k.destBase = 0x08E37000;
    k.destPitch = 1152;
    k.format = 6;
    k.copyControl = CopyBits(0x00100000);
    k.surfaceInfo = 1200;
    return k;
}
static Key SceneDepth()
{
    Key k = SceneColor();
    k.source = 2;
    k.sourceImage = 200;
    k.destBase = 0x094AF000;
    k.format = 22;
    k.copyControl = CopyBits(0x00100004);
    return k;
}

// Guest memory as the renderer sees it: a write sequence per destination.
struct Memory
{
    std::map<uint32_t, uint64_t> seq;
    uint64_t next = 1;
    uint64_t Write(uint32_t base) { return seq[base] = next++; }
    auto Since(uint32_t base) { return [this, base](uint64_t s) { return seq[base] > s; }; }
};

static void TestRepeatPair()
{
    // The game's A B A B: both second copies repeat.
    Book book;
    Memory mem;
    uint64_t epoch = 7;
    Key a = SceneDepth(), b = SceneColor();
    CHECK(!book.Repeats(a, epoch, mem.Since(a.destBase)), "nothing recorded yet");
    book.Record(a, epoch, mem.Write(a.destBase));
    CHECK(!book.Repeats(b, epoch, mem.Since(b.destBase)), "another key");
    book.Record(b, epoch, mem.Write(b.destBase));
    CHECK(book.Repeats(a, epoch, mem.Since(a.destBase)), "depth again");
    CHECK(book.Repeats(b, epoch, mem.Since(b.destBase)), "colour again");
}

static void TestBreaks()
{
    Memory mem;
    Key a = SceneColor();
    {
        // A draw or clear into any target between them.
        Book book;
        book.Record(a, 7, mem.Write(a.destBase));
        CHECK(!book.Repeats(a, 8, mem.Since(a.destBase)), "the target epoch moved");
    }
    {
        // The destination written since (the CPU, another resolve).
        Book book;
        book.Record(a, 7, mem.Write(a.destBase));
        mem.Write(a.destBase);
        CHECK(!book.Repeats(a, 7, mem.Since(a.destBase)), "the destination was written");
    }
    {
        // Unknown sequence (per-page sequences): never recorded.
        Book book;
        book.Record(a, 7, 0);
        CHECK(!book.Repeats(a, 7, mem.Since(a.destBase)), "no sequence");
    }
    struct Case { const char* what; void (*change)(Key&); };
    const Case cases[] = {
        { "source", [](Key& k) { k.source++; } },
        { "source image", [](Key& k) { k.sourceImage++; } },
        { "rectangle", [](Key& k) { k.x1 -= 8; } },
        { "origin", [](Key& k) { k.y0 += 8; } },
        { "destination", [](Key& k) { k.destBase += 0x1000; } },
        { "pitch", [](Key& k) { k.destPitch += 32; } },
        { "format", [](Key& k) { k.format = 7; } },
        { "endian / swap / exp bias", [](Key& k) { k.destInfo ^= 1; } },
        { "copy command / sample", [](Key& k) { k.copyControl ^= 0x10; } },
        { "surface (MSAA)", [](Key& k) { k.surfaceInfo ^= 0x10000; } },
        { "scaled samples wanted", [](Key& k) { k.extra = 1; } },
    };
    for (const Case& c : cases)
    {
        Book book;
        book.Record(a, 7, mem.Write(a.destBase));
        Key k = a;
        c.change(k);
        CHECK(!book.Repeats(k, 7, mem.Since(k.destBase)), "%s differs", c.what);
    }
}

static void TestClearBits()
{
    // A copy that also clears copies the same words: its clear runs after
    // (and moves the epoch, ending the match for what follows).
    CHECK(CopyBits(0x00100300) == CopyBits(0x00100000), "clear bits left out");
    CHECK(CopyBits(0x00100004) != CopyBits(0x00100000), "source select kept");
    CHECK(CopyBits(0x00200000) != CopyBits(0x00100000), "copy command kept");
}

static void TestBookSize()
{
    // Older entries fall out of the ring; a key's newer copy replaces its older one.
    Book book;
    Memory mem;
    Key k[Book::kEntries + 1];
    for (size_t i = 0; i <= Book::kEntries; i++)
    {
        k[i] = SceneColor();
        k[i].destBase += uint32_t(i) << 20;
        book.Record(k[i], 7, mem.Write(k[i].destBase));
    }
    CHECK(!book.Repeats(k[0], 7, mem.Since(k[0].destBase)), "the oldest fell out");
    for (size_t i = 1; i <= Book::kEntries; i++)
        CHECK(book.Repeats(k[i], 7, mem.Since(k[i].destBase)), "entry %zu kept", i);
    Book again;
    Key a = SceneColor();
    again.Record(a, 7, mem.Write(a.destBase));
    uint64_t newer = mem.Write(a.destBase);
    again.Record(a, 7, newer);
    CHECK(again.Repeats(a, 7, mem.Since(a.destBase)), "the newer copy counts");
    again.Record(SceneDepth(), 8, mem.Write(SceneDepth().destBase));
    CHECK(!again.Repeats(a, 8, mem.Since(a.destBase)), "a new epoch drops the old entries");
    again.Clear();
    CHECK(!again.Repeats(SceneDepth(), 8, mem.Since(SceneDepth().destBase)), "cleared");
}

// renderer.cpp: Resolve consults the book before copying, the target epoch
// moves on every target write, and the switch exists.
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
    auto count = [&](const std::string& s) {
        size_t n = 0;
        for (size_t at = src.find(s); at != std::string::npos; at = src.find(s, at + 1))
            n++;
        return n;
    };
    CHECK(count("s_resolveRepeats.Repeats(") == 1, "Resolve checks the book");
    CHECK(count("s_resolveRepeats.Record(") == 1, "Resolve records its copies");
    CHECK(count("NFSMW_RESOLVE_REPEAT") >= 1, "the switch");
    // Every place that writes a target's image moves the epoch: passes,
    // clears outside passes, the blits of PropagateDrawClear, copies into a
    // remade target.
    CHECK(count("s_vk->cmdBeginRendering(") == 1, "one place begins passes");
    CHECK(count("s_targetEpoch++") >= 5, "the epoch moves at every target write (%zu)", count("s_targetEpoch++"));
}

int main(int argc, char** argv)
{
    TestRepeatPair();
    TestBreaks();
    TestClearBits();
    TestBookSize();
    TestRenderer(argc > 1 ? argv[1] : "runtime/gpu/renderer.cpp");
    if (g_failures)
    {
        printf("resolve_repeat_test: %d failures\n", g_failures);
        return 1;
    }
    printf("resolve_repeat_test: all passed\n");
    return 0;
}
