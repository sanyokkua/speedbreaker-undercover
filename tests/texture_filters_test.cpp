// Tests for gpu::texture_filters (runtime/gpu/texture_filters.h): a tfetch
// instruction's filters replace the fetch constant's, field by field, and
// "use the fetch constant" keeps it. The bit positions are checked against
// xenos::xe_gpu_texture_fetch_t; the translator and the renderer are read
// to check they record and apply the overrides. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/texture_filters_test.cpp -o build/texture_filters_test
//   build/texture_filters_test runtime/gpu/shader_translator.cpp runtime/gpu/renderer.cpp
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/texture_filters.h"
#include "gpu/xenos/xenos.h"

namespace xenos = xe::gpu::xenos;

namespace
{
    int s_failures = 0;
    void Check(bool ok, const char* what)
    {
        if (!ok)
        {
            std::printf("FAIL: %s\n", what);
            s_failures++;
        }
    }

    std::string Read(const char* path)
    {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    // The fetch constant's filters after Apply, read through the struct.
    xenos::xe_gpu_texture_fetch_t Applied(const xenos::xe_gpu_texture_fetch_t& f, uint16_t packed)
    {
        uint32_t words[6];
        std::memcpy(words, &f, sizeof(words));
        words[3] = gpu::texture_filters::Apply(words[3], packed);
        xenos::xe_gpu_texture_fetch_t out;
        std::memcpy(&out, words, sizeof(out));
        return out;
    }
}

int main(int argc, char** argv)
{
    using namespace gpu::texture_filters;
    using xenos::AnisoFilter;
    using xenos::TextureFilter;

    // No override at all packs to 0 and changes nothing.
    Check(Pack(3, 3, 3, 7) == 0, "all 'use fetch constant' packs to 0");
    Check(Apply(0x12345678u, 0) == 0x12345678u, "0 leaves dword 3 alone");

    // A fetch constant at point / base map, no anisotropy, other fields set.
    xenos::xe_gpu_texture_fetch_t f;
    std::memset(&f, 0, sizeof(f));
    f.swizzle = 0x688;
    f.exp_adjust = -3;
    f.mag_filter = TextureFilter::kPoint;
    f.min_filter = TextureFilter::kPoint;
    f.mip_filter = TextureFilter::kBaseMap;
    f.aniso_filter = AnisoFilter::kDisabled;
    f.arbitrary_filter = xenos::ArbitraryFilter(5);
    f.border_size = 1;

    // The shader asks for trilinear 16:1.
    auto t = Applied(f, Pack(1, 1, 1, uint32_t(AnisoFilter::kMax_16_1)));
    Check(t.mag_filter == TextureFilter::kLinear, "mag overridden");
    Check(t.min_filter == TextureFilter::kLinear, "min overridden");
    Check(t.mip_filter == TextureFilter::kLinear, "mip overridden");
    Check(t.aniso_filter == AnisoFilter::kMax_16_1, "aniso overridden");
    Check(t.swizzle == 0x688 && t.exp_adjust == -3 && uint32_t(t.arbitrary_filter) == 5 && t.border_size == 1,
        "other dword 3 fields kept");

    // Only the mip filter overridden: the rest stays the fetch constant's.
    auto m = Applied(f, Pack(3, 3, uint32_t(TextureFilter::kPoint), 7));
    Check(m.mag_filter == TextureFilter::kPoint && m.min_filter == TextureFilter::kPoint, "mag/min kept");
    Check(m.mip_filter == TextureFilter::kPoint, "mip overridden alone");
    Check(m.aniso_filter == AnisoFilter::kDisabled, "aniso kept");

    // An override down to point and no anisotropy from a linear constant.
    f.mag_filter = f.min_filter = f.mip_filter = TextureFilter::kLinear;
    f.aniso_filter = AnisoFilter::kMax_4_1;
    auto p = Applied(f, Pack(0, 0, 2, 0));
    Check(p.mag_filter == TextureFilter::kPoint && p.min_filter == TextureFilter::kPoint, "point overrides");
    Check(p.mip_filter == TextureFilter::kBaseMap, "base map override");
    Check(p.aniso_filter == AnisoFilter::kDisabled, "aniso disabled override");

    // The translator records Pack per slot; the renderer applies it.
    if (argc > 2)
    {
        const std::string tr = Read(argv[1]), re = Read(argv[2]);
        Check(tr.find("out.textureFilters[slot] = gpu::texture_filters::Pack(") != std::string::npos,
            "the translator records the tfetch filters");
        Check(re.find("gpu::texture_filters::Apply(fetch[3]") != std::string::npos,
            "the renderer applies them to the fetch constant");
    }

    if (s_failures)
        return 1;
    std::printf("texture_filters_test: all passed\n");
    return 0;
}
