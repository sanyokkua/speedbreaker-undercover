# Changelog

What changed in each SpeedBreaker: Undercover build, newest first. The game shows the entry once,
the first time a new version starts (offline: it's built into the game).

## v0.1.1

- Fixed a thin light line along the top and left edges of dark screens, such as the GPS map, at an Internal
  Resolution above 1x.
- Updated the port's runtime and code translation tools to the latest version for better stability.
- Known issue: now and then a sign, a texture or a reflection disappears for a single frame.

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
