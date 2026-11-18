// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// NFS: Undercover's GetExitCodeThread (XAPI, linked into the game at
// sub_82D5BCA8): ProStreet's sub_82A6C9B8 instruction for instruction (24
// instructions with immediates masked, one hit; the same 0x74 bytes). See
// game/prostreet/xapi_threads.cpp for why the runtime answers it.
#define SB_XAPI_GET_EXIT_CODE_THREAD sub_82D5BCA8
#include "../prostreet/xapi_threads.cpp"
