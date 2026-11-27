# NFS: Undercover: how the port was made

SpeedBreaker: Undercover started as a copy of SpeedBreaker: ProStreet (itself a fork of
project(u)'s SpeedBreaker: Carbon). Undercover (EA Black Box, 2008) is ProStreet's engine one year
later, so the same pipeline carried over: survey the executable, recompile it with XenonRecomp,
boot it on the HLE runtime, then fix what the new game does differently. This page lists what
that was. [PROSTREET_SURVEY.md](PROSTREET_SURVEY.md) describes the pipeline itself.

## The disc

- `Need for Speed - Undercover (USA) (En,Fr,Es).iso`, title ID `0x45410876`.
- `default.xex` SHA-256 `97ac0c072253d46458dc6d57c6609922adbd3b9658216940282c361ad575e221`.
- Code 0x821E0000-0x83085E2C (15 MB, half again ProStreet's), about 120,000 functions.
- The same packed data layout as ProStreet (`NFS/*.BIN`, `ZDIR.BIN`), VP6 movies decoded by the
  game itself. 75 files, about 5 GB installed.

The build is `-DSB_GAME=undercover`. It defines `SB_GAME_CARBON`, `SB_GAME_PROSTREET` and
`SB_GAME_UNDERCOVER`, so it inherits both engines' code paths; Undercover's own addresses live in
`runtime/game/undercover/` and `config/undercover/undercover.toml`.

## Recompiling (4,041 errors to 0)

- **XenonRecomp patch 0011** (`patches/xenonrecomp/`):
  - `vpkuwus128`, a VMX128 pack instruction the other games never used.
  - A `data_ranges` setting: a 0x794-byte table after the import thunks is data, not code.
  - The disassembler matched a PowerPC 405 multiply-accumulate (`mullhwu.`) before Xenon's
    `vsldoi128` (both opcode 4). With VMX128 on, the 405 entries are skipped. ProStreet's output
    is byte-identical with the fix.
- **Survey scripts** (`survey/`):
  - `leaf_switch_bounds.py`: leaf functions that end in a `bctr` tail call.
  - `find_entry_points.py`: entry points right after a `bctr`, when data references them
    (vtable thunks that run into the next method).
  - `games.py`: by hand, 8 byte or halfword offset jump tables and 4 `b +4` stub bounds.
- **Dropped branches (2026-11-27, patches 0012-0014 from the OotP port):** XenonRecomp turned a
  branch to an address that starts no function into `// ERROR <addr>` and a `return`. Undercover
  had 69 (39 targets, in 29 functions); with patch 0014 each is a FATAL. All 69 are in code that
  never runs:
  - 48 in 19 functions XenonRecomp starts 8 bytes before a `.pdata` function (the two C++ EH data
    words in front of it decoded as code). Nothing calls them; the real function, 8 bytes later,
    has the same branches as `goto`s.
  - 18 in 7 SEH `__finally` handlers (`.rdata` scope entries `{begin, end, handler, 0}`, e.g.
    `82D49140 82D49168 82D49150 0`): one or two instructions inside the parent with an entry of
    their own, which the parent branches around, so the code after a handler looked like "the
    middle of another function". They run only when SEH unwinds, and the runtime has no SEH
    (`__C_specific_handler` and `RtlUnwind` are stubs, `RtlRaiseException` stops the game).
  - 3 in pieces of a parent that only the 19 dead starts branch to.

  None explains a known crash (the rare boot crash is in `sub_8275A5B0`'s resolve, the flicker
  is not a crash).

  Fix, the same as ProStreet's: `survey/eh_data_ranges.py` (from OotP) writes the 8 bytes before
  each of the 89 exception-handling functions as `data_ranges`. That removes 118 bogus starts and
  fragments (none referenced) and with them all 69 drops: 0 FATAL, 0 `ERROR` markers.
  `find_entry_points.py` (OotP's, with direct branches into another function's middle) then keeps
  23 SEH handlers callable that were function starts only through the bogus split: 32 entries
  (9 before). A rerun of both gives the same config. Its reading of the configured list no longer
  takes the `in .pdata fn 0x...` addresses in the comments for entries (they removed real
  function starts from its list). Patches 0012 (an invalid word inside a function) and 0013
  (basic-compressed XEX buffer) change nothing in Undercover's output (diffed: only the 69
  markers differed). Recompiling takes 7 s.

## Booting

- **Thread processors.** The job threads index per-processor data by the processor number in
  their PCR (+0x10C). `KeSetAffinityThread` (behind `XSetThreadProcessor`) didn't set it, so
  every thread claimed processor 0 and the jobs crashed within seconds. The runtime now sets it,
  and keeps it for a suspended thread until it starts.
- `GetExitCodeThread`'s XAPI wrapper: `sub_82D5BCA8`.
- **Still open (guarded):** about 1 boot in 40 crashed in its first 2 seconds in
  `D3DDevice_Resolve` (`sub_82935280`), reading a null depth-stencil surface (device +0x30A8).
  The render thread (processor 0) and the render jobs (`sub_82751068`, processors 1, 3 and 5)
  both run `sub_8275A5B0`, which sets the depth-stencil surface and resolves it; the owner field
  at device +10888 is only a debug check, and the game's frame handshake keeps them apart on the
  console but not on a Mac. `runtime/game/undercover/d3d_resolve.cpp` skips a resolve whose source
  surface is null. 60 boots afterwards: no crash, but the guard never fired either, so it is
  unproven.

## Graphics

- **`getWeights`** (texture fetch opcode 19): the bilinear weights of a fetch, which Undercover's
  pixel shaders use for their own filtering. Without it cars, helicopters and the road were
  missing.
- **Textures the CPU rewrites every frame.** A 256 KB linear 3D colour-grading table (`01ED7000`,
  written by vector code each frame) made the CPU wait ~10 ms a frame for the GPU under the write
  watch. Such textures are now copied into the ring at load (a slice at a time for linear
  volumes) for 300 frames after being seen changing, and not guarded.
- **Blinking** (single over-bright or dark frames): the same copy rule also caught the scene's
  resolve target, which the GPU writes. Its copy was taken from guest memory before the resolve
  landed, so the tone mapping read a stale scene. Memory with a GPU write pending, or owned by
  the GPU, is never copied now (as for vertex buffers). Measured on screen recordings (1-2 frame
  brightness spikes): 0.66 a second before, 0.01 after; the Xbox 360 reference has none.
- **Vertex buffers the CPU rewrites.** A 600 KB streaming buffer (`sub_8291CD20`) is refilled
  every frame in a chase, so the snapshot limit is 1 MB for Undercover. A copied range is checked
  again (guarded) after 300 frames, and that check costs one ~20 ms wait; each check that finds
  the range still rewritten now doubles the period (up to 64 times).
- **Not applicable:** Shadow Resolution. Undercover has no 1600x1600 sun shadow map (its depth
  targets are 1280 and 1040 wide), so the row is hidden.

## Single-frame object flicker (open)

Now and then a sign, a building's texture or the car paint's reflection disappears for one frame
(seen in a 60 fps screen recording: the Jason T's signs at 3.1 s). Ruled out, each by a run with
it switched off and the flicker still there: the dynamic texture copies
(`NFSMW_DYNAMIC_TEXTURE_GAP=2`), the vertex copies (`NFSMW_DYNAMIC_SNAPSHOTS=0`), scaled resolves
stored into textures (`NFSMW_SCALED_FUSE=0`), occlusion queries (`NFSMW_OCCLUSION_FAKE=1000`),
untiled rendering (`NFSMW_TILING=1`) and upscaling (`NFSMW_RESOLUTION=1x`); bin predication
(`SET_BIN_MASK`/`SET_BIN_SELECT`) drops only draws of masks the game never selects. Not reproduced
with frame dumps (they slow the game). Suspected: the render thread and the render jobs (the
boot crash's pair) building a frame's lists at once, which the console's scheduling prevents.
Also ruled out (2026-11-27): the write watch's neighbour-page race (next section), and
per-tile bin masks ("Fixes from the HBP port": every scene draw's mask selects tile 0).

## Fixes from the OotP port (2026-11-27)

The Order of the Phoenix port (`hp5-ootp-x360-port`, a copy of this tree) fixed four runtime
bugs and three recompiler ones. Each was checked against Undercover; all are in now. Runs: a
throwaway `HOME`, the disc extracted fresh, a new game (`NFSMW_INPUT_SCRIPT="10-70/2:A,
11-71/6:START,75-600/5:A,72~600:RT"`, 600 s at Frame Rate 60: the intro, the prologue chase,
sometimes free roam).

- **Write watch, neighbouring guest pages** (`kernel/write_watch.cpp`, OotP PORTING §8): a store
  to one 4 KB guest page opened its whole 16 KB host page while another guest page in it still
  had a GPU read pending; later stores to that page went unseen. Same script with
  `NFSMW_GUARD_RACE=1 NFSMW_GLITCH_WATCH=1`: the old rule (`NFSMW_GUARD_NEIGHBOURS=0`) had 491
  guest pages change while open (in 407 frames), the fix 126 (all texture untiles, which the
  fix leaves to the snapshot compare; 80 neighbour waits, none gave up). Each run logged 6
  `[glitch]` frames, the same scripted moments in both (scores 15.6/14.0, 107.5/104.4,
  94.7/94.6, ...), none within 2 frames of a race; two runs without `NFSMW_GUARD_RACE` logged
  none. So the race happens in Undercover but caused no glitch seen here, and does not explain
  the object flicker (a small sign vanishing may stay under the 32x18 grid's threshold, so this
  is not proof). Guard stalls stay 0.1-0.2 ms a frame.
- **Drawn clears** (`gpu/draw_clear.h`, OotP §4): Undercover has the pattern. Logged once per
  layout: the scene colour (base 0, pitch 640 at 4x MSAA, also clears pitch 1280 at 1x), depth at
  bases 1312 and 1328 (same), and depth at base 1216 (pitch 520 at 4x and 1040 at 1x, both ways).
  No visible difference found with it off (`NFSMW_DRAW_CLEAR=0`, new): the game draws over the
  whole scene. Kept, as it gives the guest's EDRAM result; 0 glitches with it on and off.
- **Edge snap** (`gpu/edge_snap.h`, OotP §9): at 2x a line along the top and left (light on
  dark screens: the GPS map's row 0 mean 10.2 against 0.2 below it, column 0 21.7 against 1.0;
  darker on the loading screen: 5.2 against 10.2). Frame dumps every 300 frames of a 320 s run:
  6 of 35 show it with `NFSMW_EDGE_SNAP=0` (largest row/column 0-1 gap 21.1), 0 of 33 with the
  fix (largest gap 1.3).
- **`ExGetXConfigSetting`**: the AV-region case fell through into the user settings, and the
  country is one byte. Undercover reads only category 3 (user) settings 0x0009 and 0x000A
  (`[xconfig]` lines), so this changes nothing it reads today.
- **Recompiler 0012-0014:** see "Recompiling".

With all of them on (and the final recompile), a new game played through the intro and the prologue chase in 600 s with no crash and no `[glitch]`; an earlier build with the runtime fixes reached free roam; `ctest`: 98 tests,
`swapchain_use_test` passes alone (it can fail in a parallel run, as before).

## Fixes from the HBP port (2026-11-27, v0.1.1)

The Half-Blood Prince port (`hp6-hbp-x360-port`, from the OotP tree) found three more general
runtime bugs. All are in; none changed what Undercover shows. Runs: 300 s each, a throwaway
`HOME`, the disc extracted fresh, a new game, the script above (the intro, the prologue chase,
the first free-roam drive), `NFSMW_GLITCH_WATCH=1`. A: the build before (cf7bc43); B: the
fixes with `NFSMW_TILE_PREDICATE=0`; C: the fixes. All three played the same route with no crash
or hang (`[perf]` lines at 60.0 fps: A 49 of 111, B 44 of 131, C 58 of 139; the rest are
loading and movies); `[glitch]` frames 4 / 7 / 7 (scores 6.5-141, at movie cuts and fades in all
three builds; the 32x18 grid can't see a small sign vanish, so this is no proof either way).

- **Wait order** (`kernel/wait_queue.h`, `tests/dispatcher_fifo_test.cpp`): a semaphore's count
  or an auto-reset event's signal goes to the threads already waiting, oldest first, as NT does
  inside the release. HBP's render thread took its own release back and the main thread waited
  forever. Undercover boots and plays as before.
- **Rectangle lists in both corner orders** (`gpu/rect_list.h`, `tests/rect_list_test.cpp`): the
  vertex shader assumed the right angle at v0 (`v3 = v1 + v2 - v0`); HBP's clears and resolves
  put it at v1. Now the corner opposite the longest edge. Plus `draw_clear.h` `EdramBounds`: a
  drawn clear is cut to the pitch's columns and the EDRAM rows after its base. Menus, movies and
  the chase look the same before and after (shots of runs A and C at the same times).
- **Predicated tiling with per-tile bin masks** (`gpu/tile_predicate.h`,
  `tests/tile_predicate_test.cpp`): untiled, a predicated scene draw runs in tile 0 when its mask
  selects any tile (HBP's right-half-only draws were never drawn). Undercover is not affected.
  `NFSMW_LOG_PREDICATE=1` over run C: three tiles (selects `80000003`, `C`, `30`); scene draws
  carry mask `FFFFFFFF` in tile 0 and `8000003F` in tiles 1 and 2; masks `3`, `C`, `30` (about
  15 draws and 290 other packets a frame each) are each tile's own copy and run in their tile.
  Draws only the new rule runs: 0 in the whole run. So this is not the single-frame flicker.
  `NFSMW_TILE_PREDICATE=0` keeps the old rule.

`ctest -j3`: 103 of 103 passed.

## Feature parity

Found by matching ProStreet's code:

| Feature | Undercover |
|---|---|
| Game time in whole display refreshes | quantiser `sub_8262F438`, world step `sub_8279DFF0`; mid-function hooks at `0x826178DC`, `0x8279E074`, `0x8279E044` |
| GPU-wait back-off | `sub_8291BAC8` (hang check), `sub_8291AB18` (fence wait), ProStreet's offsets |
| Sound mixer's CPU budget | `sub_829AA130`, config +204 |
| Fill Screen | `sub_823C5A38` (hook at `0x823C5D60`: r29, f24, f2), view table `0x832261D0` with the view id at +4 (ids 1, 2, 5, 6, 39), front end `0x8275C8E8`, mirror `sub_82741E58` |
| Language | not found yet (the row is hidden) |

## Performance (M1 Pro)

The GPU is the limit, and much of its time doesn't depend on resolution: at 720p a chase already
takes 13 ms. At 1.5x (Auto) a chase takes ~16 ms (60 fps); at 2x 19-25 ms. Busy races and free
roam take 20-24 ms at 1.5x, so Auto frame rate settles at 30 there.

## Look

HDR Output at 2x clips the brightest whites: the game's own brightness screen loses its faintest
line. HDR Output off with Xbox 360 Display Gamma on is the console's picture. Smoke over cars
still looks slightly off.
