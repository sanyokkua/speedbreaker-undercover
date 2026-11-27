// Tests for gpu::pipeline_cache::BackgroundLoad (runtime/gpu/pipeline_cache_load.h):
// the pipeline cache is made on a thread of its own, and until it is there
// the command processor gets no cache instead of waiting for it. From the
// repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/pipeline_cache_load_test.cpp -o build/pipeline_cache_load_test
//   build/pipeline_cache_load_test
#include <gpu/pipeline_cache_load.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

using namespace gpu::pipeline_cache;
using namespace std::chrono_literals;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

// A make() that returns only once Open() is called (a slow cache load:
// MoltenVK compiling every cached Metal library on a cold Metal cache).
struct Gate
{
    std::mutex m;
    std::condition_variable cv;
    bool open = false;
    void Open() { { std::lock_guard l(m); open = true; } cv.notify_all(); }
    void Wait() { std::unique_lock l(m); cv.wait(l, [&] { return open; }); }
};

template<typename T>
static bool WaitForValue(const BackgroundLoad<T>& load, T want)
{
    for (int i = 0; i < 2000 && load.Get() != want; i++)
        std::this_thread::sleep_for(1ms);
    return load.Get() == want;
}

int main()
{
    {
        // Not started: no cache.
        BackgroundLoad<int*> load;
        CHECK(load.Get() == nullptr, "%p", (void*)load.Get());
    }
    {
        // Start returns at once; the cache is null while it loads, then there.
        static int cache = 7;
        Gate gate;
        BackgroundLoad<int*> load;
        const auto start = std::chrono::steady_clock::now();
        load.Start([&] { gate.Wait(); return &cache; });
        CHECK(std::chrono::steady_clock::now() - start < 500ms, "Start waited for the load");
        std::this_thread::sleep_for(20ms);
        CHECK(load.Get() == nullptr, "a cache before the load finished: %p", (void*)load.Get());
        gate.Open();
        CHECK(WaitForValue(load, &cache), "the loaded cache never showed: %p", (void*)load.Get());
    }
    {
        // The loader outlives the object (a quit during a long load): the
        // result goes nowhere, nothing crashes.
        static int cache = 9;
        auto gate = std::make_shared<Gate>();
        auto done = std::make_shared<std::atomic<bool>>(false);
        {
            BackgroundLoad<int*> load;
            load.Start([gate, done] { gate->Wait(); done->store(true); return &cache; });
        }
        gate->Open();
        for (int i = 0; i < 2000 && !done->load(); i++)
            std::this_thread::sleep_for(1ms);
        std::this_thread::sleep_for(10ms);
        CHECK(done->load(), "the loader never ran");
    }

    if (g_failures)
    {
        printf("pipeline_cache_load_test: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("pipeline_cache_load_test: all passed\n");
    return 0;
}
