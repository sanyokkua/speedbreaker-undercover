// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// NFS: Undercover's Direct3D GPU waits: Carbon's hooks (game/carbon/d3d_waits.cpp,
// included below) on Undercover's D3D. sub_8291BAC8 is the GPU hang check,
// ProStreet's sub_8286B308 field for field (the only function with
// `cmplwi cr6,r11,5000`; 10 call sites, as ProStreet's): args[0] the device,
// args[8] the counter as last seen, args[12] its time; the counter pointer at
// device +10896 and the last value issued at +10908, unchanged from ProStreet.
// sub_8291AB18 is the wait for a fence (r3 the device, r4 the fence value;
// ProStreet's sub_82853E70, with one more flag argument in r6 the hook
// doesn't read).
#define SB_D3D_HANG_CHECK sub_8291BAC8
#define SB_D3D_FENCE_WAIT sub_8291AB18
#define SB_D3D_COUNTER_PTR 10896
#define SB_D3D_ISSUED 10908
#include "../carbon/d3d_waits.cpp"
