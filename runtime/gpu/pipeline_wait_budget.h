// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// How long the command processor may wait for pipelines still compiling
// (gpu/renderer.cpp, GetPipeline), a frame at a time. Vulkan-free:
// tests/pipeline_wait_budget_test.cpp checks it alone.
//
// A normal frame waits `frameUs` in all (NFSMW_PIPELINE_WAIT_US, 3 ms: cache
// hits make it), then skips the draws whose pipeline isn't ready. For 120
// frames after a loading screen (a frame of under 64 draws) and at boot, a
// frame may wait `loadFrameUs` (NFSMW_PIPELINE_LOAD_WAIT_US, 50 ms), and
// those 120 frames twice that in all: scene setup, including anything the
// game renders once, is drawn exactly when its pipelines come from the
// caches, and a cold cache costs at most two long frames.
//
// It used to wait in full in that window. Each draw then waited for its
// pipeline before the next draw queued its own, so the workers compiled one
// at a time: on MoltenVK with a cold cache (100-600 ms a pipeline) the first
// frame of DH1's road chase took 9.3 s (69 pipelines), and the next 2 s had
// more of 0.3-4.5 s. A 50 ms budget in every frame of the window instead
// made 20 frames in a row of 67 ms (15 fps) at the sky chase's start.
#pragma once
#include <algorithm>
#include <cstdint>

namespace gpu::pipeline_wait
{
    constexpr uint64_t kLoadFrames = 120;   // the window after a loading screen
    constexpr uint64_t kLoadingDraws = 64;  // a frame of fewer draws is a loading screen (or movie)

    class Budget
    {
    public:
        Budget(int64_t frameUs, int64_t loadFrameUs)
            : m_frameUs(frameUs), m_loadFrameUs(loadFrameUs), m_loadLeftUs(2 * loadFrameUs)
        {
            Start(0);
        }

        // At a swap: `frame` starts; the frame that ended drew `draws`.
        void OnSwap(uint64_t frame, uint64_t draws)
        {
            m_loadLeftUs -= std::max<int64_t>(m_startUs - m_leftUs, 0);
            if (draws < kLoadingDraws)
            {
                m_loadUntil = frame + kLoadFrames;
                m_loadLeftUs = 2 * m_loadFrameUs;
            }
            Start(frame);
        }

        int64_t LeftUs() const { return std::max<int64_t>(m_leftUs, 0); }
        void Spend(int64_t us) { m_leftUs -= us; }
        bool InLoadWindow(uint64_t frame) const { return frame < m_loadUntil; }

    private:
        void Start(uint64_t frame)
        {
            m_startUs = m_frameUs;
            if (InLoadWindow(frame))
                m_startUs = std::max(m_frameUs, std::min(m_loadFrameUs, m_loadLeftUs));
            m_leftUs = m_startUs;
        }

        int64_t m_frameUs, m_loadFrameUs;
        uint64_t m_loadUntil = kLoadFrames;  // boot counts as a loading screen
        int64_t m_loadLeftUs;
        int64_t m_startUs = 0, m_leftUs = 0;
    };
}
