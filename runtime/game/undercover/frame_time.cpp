// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See game/frame_time.h.
//
// NFS: Undercover: game time in whole vblanks isn't hooked yet (ProStreet's
// hooks, game/prostreet/frame_time.cpp, are still to be found in Undercover's
// code): the game times its frames itself, and the [perf] line counts none.
#include <stdafx.h>
#include <game/frame_time.h>

namespace game
{
    FrameTimeStats GetFrameTimeStats() { return {}; }
    double FrameClockMs() { return 0.0; }
}
