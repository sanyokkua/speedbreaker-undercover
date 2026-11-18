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
