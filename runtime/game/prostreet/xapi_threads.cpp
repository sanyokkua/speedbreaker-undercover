// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// NFS: ProStreet's GetExitCodeThread (XAPI, linked into the game at
// sub_82A6C9B8): ObReferenceObjectByHandle(handle, ExThreadObjectType), then
// it reads the KTHREAD itself: its dispatcher header's SignalState (+4, the
// low byte) and, once signalled, the exit status at +0x140; STILL_ACTIVE
// (259) before. The runtime's thread handles are host objects (a handle is
// the guest address of a GuestThreadHandle, cpu/guest_thread.h), so those
// bytes are a host vtable pointer's: the thread looked alive forever, and
// the game waited after the intro for its loader thread, polling this with
// SwitchToThread, with a black screen. Answered here from the runtime's own
// thread state instead. Most Wanted and Carbon never call it.
//
// GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode): TRUE, and the exit
// code (259 while running); FALSE for a handle that isn't a thread's.
#include <stdafx.h>
#include <cpu/guest_thread.h>
#include <kernel/dispatcher.h>
#include <kernel/xdm.h>

#ifndef SB_XAPI_GET_EXIT_CODE_THREAD
#define SB_XAPI_GET_EXIT_CODE_THREAD sub_82A6C9B8
#endif

PPC_FUNC(SB_XAPI_GET_EXIT_CODE_THREAD)
{
    constexpr uint32_t kStillActive = 259;
    constexpr uint32_t kErrorInvalidHandle = 6;
    const uint32_t handle = ctx.r3.u32;
    const uint32_t out = ctx.r4.u32;

    uint32_t code = kStillActive;
    if (handle != CURRENT_THREAD_HANDLE)
    {
        auto* thread = IsKernelObject(handle) ? dynamic_cast<GuestThreadHandle*>(GetKernelObject(handle)) : nullptr;
        if (!thread)
        {
            GuestThread::SetLastError(kErrorInvalidHandle);
            ctx.r3.u64 = 0;
            return;
        }
        std::lock_guard lock(dispatcher::Lock());
        if (thread->finished)
            code = thread->exitCode;
    }
    PPC_STORE_U32(out, code);
    ctx.r3.u64 = 1;
}
