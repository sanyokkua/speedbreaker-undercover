// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// The pipelines a player's game has drawn with, and the guest shaders they
// use, kept in the player's cache folder (GetCachePath()/pipelines.list,
// beside pipelines.bin). Vulkan-free: tests/pipeline_list_test.cpp checks it
// alone; gpu/renderer.cpp writes it with pipelines.bin and, at boot,
// translates the shaders and compiles every pipeline again on the pipeline
// workers, in the background ("warm-up", as Xenia's shader storage does).
//
// Why: a pipeline is only known when a draw first needs it, and a cold one
// takes 100-600 ms on MoltenVK. pipelines.bin alone keeps a pipeline cheap
// (~1 ms) the next time it is asked for, but it is still asked for mid-game,
// and macOS's own Metal cache (per executable, shared with every other
// program, rebuilt after an update) can be gone by then: the draws are
// skipped while it compiles, and objects pop in. Compiled at boot, a level's
// pipelines are ready before it is entered.
//
// The file holds the player's own disc's shader microcode: like the caches
// beside it, it is made on the player's Mac and never shipped.
//
// Layout (little-endian): header { 'SBPL', version, key size, shader count,
// key count }, then per shader { type, dword count, hash (8 bytes), words in
// guest order }, then the keys (the renderer's PipelineKey, raw). A shader's
// hash is LoadShader's, checked on load: any damage sets the file aside.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace gpu::pipeline_list
{
    constexpr uint32_t kMagic = 0x4C504253;  // "SBPL"
    // Bump when PipelineKey's meaning changes without its size changing.
    constexpr uint32_t kVersion = 1;
    constexpr size_t kHeaderSize = 20;
    constexpr uint32_t kMaxShaderDwords = 1u << 16;  // Xenos shaders are far smaller

    using Key = std::vector<uint8_t>;

    struct Shader
    {
        uint32_t type = 0;  // 0 vertex, 1 pixel (LoadShader's)
        uint64_t hash = 0;
        std::vector<uint32_t> words;  // guest order, as in memory
    };

    struct List
    {
        std::vector<Shader> shaders;
        std::vector<Key> keys;
    };

    // The renderer's shader hash (LoadShader): FNV-1a over the guest words.
    inline uint64_t HashWords(uint32_t type, const uint32_t* w, uint32_t n)
    {
        uint64_t h = 0xCBF29CE484222325ull ^ type;
        for (uint32_t i = 0; i < n; i++)
            h = (h ^ w[i]) * 0x100000001B3ull;
        return h;
    }

    inline std::vector<uint8_t> Serialize(const List& list, uint32_t keySize)
    {
        std::vector<uint8_t> out;
        auto put = [&](const void* p, size_t n) {
            const uint8_t* b = static_cast<const uint8_t*>(p);
            out.insert(out.end(), b, b + n);
        };
        auto put32 = [&](uint32_t v) { put(&v, 4); };
        put32(kMagic);
        put32(kVersion);
        put32(keySize);
        put32(uint32_t(list.shaders.size()));
        put32(uint32_t(list.keys.size()));
        for (const Shader& s : list.shaders)
        {
            put32(s.type);
            put32(uint32_t(s.words.size()));
            put(&s.hash, 8);
            put(s.words.data(), s.words.size() * 4);
        }
        for (const Key& k : list.keys)
            put(k.data(), keySize);
        return out;
    }

    // False (and `out` cleared) for a missing, foreign or damaged file.
    inline bool Parse(const uint8_t* data, size_t size, uint32_t keySize, List& out)
    {
        out = {};
        size_t at = 0;
        auto get = [&](void* p, size_t n) {
            if (size - at < n)
                return false;
            std::memcpy(p, data + at, n);
            at += n;
            return true;
        };
        uint32_t header[5];
        if (!data || keySize == 0 || !get(header, sizeof(header)) || header[0] != kMagic || header[1] != kVersion || header[2] != keySize)
            return false;
        const uint32_t shaders = header[3], keys = header[4];
        for (uint32_t i = 0; i < shaders; i++)
        {
            Shader s;
            uint32_t dwords = 0;
            if (!get(&s.type, 4) || !get(&dwords, 4) || !get(&s.hash, 8) || s.type > 1 || dwords == 0 ||
                dwords > kMaxShaderDwords || size - at < size_t(dwords) * 4)
                return out = {}, false;
            s.words.resize(dwords);
            get(s.words.data(), size_t(dwords) * 4);
            if (HashWords(s.type, s.words.data(), dwords) != s.hash)
                return out = {}, false;
            out.shaders.push_back(std::move(s));
        }
        if ((size - at) / keySize < keys || (size - at) != size_t(keys) * keySize)
            return out = {}, false;
        out.keys.resize(keys, Key(keySize));
        for (Key& k : out.keys)
            get(k.data(), keySize);
        return true;
    }
}
