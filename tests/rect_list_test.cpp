// Tests for gpu::rect_list (runtime/gpu/rect_list.h): a rectangle list's
// three corners, in either Direct3D order (right angle at v0 or at v1) or
// the third one, give two triangles of one winding that cover the whole
// rectangle exactly once. Half-Blood Prince's scene clear (0,0 320,0
// 320,736) lost its lower-left half when the 4th corner was taken as
// v1 + v2 - v0. The shader's copy of the rule (shader_translator.cpp) is
// checked in the source given as the argument. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/rect_list_test.cpp -o build/rect_list_test
//   build/rect_list_test runtime/gpu/shader_translator.cpp
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "gpu/rect_list.h"

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

    float Cross(const float* a, const float* b, const float* c)
    {
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    }

    bool Inside(const float* a, const float* b, const float* c, float x, float y)
    {
        const float p[2] = { x, y };
        const float d0 = Cross(a, b, p), d1 = Cross(b, c, p), d2 = Cross(c, a, p);
        return (d0 > 0 && d1 > 0 && d2 > 0) || (d0 < 0 && d1 < 0 && d2 < 0);
    }

    // Every sample of the box x0..x1, y0..y1 (off the edges) is in exactly
    // one of the two triangles, both wound the same way.
    bool CoversOnce(const float v[3][2], float x0, float y0, float x1, float y1)
    {
        float o[6][2];
        gpu::rect_list::Expand(v, o);
        const float w0 = Cross(o[0], o[1], o[2]), w1 = Cross(o[3], o[4], o[5]);
        if ((w0 > 0) != (w1 > 0) || w0 == 0 || w1 == 0)
            return false;
        for (float y = y0 + 0.37f; y < y1; y += (y1 - y0) / 53.0f)
            for (float x = x0 + 0.41f; x < x1; x += (x1 - x0) / 47.0f)
                if (int(Inside(o[0], o[1], o[2], x, y)) + int(Inside(o[3], o[4], o[5], x, y)) != 1)
                    return false;
        return true;
    }

    std::string Read(const char* path)
    {
        std::ifstream f(path);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }
}

int main(int argc, char** argv)
{
    using gpu::rect_list::RightAngleCorner;
    // Order of the Phoenix's form: right angle at v0.
    {
        const float v[3][2] = { { 0, 0 }, { 640, 0 }, { 0, 368 } };
        Check(RightAngleCorner(v[0], v[1], v[2]) == 0, "0,0 w,0 0,h: corner v0");
        Check(CoversOnce(v, 0, 0, 640, 368), "0,0 w,0 0,h covers the rectangle once");
    }
    // Half-Blood Prince's clear and resolves: right angle at v1.
    {
        const float v[3][2] = { { 0, 0 }, { 320, 0 }, { 320, 736 } };
        Check(RightAngleCorner(v[0], v[1], v[2]) == 1, "0,0 w,0 w,h: corner v1");
        float o[6][2];
        gpu::rect_list::Expand(v, o);
        Check(o[4][0] == 0 && o[4][1] == 736, "0,0 w,0 w,h: the 4th corner is 0,h (not 2w,h)");
        Check(CoversOnce(v, 0, 0, 320, 736), "0,0 w,0 w,h covers the rectangle once");
    }
    // Right angle at v2, and screen-space offsets (a tile at 640).
    {
        const float v[3][2] = { { 640, 0 }, { 1280, 736 }, { 1280, 0 } };
        Check(RightAngleCorner(v[0], v[1], v[2]) == 2, "corner v2");
        Check(CoversOnce(v, 640, 0, 1280, 736), "right angle at v2 covers the rectangle once");
    }
    // Clip-space corners (y up, a non-square viewport): the same rules.
    {
        const float v[3][2] = { { -1, 1 }, { 1, 1 }, { 1, -1 } };
        Check(RightAngleCorner(v[0], v[1], v[2]) == 1, "clip space: corner v1");
        Check(CoversOnce(v, -1, -1, 1, 1), "clip space covers the rectangle once");
    }
    // A degenerate rectangle keeps the old rule (corner v0).
    {
        const float v[3][2] = { { 5, 5 }, { 5, 5 }, { 5, 5 } };
        Check(RightAngleCorner(v[0], v[1], v[2]) == 0, "degenerate: corner v0");
    }

    // The shader applies the same rule.
    if (argc >= 2)
    {
        const std::string shader = Read(argv[1]);
        Check(shader.find("uint corner = (d02 > d01 && d02 > d12) ? 1u : (d01 > d02 && d01 > d12) ? 2u : 0u;") != std::string::npos,
            "shader picks the corner opposite the longest edge");
        Check(shader.find("pick = k < 3u ? 0u : k - 2u;") != std::string::npos, "second triangle: a, the 4th corner, b");
        Check(shader.find("pos = pick == 1u ? pa : pick == 3u ? pb : pa + pb - pcorner;") != std::string::npos,
            "shader places a, b and the 4th corner");
        Check(shader.find("it[i] = pick == 1u ? ia : pick == 3u ? ib : ia + ib - ic;") != std::string::npos,
            "interpolants follow the corners");
        Check(shader.find("v3 = v1 + v2 - v0") == std::string::npos, "the old fixed corner is gone");
    }
    else
        Check(false, "source not given");

    std::printf("%s\n", s_failures ? "rect_list_test: FAILED" : "rect_list_test: OK");
    return s_failures ? 1 : 0;
}
