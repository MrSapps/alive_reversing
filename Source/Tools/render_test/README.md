# Render test

`relive_render_test` draws a series of test scenes with the real renderers, so that you
can see whether they're drawing correctly. It needs no game data. Use it when you change
a renderer, port one to a new platform, or compare the renderers' speed.

Everything it draws goes through the engine's own code: the ordering table,
`PSX_DrawOTag`, `IRenderer`, `Animation`, `AliveFont`, `ScreenManager`, `FG1`, and the real
game objects for the effects (`LaughingGas`, `AO::ScreenWave`, `ZapLine`, `ScreenClipper`,
`ScreenShake`, `Fade`, `Flash`, `DeathGas`, `MainMenuTransition`). Cameras, FG1 layers and
sprite sheets are generated in code (`TestResources.cpp`). The animations the game objects
load are added to the resource manager with `ResourceManagerWrapper::AddAnimation`.

## Looking at it

```sh
build/Source/Tools/render_test/relive_render_test                    # starts on OpenGL
build/Source/Tools/render_test/relive_render_test -renderer=sdl3
build/Source/Tools/render_test/relive_render_test -scene=blend       # start at a scene
build/Source/Tools/render_test/relive_render_test -list              # scene names
```

The top line names the scene. On the right are the renderer, the frame time, the draw
calls and the number of cached textures. The bottom lines say what a correct renderer
draws.

| Key | |
|---|---|
| Left / right | Previous / next scene |
| R | Switch renderer (the scene carries on) |
| H | Hide the text |
| P | Pause |
| F | Filtering when scaling to the window (off by default, so you can see single pixels) |
| U | Uncapped frame rate (30 fps by default, like the game) |
| C | Save the frame as a PNG in `render_test_out/<renderer>/` |
| Escape | Quit |

Palette effects such as invisibility aren't tested yet, as they're known to be broken.

## Unattended (`-auto`)

```sh
build/Source/Tools/render_test/relive_render_test -auto -out=rt_out
```

This runs every scene on each renderer in turn, then writes `rt_out/report.txt` (also
printed), `rt_out/perf.csv`, and a PNG of each scene for each renderer. It exits non zero
if a check fails.

Checks (FAIL):

- **Static scenes draw the same twice in a row.** A difference means the renderer keeps
  state from frame to frame that it shouldn't. The difference is saved as `unstable_*.png`.
- **Nothing leaks into the next frame.** The reference frame (scene 1) is drawn again after
  every scene and must match exactly. This catches blend modes, clip rectangles,
  framebuffers and so on left set. The difference is saved as `leak_after_*.png`.
- **No objects are left behind.** Game objects, drawables and animations go back to the
  number there was before each scene.
- **No textures are left behind.** Once the scenes are over, and after longer than any
  renderer keeps an unused texture, the texture cache is back to its starting size.
- **The renderers draw the same.** With more than one renderer, no more than 0.1% of a
  scene's pixels may differ by more than 8 levels. What's left between them is which pixel
  wins where a triangle edge or texel boundary falls exactly on a pixel centre, which differs
  between GPUs and SDL's backends. The differences are saved in `diff_<a>_<b>/`.
- **With `-baseline=<dir>`, captures are unchanged.** `<dir>` is an earlier `-auto` run's
  output. Anything different fails, and the difference is saved as `baseline_diff_*.png`.
  Look at those, and if the change was intended, use the new run as the baseline. GPUs and
  drivers round differently, so keep baselines per machine.

Reported, not failed:

- Frame times per scene and renderer: average, 95th percentile and worst. This is CPU time
  for the whole frame, including the renderer's work and presenting it, so it includes GPU
  time only where the driver waits for the GPU. There are also draw calls and texture
  uploads per frame. The stress scenes are timed for 4 times as long (`-frames` sets the
  count).
- How much each scene differs between the renderers.
- WARN if a frame that draws nothing still shows the last frame. The renderers don't clear
  the framebuffer. The game doesn't need them to, as it draws a full screen camera every
  frame. Every scene does the same.

Captured frames are the 640x240 PSX framebuffer, before it's scaled to the window. So
screen shake, filtering and the window size don't affect them.

### Memory leaks

On Linux, Debug builds have ASan, which reports leaks at exit. Run on a desktop:

```sh
ASAN_OPTIONS=detect_leaks=1 build/Source/Tools/render_test/relive_render_test -auto
```

The ctest (`relive_render_test`) runs without a window, using `SDL_VIDEO_DRIVER=offscreen`,
where Mesa's EGL drivers are unloaded before ASan checks for leaks. Their own allocations
then look leaked, so the ctest turns the leak check off. SDL's software renderer can't do
the custom blend modes the SDL3 renderer needs, so the SDL3 renderer refuses to start on it.
Use `SDL_RENDER_DRIVER=opengl`.

## Adding a scene

Add a class derived from `Scene` to `Scenes.cpp` and list it in `AllScenes()`:

- `Title()` and `Expected()` are shown on screen. Say what a correct result looks like,
  concretely enough that someone who doesn't know the renderer can tell.
- `Enter()` makes what the scene draws. Game objects are made with `relive_new` as usual.
  The runner destroys them when the scene ends. Sprites without a game object are
  `SceneSprite`s the scene owns.
- `Render()` adds the scene's own primitives to the ordering table. Draw a camera or
  `DrawBackground()` first, as the framebuffer isn't cleared. Within a layer, the ordering
  table draws what was added last first, and scenes add theirs after the game objects.
- `IsStatic()` for scenes where nothing moves, so they're checked for drawing the same
  twice. `IsStress()` for performance scenes. `CaptureFrame()` picks the frame that's saved.

Keep scenes deterministic. The runner resets the game's random seed and frame counter when
a scene starts. Use `SceneRandom` rather than anything time based.
