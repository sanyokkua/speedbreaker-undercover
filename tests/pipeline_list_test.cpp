// Tests for gpu::pipeline_list (runtime/gpu/pipeline_list.h): the file of
// pipelines seen (GetCachePath()/pipelines.list) that the renderer compiles
// again in the background at boot. From the repo root:
//   clang++ -std=c++20 -O2 -UNDEBUG -Iruntime tests/pipeline_list_test.cpp -o build/pipeline_list_test
//   build/pipeline_list_test
#include <gpu/pipeline_list.h>

#include <cstdio>
#include <cstring>

using namespace gpu::pipeline_list;

static int g_failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { g_failures++; printf("  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

constexpr uint32_t kKeySize = 80;

static Key MakeKey(uint8_t seed)
{
    Key k(kKeySize);
    for (uint32_t i = 0; i < kKeySize; i++)
        k[i] = uint8_t(seed + i);
    return k;
}

static Shader MakeShader(uint32_t type, uint32_t seed, uint32_t dwords)
{
    Shader s;
    s.type = type;
    for (uint32_t i = 0; i < dwords; i++)
        s.words.push_back(seed * 0x9E3779B9u + i);
    s.hash = HashWords(type, s.words.data(), uint32_t(s.words.size()));
    return s;
}

int main()
{
    {
        // The hash is the renderer's LoadShader hash: FNV-1a over the guest's
        // words, seeded with the type.
        const uint32_t w[2] = { 1, 2 };
        uint64_t h = 0xCBF29CE484222325ull ^ 1;
        h = (h ^ 1) * 0x100000001B3ull;
        h = (h ^ 2) * 0x100000001B3ull;
        CHECK(HashWords(1, w, 2) == h, "hash %016llx", (unsigned long long)HashWords(1, w, 2));
        CHECK(HashWords(0, w, 2) != h, "the type is part of the hash");
    }
    {
        // A round trip keeps the shaders and keys, in order.
        List in;
        in.shaders = { MakeShader(0, 1, 40), MakeShader(1, 2, 7) };
        in.keys = { MakeKey(3), MakeKey(9) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        List out;
        CHECK(Parse(file.data(), file.size(), kKeySize, out), "parse");
        CHECK(out.shaders.size() == 2 && out.keys.size() == 2, "%zu shaders, %zu keys", out.shaders.size(), out.keys.size());
        if (out.shaders.size() == 2 && out.keys.size() == 2)
        {
            CHECK(out.shaders[0].type == 0 && out.shaders[0].words == in.shaders[0].words && out.shaders[0].hash == in.shaders[0].hash, "shader 0");
            CHECK(out.shaders[1].type == 1 && out.shaders[1].words == in.shaders[1].words, "shader 1");
            CHECK(out.keys[0] == in.keys[0] && out.keys[1] == in.keys[1], "keys");
        }
    }
    {
        // An empty list is a valid file.
        std::vector<uint8_t> file = Serialize(List{}, kKeySize);
        List out;
        CHECK(Parse(file.data(), file.size(), kKeySize, out) && out.shaders.empty() && out.keys.empty(), "empty");
    }
    {
        // Nothing (no file yet) or garbage: no list.
        List out;
        CHECK(!Parse(nullptr, 0, kKeySize, out), "no data");
        const uint8_t junk[64] = { 1, 2, 3 };
        CHECK(!Parse(junk, sizeof(junk), kKeySize, out), "junk");
    }
    {
        // Another key layout (a build whose PipelineKey changed): ignored.
        List in;
        in.keys = { MakeKey(1) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        List out;
        CHECK(!Parse(file.data(), file.size(), kKeySize + 8, out), "another key size");
    }
    {
        // Another format version: ignored.
        List in;
        in.keys = { MakeKey(1) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        file[4] ^= 0xFF;
        List out;
        CHECK(!Parse(file.data(), file.size(), kKeySize, out), "another version");
    }
    {
        // Every truncation is rejected, never read past the end.
        List in;
        in.shaders = { MakeShader(0, 5, 16) };
        in.keys = { MakeKey(2), MakeKey(4) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        int accepted = 0;
        for (size_t n = 0; n < file.size(); n++)
        {
            std::vector<uint8_t> cut(file.begin(), file.begin() + n);
            List out;
            accepted += Parse(cut.data(), cut.size(), kKeySize, out);
        }
        CHECK(accepted == 0, "%d truncations accepted", accepted);
    }
    {
        // A shader whose words no longer hash to its hash (a damaged file):
        // the whole file is set aside.
        List in;
        in.shaders = { MakeShader(1, 8, 12) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        file[file.size() - 1] ^= 0x40;  // the last word of the shader
        List out;
        CHECK(!Parse(file.data(), file.size(), kKeySize, out), "damaged shader");
    }
    {
        // A shader too large to be one (a damaged count) is rejected.
        List in;
        in.shaders = { MakeShader(0, 3, 4) };
        std::vector<uint8_t> file = Serialize(in, kKeySize);
        const uint32_t huge = 0x7FFFFFFF;
        std::memcpy(file.data() + kHeaderSize + 4, &huge, 4);  // the shader's dword count
        List out;
        CHECK(!Parse(file.data(), file.size(), kKeySize, out), "huge dword count");
    }

    if (g_failures)
    {
        printf("pipeline_list_test: %d failure(s)\n", g_failures);
        return 1;
    }
    printf("pipeline_list_test: all passed\n");
    return 0;
}
