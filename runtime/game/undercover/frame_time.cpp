// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See game/frame_time.h.
//
// NFS: Undercover's game time in whole vblanks: Carbon's hook
// (game/carbon/frame_time.cpp, included below) on Undercover's code, which
// times itself as ProStreet does (game/prostreet/frame_time.cpp) with the same
// constants. Its main loop (sub_82617778, the only user of the 32000 ms cap)
// hands the frame's time, milliseconds in 16.16 fixed point, to the frame
// (sub_82617660) at 0x826178DC (midasm GameFrameTime, r3). The frame's
// quantiser is sub_8262F438 (the only user of 3600.0 and 1.1111112; its call
// from the frame returns to 0x826176A4), keeping the frame's 1/3600 s ticks at
// 0x831EB71C and its seconds at 0x832652F8. The world's accumulator is
// sub_8279DFF0 (the only user of 59.999996), its step count at 0x8279E074
// (WorldStepCount: r11 the steps, f0 the seconds, as ProStreet's) and its
// below-one-step return at 0x8279E044 (WorldNoStep).
//
// Where Undercover differs (none of it changes the hooks): the frame no
// longer calls the world itself; it posts EMainSimulation, whose handler
// (sub_825CE7C8) reads the frame's seconds and, through two more queued
// messages drained by the frame (sub_823CD720), calls the accumulator
// (sub_825DBC78). Online (the byte at 0x831EB87C) dt comes from elsewhere,
// as in ProStreet; SpeedBreaker plays offline.
#define SB_FT_QUANTISER sub_8262F438
#define SB_FT_WORLD sub_8279DFF0
#define SB_FT_QUANTISER_CALL 0x826176A4
#define SB_FT_FRAME_TICKS 0x831EB71C
#define SB_FT_FRAME_SECONDS 0x832652F8
#include "../carbon/frame_time.cpp"
