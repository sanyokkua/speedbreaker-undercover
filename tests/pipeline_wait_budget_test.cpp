// Tests for gpu::pipeline_wait::Budget (runtime/gpu/pipeline_wait_budget.h):
// how long the command processor may wait for compiling pipelines a frame,
// normally and after a loading screen. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/pipeline_wait_budget_test.cpp -o build/pipeline_wait_budget_test
//   build/pipeline_wait_budget_test
#include <gpu/pipeline_wait_budget.h>

#include <cstdio>

using namespace gpu::pipeline_wait;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

constexpr int64_t kFrame = 3000, kLoad = 50000;
constexpr uint64_t kBusy = 500;  // draws in a gameplay frame

// Runs frames from `first` to `last` (inclusive) of `draws` each, every one
// spending all its budget; returns the total waited.
static int64_t SpendAll(Budget& b, uint64_t first, uint64_t last, uint64_t draws)
{
    int64_t total = 0;
    for (uint64_t f = first; f <= last; f++)
    {
        b.OnSwap(f, draws);
        total += b.LeftUs();
        b.Spend(b.LeftUs());
    }
    return total;
}

int main()
{
    {
        // Boot: the first frame has the loading budget.
        Budget b(kFrame, kLoad);
        CHECK(b.LeftUs() == kLoad, "boot budget %lld", (long long)b.LeftUs());
    }
    {
        // A level's first frames on a cold cache: two long frames at most,
        // then the normal budget, though still in the window.
        Budget b(kFrame, kLoad);
        SpendAll(b, 1, 300, kBusy);  // past the boot window
        b.OnSwap(301, 10);           // a loading screen ended this frame
        int64_t first = b.LeftUs();
        b.Spend(first);
        b.OnSwap(302, kBusy);
        int64_t second = b.LeftUs();
        b.Spend(second);
        b.OnSwap(303, kBusy);
        int64_t third = b.LeftUs();
        CHECK(first == kLoad && second == kLoad, "first two frames %lld, %lld", (long long)first, (long long)second);
        CHECK(third == kFrame, "third frame %lld (the window's budget is spent)", (long long)third);
        int64_t rest = SpendAll(b, 304, 500, kBusy);
        CHECK(rest == (500 - 304 + 1) * kFrame, "rest of the window %lld", (long long)rest);
    }
    {
        // Cache hits: frames that wait little leave the window's budget for later frames.
        Budget b(kFrame, kLoad);
        SpendAll(b, 1, 300, kBusy);
        b.OnSwap(301, 10);
        int64_t waited = 0;
        for (uint64_t f = 302; f < 301 + kLoadFrames; f++)
        {
            b.Spend(200);
            waited += 200;
            b.OnSwap(f, kBusy);
        }
        CHECK(b.LeftUs() == kLoad, "after %lld us of hits: %lld", (long long)waited, (long long)b.LeftUs());
        b.OnSwap(301 + kLoadFrames, kBusy);
        CHECK(b.LeftUs() == kFrame, "the window ends: %lld", (long long)b.LeftUs());
    }
    {
        // A loading screen of many low-draw frames: each may wait (nobody
        // plays then), and the window restarts with its full budget.
        Budget b(kFrame, kLoad);
        SpendAll(b, 1, 300, kBusy);
        SpendAll(b, 301, 400, 10);
        b.OnSwap(401, kBusy);
        CHECK(b.LeftUs() == kLoad, "after the loading screen %lld", (long long)b.LeftUs());
    }
    {
        // An overshoot (a wait that ended late) never gives a negative budget.
        Budget b(kFrame, kLoad);
        SpendAll(b, 1, 300, kBusy);
        b.Spend(kFrame + 5000);
        CHECK(b.LeftUs() == 0, "overshot %lld", (long long)b.LeftUs());
        b.OnSwap(301, kBusy);
        CHECK(b.LeftUs() == kFrame, "next frame %lld", (long long)b.LeftUs());
    }
    if (g_failures)
    {
        printf("pipeline_wait_budget_test: %d failures\n", g_failures);
        return 1;
    }
    printf("pipeline_wait_budget_test: all passed\n");
    return 0;
}
