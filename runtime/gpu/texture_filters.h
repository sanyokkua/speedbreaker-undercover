// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Texture filter overrides from the shader. A tfetch instruction carries its
// own mag, min, mip and anisotropic filters; each one is "use the fetch
// constant's" or a value that replaces the fetch constant's for that fetch
// (Xenia: VulkanTextureCache::GetSamplerParameters merges the shader's
// sampler binding over the fetch constant). HLSL sampler states compile into
// them, so a game can leave the fetch constant at "base map, point" and ask
// for trilinear anisotropic filtering in the shader; the fetch constant's
// filters alone would sample such a texture from level 0 only. DH1's sky
// shaders set 2:1 anisotropy and point filters this way.
//
// Vulkan-free: tests/texture_filters_test.cpp checks it alone; the shader
// translator records Pack() per fetch slot, the renderer applies Apply() to
// its copy of the fetch constant before choosing the sampler.
#pragma once
#include <cstdint>

namespace gpu::texture_filters
{
// Xenos values (xenos::TextureFilter, xenos::AnisoFilter).
constexpr uint32_t kFilterUseFetchConst = 3;
constexpr uint32_t kAnisoUseFetchConst = 7;

// One slot's overrides: bit 15 set when any filter is overridden; bits 0-1
// mag, 2-3 min, 4-5 mip, 6-8 aniso (the "use the fetch constant" values
// where not overridden). 0: no override.
inline uint16_t Pack(uint32_t mag, uint32_t min, uint32_t mip, uint32_t aniso)
{
    mag &= 3;
    min &= 3;
    mip &= 3;
    aniso &= 7;
    if (mag == kFilterUseFetchConst && min == kFilterUseFetchConst && mip == kFilterUseFetchConst &&
        aniso == kAnisoUseFetchConst)
        return 0;
    return uint16_t(0x8000u | mag | (min << 2) | (mip << 4) | (aniso << 6));
}

// Fetch constant dword 3: mag_filter at bit 19, min 21, mip 23, aniso 25.
constexpr uint32_t kMagShift = 19, kMinShift = 21, kMipShift = 23, kAnisoShift = 25;

// `dword3` with the overridden filters replaced.
inline uint32_t Apply(uint32_t dword3, uint16_t packed)
{
    if (!(packed & 0x8000u))
        return dword3;
    auto put = [&](uint32_t value, uint32_t useFetch, uint32_t shift, uint32_t mask) {
        if (value != useFetch)
            dword3 = (dword3 & ~(mask << shift)) | (value << shift);
    };
    put(packed & 3, kFilterUseFetchConst, kMagShift, 3);
    put((packed >> 2) & 3, kFilterUseFetchConst, kMinShift, 3);
    put((packed >> 4) & 3, kFilterUseFetchConst, kMipShift, 3);
    put((packed >> 6) & 7, kAnisoUseFetchConst, kAnisoShift, 7);
    return dword3;
}
}  // namespace gpu::texture_filters
