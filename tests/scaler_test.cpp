// Tests for gpu::scaler (runtime/gpu/scaler.h): a front buffer the display
// scaler stretches is shown at the scaler's output shape (Deathly Hallows -
// Part 1: 1152x720 shown as 1280x720), anything else at its own. The renderer
// is read to check it passes that shape to the presenter. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/scaler_test.cpp -o build/scaler_test
//   build/scaler_test runtime/gpu/renderer.cpp
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/scaler.h"

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

    bool Is(gpu::scaler::Shape s, uint32_t w, uint32_t h) { return s.width == w && s.height == h; }
}

int main(int argc, char** argv)
{
    using gpu::scaler::DisplayShape;
    // DH1's VdInitializeScalerCommandBuffer: source 0x048002D0, output 0x050002D0.
    Check(Is(DisplayShape(1152, 720, 0x048002D0, 0x050002D0), 1280, 720), "DH1's 1152x720 is shown 1280x720");
    // No scaler set up: the buffer's own shape.
    Check(Is(DisplayShape(1152, 720, 0, 0), 1152, 720), "no scaler: the buffer's shape");
    // A 1:1 scaler (1280x720 to 1280x720).
    Check(Is(DisplayShape(1280, 720, 0x050002D0, 0x050002D0), 1280, 720), "1:1 scaler");
    // A source that is part of the buffer (1280x720 of OotP's 1280x736): left as it was.
    Check(Is(DisplayShape(1280, 736, 0x050002D0, 0x050002D0), 1280, 736), "a partial source rect is left alone");
    // A front buffer of another size than the scaler's source (a movie's, say).
    Check(Is(DisplayShape(1280, 720, 0x048002D0, 0x050002D0), 1280, 720), "another buffer size keeps its own");
    // A zero output is ignored.
    Check(Is(DisplayShape(1152, 720, 0x048002D0, 0), 1152, 720), "zero output ignored");
    Check(Is(gpu::scaler::Unpack(0x048002D0), 1152, 720), "unpack");

    if (argc > 1)
    {
        std::ifstream f(argv[1]);
        std::stringstream ss;
        ss << f.rdbuf();
        const std::string src = ss.str();
        Check(src.find("gpu::scaler::CurrentDisplayShape(width, height)") != std::string::npos,
            "renderer.cpp asks the scaler's shape for the front buffer");
        Check(src.find("SubmitFrontImage(front.image, outW, outH, shape.width, shape.height") != std::string::npos,
            "renderer.cpp presents at that shape");
    }
    if (s_failures == 0)
        std::printf("scaler_test: all passed\n");
    return s_failures == 0 ? 0 : 1;
}
