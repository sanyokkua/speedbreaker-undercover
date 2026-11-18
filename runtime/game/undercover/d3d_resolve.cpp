// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// NFS: Undercover's D3DDevice_Resolve (sub_82935280: r3 the device, r4 the
// flags) with no surface to resolve from. The render thread and the render
// jobs (sub_82751068 on processors 1, 3, 5) both run sub_8275A5B0, which
// sets the depth-stencil surface (sub_82913288, device +0x30A8) and then
// resolves it. On the console the game's own frame handshake keeps them
// apart; on a Mac every guest thread is a host thread, and about one boot in
// 40 one of them resolved while the other had the surface unset: a null
// surface read at 0x829352E4, a crash in the first seconds.
//
// When the resolve's source (flags & 7: a colour target at +0x3098 + 4n, or
// 4, the depth-stencil at +0x30A8; only when flags & 0x70 is 0, as the
// function reads it) is null, the resolve is skipped: the console never
// reaches that state, and a skipped resolve costs one frame of that copy
// instead of the game.
#include <stdafx.h>
#include <cstdio>

PPC_FUNC_IMPL(__imp__sub_82935280);
PPC_FUNC(sub_82935280)
{
    const uint32_t device = ctx.r3.u32, flags = ctx.r4.u32;
    if ((flags & 0x70) == 0)
    {
        const uint32_t index = flags & 7;
        const uint32_t field = index == 4 ? 0x30A8 : 0x3098 + index * 4;
        if (PPC_LOAD_U32(device + field) == 0)
        {
            static uint32_t skipped = 0;
            if (skipped++ < 8)
                fprintf(stderr, "[d3d] resolve skipped: no surface (flags %X, device %08X, slot %u)\n", flags, device, index);
            return;
        }
    }
    __imp__sub_82935280(ctx, base);
}
