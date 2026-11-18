// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING). See game/ultrawide.h.
//
// NFS: Undercover's Hor+ and Vert+: ProStreet's (game/prostreet/ultrawide.cpp)
// on Undercover's code. Two mid-asm hooks (config/undercover/undercover.toml):
//   - UltrawideProjection, before `stfs f24,64(r31)` at 0x823C5D60 in the
//     view setup sub_823C5A38 (ProStreet's sub_821DE9A8): f24 = P[0][0] and
//     f2 = P[1][1] (already negated: a positive scale keeps its sign), both
//     stored only after this point (view + 64 and + 128). r29 is the view,
//     its id at +4 (ProStreet: +8); the game's own 16:9 widening (0x823C5BA8)
//     tests 1, 5, 2, 6 and 39 (EVIEW_SHADOWMATTE moved from 40 to 39), the
//     only views changed here. The view table is at 0x832261D0, 544 bytes an
//     id, the target at +528.
//   - UltrawideFrontEnd, at the branch at 0x8275C8E8 that ends the widescreen
//     case of the front-end matrix (sub_8275C7E8): f13 its x scale, f0 its y
//     scale. The matrix is copied to 0x83668BE0 (sub_827516F8); no cache of
//     it like ProStreet's 0x82BFD5C4 was found (HUD draws seem to copy it by
//     value), so nothing is resent.
// And one function override: sub_82741E58, the mirror's rectangle builder
// (ProStreet's sub_82276488; called by sub_82749020 only), narrowed about the
// screen centre.
//
// Movies are EA VP6 as ProStreet's: Fill Screen is on only while the game
// draws a 3D view (HooksActive() while UltrawideProjection ran in the last
// half second).
#include <stdafx.h>
#include <game/ultrawide.h>

#include <kernel/function.h>

extern "C" PPC_FUNC(__imp__sub_82741E58);

namespace
{
    // Per-view table: 0x832261D0 + id * 544, the view's target at +528,
    // its width and height at +16 and +20.
    constexpr uint32_t kViewTable = 0x832261D0, kViewStride = 544, kViewTarget = 528;

    struct Scales
    {
        float x, y;
    };
    std::atomic<Scales> s_scales{ Scales{ 1.0f, 1.0f } };
    // When UltrawideProjection last ran (steady clock, ns; 0: never).
    std::atomic<int64_t> s_lastProjectionNs{ 0 };
    constexpr int64_t kActiveNs = 500'000'000;

    int64_t NowNs()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // A 3D view drawn in the last half second (see the file's header).
    bool Active()
    {
        const int64_t last = s_lastProjectionNs.load(std::memory_order_relaxed);
        return last && NowNs() - last < kActiveNs;
    }
    std::atomic<float> s_frontEndX{ 0.0f };
    std::atomic<float> s_frontEndY{ 0.0f };

    bool WidenedViewId(uint32_t id)
    {
        return id == 1 || id == 2 || id == 5 || id == 6 || id == 39;
    }

    void LogView(uint32_t id, bool widened)
    {
        static std::atomic<uint64_t> seen{ 0 };
        if (id >= 64 || (seen.fetch_or(1ull << id) & (1ull << id)))
            return;
        uint32_t target = ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(kViewTable + id * kViewStride + kViewTarget)));
        uint32_t w = 0, h = 0;
        if (target)
        {
            w = ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(target + 16)));
            h = ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(target + 20)));
        }
        fprintf(stderr, "[uw] view %u: target %ux%u%s\n", id, w, h, widened ? ", widened" : "");
    }

    void ScaleRegister(PPCRegister& f, float scale)
    {
        f.f64 = double(float(f.f64) * scale);
    }

    // About the screen centre (0.5 of it), for the mirror's 0..1 edges.
    void ScaleAboutCentre(PPCRegister& f, float scale)
    {
        f.f64 = double(0.5f + (float(f.f64) - 0.5f) * scale);
    }
}

namespace game
{
    void SetAspectScale(float scaleX, float scaleY)
    {
        const Scales next{ std::clamp(scaleX, 0.1f, 1.0f), std::clamp(scaleY, 0.5f, 1.0f) };
        const Scales last = s_scales.exchange(next, std::memory_order_relaxed);
        if (last.x != next.x || last.y != next.y)
            fprintf(stderr, "[uw] view scale x %.4f y %.4f\n", next.x, next.y);
    }

    float AspectScaleX()
    {
        return s_scales.load(std::memory_order_relaxed).x;
    }

    float AspectScaleY()
    {
        return s_scales.load(std::memory_order_relaxed).y;
    }

    bool HooksActive()
    {
        return Active();
    }

    bool TallHooksActive()
    {
        return Active();
    }

    float FrontEndScaleX()
    {
        return s_frontEndX.load(std::memory_order_relaxed);
    }

    float FrontEndScaleY()
    {
        return s_frontEndY.load(std::memory_order_relaxed);
    }

    // Carbon's pulsing minimap icon has no Undercover counterpart known:
    // Most Wanted's rule.
    bool FrontEndParksByCentre()
    {
        return false;
    }
}

void UltrawideProjection(PPCRegister& r29, PPCRegister& f24, PPCRegister& f2)
{
    if (!Active())
        fprintf(stderr, "[uw] 3D views drawn: Fill Screen on\n");
    s_lastProjectionNs.store(NowNs(), std::memory_order_relaxed);
    const Scales scale = s_scales.load(std::memory_order_relaxed);
    const uint32_t id = ByteSwap(*static_cast<const uint32_t*>(g_memory.Translate(r29.u32 + 4)));
    const bool widen = WidenedViewId(id);
    LogView(id, widen);
    if (!widen || (scale.x == 1.0f && scale.y == 1.0f))
        return;
    if (scale.x != 1.0f)
        ScaleRegister(f24, scale.x);
    if (scale.y != 1.0f)
        ScaleRegister(f2, scale.y);  // already negated: the sign stays
}

void UltrawideFrontEnd(PPCRegister& f13, PPCRegister& f0)
{
    const Scales scale = Active() ? s_scales.load(std::memory_order_relaxed) : Scales{ 1.0f, 1.0f };
    if (scale.x != 1.0f)
        ScaleRegister(f13, scale.x);
    if (scale.y != 1.0f)
        ScaleRegister(f0, scale.y);
    s_frontEndX.store(float(f13.f64), std::memory_order_relaxed);
    s_frontEndY.store(float(f0.f64), std::memory_order_relaxed);
}

// The mirror's rectangle (sub_82741E58): f1
// left edge, f2 top or bottom edge, f3 signed width, f4 height.
PPC_FUNC(sub_82741E58)
{
    const Scales scale = s_scales.load(std::memory_order_relaxed);
    if (scale.x != 1.0f)
    {
        ScaleAboutCentre(ctx.f1, scale.x);
        ScaleRegister(ctx.f3, scale.x);
    }
    if (scale.y != 1.0f)
    {
        ScaleAboutCentre(ctx.f2, scale.y);
        ScaleRegister(ctx.f4, scale.y);
    }
    __imp__sub_82741E58(ctx, base);
}
