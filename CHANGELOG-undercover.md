# Changelog

What changed in each SpeedBreaker: Undercover build, newest first. The game shows the entry once,
the first time a new version starts (offline: it's built into the game).

## Unreleased

Runtime and recompiler fixes found by the Harry Potter and the Order of the Phoenix port (same
runtime), checked against Undercover one by one (docs/UNDERCOVER_SURVEY.md, "Fixes from the
OotP port").

- **Vertex data could be read half-rewritten** (the write watch opened a whole 16 KB host page
  while another 4 KB guest page in it still had a GPU read pending). Undercover hit it in 407
  frames of a 10-minute run; it caused no glitch seen there and does not explain the
  single-frame object flicker. `NFSMW_GUARD_NEIGHBOURS=0` restores the old rule; diagnostics
  `NFSMW_GUARD_RACE=1` and `NFSMW_GLITCH_WATCH=1`.
- **Clears drawn as 4x MSAA rectangles reach the 1x layout of the same EDRAM** (the scene colour
  and three depth targets in Undercover). `NFSMW_DRAW_CLEAR=0` turns it off.
- **No light line along the top and left edges** of dark menus (the GPS map, fades) at internal
  resolutions above 1x. `NFSMW_EDGE_SNAP=0` turns it off.
- `ExGetXConfigSetting`: the AV-region answer no longer falls through into the user settings,
  and the country is one byte.
- Recompiler: XenonRecomp patches 0012-0014. A branch to an address that starts no function is
  now an error instead of a silently dropped call. Undercover's 69 such branches were all in
  code that never runs (functions decoded from the 8 data bytes before exception-handling
  functions, which `survey/eh_data_ranges.py` now marks as data). Rebuild the tools to get the
  patches: `rm -rf tools/XenonRecomp && scripts/setup_tools.sh`.

## v0.1.0

### What's new

- **NFS: Undercover runs natively on Apple Silicon Macs**, from your own USA Xbox 360 disc
  (English, French and Spanish), with sound, controllers and saves.
- **Smoother driving**: game time moves in whole display refreshes, as in SpeedBreaker for Most
  Wanted and Carbon, so the world never skips or doubles a step.
- **Lower CPU use**: the game no longer spins a CPU core while it waits for the GPU.
- **Fill Screen** (Settings > Display > Aspect Ratio): on a screen of another shape than 16:9,
  such as a MacBook's 16:10, the game fills it with a taller view instead of black bars.
- Internal Resolution, upscaling, Brightness, Xbox 360 Display Gamma and the rest of the Settings
  menu, as in SpeedBreaker for Carbon. Auto (1.5x on a Mac) is the recommended Internal
  Resolution.

### Fixed on the way

- Cars, helicopters and the road are drawn (a texture-filtering shader instruction the other
  games never used).
- No more single over-bright or dark frames ("blinks"): the tone mapping no longer reads a stale
  copy of the scene.
- No more ~10 ms stall every frame from the colour-grading table the game rewrites each frame.
- Fewer dropped frames in free roam: the game no longer waits for the GPU each time it refills
  its streaming vertex buffers.

### Not there yet

- Busy races and free roam run between 30 and 60 fps on an M1 Pro (the GPU is the limit). Auto
  frame rate holds a steady 30 there.
- Rarely (about 1 start in 40 in testing) the game could close in its first seconds: a guard now
  skips the one copy it crashed on, but the cause (two game threads the console never runs at
  once) remains; if it still closes, start it again.
- Now and then a sign, a building's texture or the paint's reflection disappears for a single
  frame. Not caused by the renderer (ruled out one by one); most likely the game's render jobs
  and render thread running at the same time, as they never do on the console.
- With HDR Output on, the brightest whites clip (the game's calibration screen loses its faintest
  line): leave HDR Output off for the console's picture.
- Online play (offline only, as in the other SpeedBreaker games); split screen hasn't been looked
  at; the menu language follows the disc (no Language setting yet).
