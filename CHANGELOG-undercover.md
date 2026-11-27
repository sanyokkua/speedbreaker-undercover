# Changelog

What changed in each SpeedBreaker: Undercover build, newest first. The game shows the entry once,
the first time a new version starts (offline: it's built into the game).

## v0.1.1

### Fixed

- **No light line along the top and left edges** of dark screens (the GPS map, fades) at an
  Internal Resolution above 1x.
- Fixes found while porting the Harry Potter Xbox 360 games, which run on the same runtime. None
  was seen to change Undercover; each makes it behave more like the console:
  - Threads waiting for the same signal get it in the order they started waiting, as on the
    console (Half-Blood Prince froze at its first logo without this).
  - Rectangles the game draws to clear or copy the screen are filled whichever corner it gives
    first, and a clear reaches only the video memory the console's would.
  - Objects the game marks for only part of the screen are drawn (they blinked or vanished in
    Half-Blood Prince). Undercover marks its objects for the whole screen, so this is not the
    cause of its one-frame flicker.
  - Vertex data can no longer be read while the game is half-way through rewriting it (one-frame
    "exploding" models in Order of the Phoenix).
  - Clearing the screen also clears the same video memory seen in another layout.
  - A system-settings answer (video region and country) fixed; Undercover doesn't use them.
  - The game's code is translated more exactly: 69 branches that were left out are now in; all
    were in code that never runs.

### Known issues

- Now and then a sign, a texture or a reflection disappears for a single frame (under
  investigation).

### For testers

A/B switches (environment variables, 0 = the old behaviour): `NFSMW_EDGE_SNAP`,
`NFSMW_DRAW_CLEAR`, `NFSMW_GUARD_NEIGHBOURS`, `NFSMW_TILE_PREDICATE`. Diagnostics:
`NFSMW_GLITCH_WATCH=1`, `NFSMW_GUARD_RACE=1`, `NFSMW_LOG_PREDICATE=1`.

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
