// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See game/ultrawide.h.
//
// NFS: Undercover: Hor+/Vert+ isn't hooked yet (ProStreet's projection and
// front-end hooks, game/prostreet/ultrawide.cpp, are still to be found in
// Undercover's code). HooksActive() is false, so a screen of another shape
// than 16:9 gets the 16:9 picture letterboxed or pillarboxed, undistorted.
#include <stdafx.h>
#include <game/ultrawide.h>

#include <atomic>

namespace
{
    std::atomic<float> s_scaleX{ 1.0f }, s_scaleY{ 1.0f };
}

namespace game
{
    void SetAspectScale(float scaleX, float scaleY)
    {
        s_scaleX.store(scaleX, std::memory_order_relaxed);
        s_scaleY.store(scaleY, std::memory_order_relaxed);
    }
    float AspectScaleX() { return s_scaleX.load(std::memory_order_relaxed); }
    float AspectScaleY() { return s_scaleY.load(std::memory_order_relaxed); }
    bool HooksActive() { return false; }
    bool TallHooksActive() { return false; }
    float FrontEndScaleX() { return 0.0f; }
    float FrontEndScaleY() { return 0.0f; }
    bool FrontEndParksByCentre() { return false; }
}
