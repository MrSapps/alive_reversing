---
name: run-game
description: Run relive (AO or AE) headlessly with an -automation script to start at any camera, take screenshots and dump the game state and objects as JSON. Use to see or check anything in the real game (rendering bugs, object state, a level's objects) without the user playing.
---

# Driving the game with -automation

`relive -automation=<script>` runs a script of commands. See `Source/relive_lib/Automation.hpp`
for the full list: `start`, `wait_frames`, `wait_until`, `hold`, `screenshot`, `dump_state`,
`dump_objects`, `timeout`, `quit`. `hold down 40` holds pad buttons (joined with `+`, e.g.
`left+run`) for that many frames.

Game data: AO in `/home/snake/dev/oddworld/ao`, AE in `/home/snake/dev/oddworld/ae`. The engine
loads from the working directory, so `cd` there. Put scripts and output in the scratchpad and use
absolute paths in the script.

```sh
S=<scratchpad>/auto; mkdir -p $S
cat > $S/look.txt <<EOF
start rupture_farms 15 1          # level path camera [x y]; no menus or movies
wait_frames 30
dump_state $S/state.json
dump_objects $S/objects.json
screenshot $S/shot.png
EOF
cd /home/snake/dev/oddworld/ao && SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy \
  ASAN_OPTIONS=detect_leaks=0 timeout 150 \
  /home/snake/dev/alive_reversing/build/Source/relive/relive -AO -automation=$S/look.txt > $S/log.txt 2>&1
grep automation $S/log.txt
```

- Exit code 1 means the script failed (the log says which line and why) or ASan found something.
- `SDL_VIDEO_DRIVER=offscreen` means no window pops up on the user's desktop. Its GL stack leaks
  about 7 KB in unknown modules at exit, so use `detect_leaks=0` with it. To check for leaks, run
  without offscreen instead.
- Add `-renderer=opengl` or `-renderer=sdl3` to compare renderers. Display options aren't saved
  to relive.ini during automation runs.
- Level names are the ones in `data_conversion/EnumSerialization.hpp` (e.g. `rupture_farms`,
  `mines`). The menus' level-select tables (`sLevelList` in the MainMenu.cpp files) list known-good
  start cameras.
- The screenshot is the 640x240 PSX framebuffer. View it with Read. Crop or zoom with PIL to look
  at details.
- `dump_objects` gives each object's type, position, tint and animation (id, frame, layer,
  semi_trans, blend_mode). Some classes set their type to `eNone` (e.g. AO's DoorLight): find
  those by `anim.id`.

# Recording the audio

`-dump_audio=<file.wav>` saves everything the game plays. The sound is then rendered offline, one
1/30 s frame per game frame, so the WAV is locked to game time whatever the audio driver does
(SDL's dummy driver drifts). Example, the AO secret-area jingle: start one screen above it and
hoist down onto the trigger:

```
start rupture_farms 15 1 1565 328
wait_frames 40
hold down 60
wait_frames 450
quit
```

Compare recordings with `build-ps1re/pitchcmp.py <wav> <t0> <t1>` (strongest partials as MIDI
notes every 30 ms); a real PS1 recording of that jingle is `build-ps1re/rec/ps1_secret.wav`.
