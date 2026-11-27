// Tests for gpu::param_gen (runtime/gpu/param_gen.h): with SQ_PROGRAM_CNTL's
// param_gen bit the pixel shader register SQ_CONTEXT_MISC::param_gen_pos
// gets the pixel's position (vPos) and face (VFACE). The register fields are
// checked against xenos/registers.h's unions; the translator and renderer
// are read to check they write it. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/param_gen_test.cpp -o build/param_gen_test
//   build/param_gen_test runtime/gpu/shader_translator.cpp runtime/gpu/renderer.cpp
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/param_gen.h"
#include "gpu/xenos/registers.h"

namespace reg = xe::gpu::reg;

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
}

int main(int argc, char** argv)
{
    using namespace gpu::param_gen;

    // The register: off unless param_gen is set; the position from SQ_CONTEXT_MISC.
    reg::SQ_PROGRAM_CNTL cntl{};
    reg::SQ_CONTEXT_MISC misc{};
    misc.param_gen_pos = 11;
    Check(Register(cntl.value, misc.value) == kOff, "param_gen clear: no register");
    cntl.param_gen = 1;
    Check(Register(cntl.value, misc.value) == 11, "param_gen set: SQ_CONTEXT_MISC::param_gen_pos (DH1's 11)");
    cntl.vs_num_reg = 63;
    cntl.ps_num_reg = 63;
    cntl.vs_export_count = 15;
    misc.param_gen_pos = 2;
    misc.sc_sample_cntl = xe::gpu::xenos::SampleControl(3);
    Check(Register(cntl.value, misc.value) == 2, "other fields don't leak into the register");
    misc.param_gen_pos = 16;
    Check(Register(cntl.value, misc.value) == kOff, "a register past the 16 interpolators is dropped");

    // The value: whole guest pixels at 1x (vPos 0, 1, 2), from host .5 centres.
    Value v = Compute(0.5f, 0.5f, 1.0f, 1.0f, true);
    Check(v.x == 0.0f && !std::signbit(v.x) && v.y == 0.0f && v.z == 0.0f && v.w == 0.0f, "1x: pixel (0, 0) is +0, 0, 0, 0");
    v = Compute(640.5f, 359.5f, 1.0f, 1.0f, true);
    Check(v.x == 640.0f && v.y == 359.0f, "1x: pixel (640, 359)");
    // 2x: guest pixel 3's host pixels 6 and 7 read 3 and 3.5, never past 4.
    v = Compute(6.5f, 7.5f, 0.5f, 0.5f, true);
    Check(v.x == 3.0f && v.y == 3.5f, "2x: host (6, 7) is guest (3, 3.5)");
    // A back face sets x's sign bit, column 0 included (-0).
    v = Compute(0.5f, 10.5f, 1.0f, 1.0f, false);
    Check(std::signbit(v.x) && v.y == 10.0f, "back face in column 0: x is -0");
    v = Compute(5.5f, 0.5f, 1.0f, 1.0f, false);
    Check(v.x == -5.0f && !std::signbit(v.y), "back face: x negative, y not");

    // The translator writes paramGen() into the register the host defines
    // (literal indices: ScalarizeRegisters keeps working), after the
    // interpolators; the renderer defines it from the registers.
    if (argc > 2)
    {
        const std::string tr = Read(argv[1]), re = Read(argv[2]);
        Check(tr.find("#if PARAM_GEN == {0}\\n    r[{0}] = paramGen();") != std::string::npos,
            "translator: r[n] = paramGen() under PARAM_GEN == n");
        Check(tr.find("r[{0}] = i_i{0};") < tr.find("r[{0}] = paramGen();"), "translator: param gen replaces the interpolator");
        Check(tr.find("(gl_FragCoord.xy - 0.5) * vec2(uintBitsToFloat(pc.vfRedirect[0].w), uintBitsToFloat(pc.vfRedirect[1].w))") !=
                std::string::npos,
            "translator: guest pixels from gl_FragCoord and the scale");
        Check(tr.find("gl_FrontFacing ? xy.x : uintBitsToFloat(floatBitsToUint(xy.x) | 0x80000000u)") != std::string::npos,
            "translator: a back face sets x's sign bit");
        Check(re.find("gpu::param_gen::Register(s_regs[XE_GPU_REG_SQ_PROGRAM_CNTL], s_regs[XE_GPU_REG_SQ_CONTEXT_MISC])") !=
                std::string::npos,
            "renderer: the pipeline key takes the register");
        Check(re.find("preamble += std::format(\"#define PARAM_GEN {}\\n\", paramGen);") != std::string::npos,
            "renderer: the pixel variant defines PARAM_GEN");
        Check(re.find("memcpy(&pc.vfRedirect[0][3], &gx, 4);") != std::string::npos &&
                re.find("memcpy(&pc.vfRedirect[1][3], &gy, 4);") != std::string::npos,
            "renderer: the push constants carry guest pixels per host pixel");
    }

    if (s_failures == 0)
        std::printf("param_gen_test: all passed\n");
    return s_failures == 0 ? 0 : 1;
}
