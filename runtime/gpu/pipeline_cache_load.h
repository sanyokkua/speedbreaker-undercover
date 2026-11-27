// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The saved pipeline cache (pipelines.bin) is made into a VkPipelineCache on
// a thread of its own (gpu/renderer.cpp, CreatePipelineCache). Vulkan-free:
// tests/pipeline_cache_load_test.cpp checks it alone.
//
// MoltenVK's vkCreatePipelineCache compiles every Metal library in the data
// before it returns, one at a time (MVKPipelineCache::readData). With macOS's
// Metal cache warm that is ~1 s for DH1's 4.9 MB; cold (evicted: it is one
// cache for every unbundled program, see docs/PORTING.md) it took 12-22 s.
// It ran on the command processor's thread in renderer::Initialize, so the
// game's first GPU work waited: 0 frames and a hang report at 20 s, then the
// game went on. Until the cache is there, pipelines are made without one
// (VK_NULL_HANDLE: compiled from SPIR-V, as on a first run) and nothing is
// saved, so the file on disk stays as it was.
#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include <utility>

namespace gpu::pipeline_cache
{
    template<typename Handle>
    class BackgroundLoad
    {
    public:
        // Runs make() on a new thread; Get() returns its result once it has
        // returned. The thread holds the state, so it may outlive this object
        // (a quit during a long load: the process exits with it running).
        template<typename Make>
        void Start(Make make)
        {
            std::thread([state = m_state, make = std::move(make)]() mutable {
                state->store(make(), std::memory_order_release);
            }).detach();
        }

        // Handle{} until the load has finished.
        Handle Get() const { return m_state->load(std::memory_order_acquire); }

    private:
        std::shared_ptr<std::atomic<Handle>> m_state = std::make_shared<std::atomic<Handle>>(Handle{});
    };
}
