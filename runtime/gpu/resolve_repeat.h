// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
//
// Repeated resolves (gpu/renderer.cpp's Resolve; header only,
// tests/resolve_repeat_test.cpp).
//
// Some games resolve the same EDRAM rectangle to the same place twice with
// nothing drawn in between: Deathly Hallows Part 1 copies its scene's colour
// and depth (1152x720) out, and right after does both again, every frame
// (~0.4 ms of GPU time at 2x internal resolution). The second copy writes
// exactly the words the first did when
//   - its source, rectangle, destination and copy settings are the first's
//     (the Key; the clear bits aside: a clear runs after the copy, and the
//     clear's own writes to the target end the match, see below),
//   - no target was written since (a pass, a clear, a target copy: the
//     renderer's target epoch moves on any of them; EDRAM targets alias one
//     another, so any target write ends every match), and
//   - nothing wrote the destination's guest memory since the first copy (its
//     write sequence: a CPU store faults and moves it, another resolve marks
//     it).
// Then the copy is skipped (its clear still runs). The book holds the last
// few resolves (the game alternates colour and depth: A B A B).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace gpu::resolve_repeat
{
    struct Key
    {
        uint64_t source = 0;       // the source target (its identity and image)
        uint64_t sourceImage = 0;
        int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // the rectangle, guest pixels
        uint32_t destBase = 0, destPitch = 0, format = 0;
        uint32_t destInfo = 0;     // RB_COPY_DEST_INFO (endian, swap, exp bias, array)
        uint32_t copyControl = 0;  // RB_COPY_CONTROL without its clear bits (CopyBits)
        uint32_t surfaceInfo = 0;  // RB_SURFACE_INFO (pitch, MSAA)
        uint32_t extra = 0;        // anything else the caller's copy depends on (scaled samples wanted)
        bool operator==(const Key&) const = default;
    };

    // RB_COPY_CONTROL without color_clear_enable (bit 8) and depth_clear_enable (bit 9).
    constexpr uint32_t CopyBits(uint32_t copyControl) { return copyControl & ~0x300u; }

    class Book
    {
    public:
        static constexpr size_t kEntries = 4;

        // Whether a resolve of `key`, with the target epoch at `epoch`,
        // repeats a recorded one: `writtenSince(base, sequence)` says
        // whether its destination was written after the sequence the
        // recorded copy left there.
        template <class WrittenSince>
        bool Repeats(const Key& key, uint64_t epoch, WrittenSince&& writtenSince) const
        {
            for (const Entry& e : entries_)
                if (e.used && e.epoch == epoch && e.key == key)
                    return !writtenSince(e.written);
            return false;
        }

        // A copy of `key` ran at target epoch `epoch` and left sequence
        // `written` on its destination (0: unknown, not recorded).
        void Record(const Key& key, uint64_t epoch, uint64_t written)
        {
            for (Entry& e : entries_)
                if (e.used && (e.epoch != epoch || e.key == key))
                    e.used = false;  // stale, or this key's older copy
            if (written == 0)
                return;
            Entry& slot = entries_[next_++ % kEntries];
            slot = { key, epoch, written, true };
        }

        void Clear()
        {
            for (Entry& e : entries_)
                e.used = false;
        }

    private:
        struct Entry
        {
            Key key;
            uint64_t epoch = 0, written = 0;
            bool used = false;
        };
        std::array<Entry, kEntries> entries_{};
        size_t next_ = 0;
    };
}
