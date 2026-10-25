# R.E.L.I.V.E.
An Open-Source Engine Replacement for Oddworld: Abe's Oddysee and Oddworld: Abe's Exoddus.

R.E.L.I.V.E. is a fan-made, open-source project that intends to become a fully compatible engine replacement for Oddworld Inhabitants' iconic first two games. The project's goals include fixing the original games' bugs and also eventually providing a modding / level creation interface, alongside of course making it possible to study or use the engine for new projects.

For more details, please check the project's website: https://aliveteam.github.io/

## Function Keys
| Key | Name |
| :-: | :--: |
| F5 | Quiksave |
| F6 | Quikload |
| F9 | Original Resolution |
| F10 | Screen Filter |
| F11 | Keep Aspect Ratio |
| F12 | Fullscreen |

## Settings (relive.ini)

Settings are saved in `relive.ini` in the game data directory:

- `[Control]`, `[Keyboard]`, `[Gamepad]`, `[Alive]`: controller choice and key/button bindings.
  These used to live in `abe2.ini`, which is no longer read.
- `[Display]`: remembered between runs.
  - `renderer`: `sdl3` or `opengl`. Set with `-renderer=` on the command line or by editing
    the file; used from the next start, since the renderer can't change while running.
  - `fullscreen`, `keep_aspect_ratio` (4:3 when `true`, stretched to the window when
    `false`), `filter_screen`, `use_original_resolution`: toggled in game with the function
    keys above, or set on the command line.

## Command Line Options

Run `relive` from the game data directory. Each option is a separate argument; values use
`-name=value` (quote the whole argument if the value has spaces, e.g. `"-mod=My Mod"`).

| Option | Description |
| :-- | :-- |
| `AE` or `-AE` | Run Abe's Exoddus (the default). |
| `AO` or `-AO` | Run Abe's Oddysee. |

If the chosen game's files aren't in the directory, the other game is run. If both `AE` and
`AO` are given, a warning is logged and the default is used.

| Option | Description |
| :-- | :-- |
| `-mod=<name>` | Run a mod from `relive_data/mods`, by its directory or its name. |
| `-renderer=<name>` | `sdl3` (default) or `opengl` (`sdl`, `gl`, `gl3` and `opengl3` also work). |
| `-fullscreen` | Start fullscreen. |
| `-keep_aspect_ratio` | Letterbox to 4:3. `-keep_aspect_ratio=false` stretches to the window instead (e.g. 16:9). |
| `-filter_screen` | Filter (smooth) the scaled image. |
| `-use_original_resolution` | Render at the original 640x240 and scale up. |
| `-ddcheat` | Enable the debug cheat menu. Builds with `FORCE_DDCHEAT` (the default) always have it. |
| `-ddfps` | Show the renderer, frame rate, frame time, draw calls and cached textures in the top right (`-show_fps` and `-showfps` also work). The frame time leaves out time spent waiting for the next frame's turn. |
| `-max_fps=<n>` | Run at up to `n` frames a second instead of the original 30, or `0` for no limit (`-maxfps` also works). The game runs faster the more frames it shows, so this also speeds up playback of a recording. |
| `-ddnoskip` | Render every frame instead of skipping frames to keep up. |
| `-ddslowload=<ms>` | Make each resource loaded in the background take at least this many milliseconds, to test loading on slow storage. |
| `-renderer_checks` | Check for renderer errors as it goes, which is slower. The OpenGL renderer checks every OpenGL call and stops on an error. The SDL3 renderer turns on the validation of SDL's GPU, Vulkan and Direct3D 11 backends, where they are used. |
| `-ps1_data=<dir>` | Play the PS1's own sound data (VH, VB and SEQ files) instead of the PC's, read from the LVL files at any depth below `<dir>`, for example the files of a PS1 disc. The PC's samples are the PS1 encoder's input, so they are close, but not the same. |
| `-spu_filter=<name>` | How voices played above 44100 Hz are resampled: how voices played above 44100 Hz are resampled. `hq` (default) filters out what would alias. `gaussian` is the PS1's own interpolation, exactly, which buzzes on some bright samples at high notes (for example the end of the secret area jingle). |
| `-convert` | Convert the game data (into `relive_data`) if it is out of date, as the first run does, then exit without opening a window. For scripts and headless machines. Exit code 1 if the game files are missing. |
| `-encode_webm` | When the game data is converted, encode the movies to AV1/Vorbis `.webm` files in `relive_data/<game>/fmvs` instead of copying the game's own movie files. The encoding takes a long time. The engine plays either: a `.webm` is played in place of the original movie with the same name. Only applies when the movies are converted (the first run, or after the conversion version changed). |
| `-help` | Show the command line options and exit (`--help`, `-h` and `/?` also work). |

The display options (`-renderer`, `-fullscreen`, `-keep_aspect_ratio`, `-filter_screen`,
`-use_original_resolution`) also take `=true` or `=false`, and are saved to `relive.ini`, so
they stay set on later runs.

### Recording and playback

Recordings capture the input and events of a play session so it can be replayed exactly,
e.g. to reproduce bugs or check that changes don't alter gameplay. A recording has to be
played back with the same game, data and settings (key bindings are saved in the recording).

| Option | Description |
| :-- | :-- |
| `-record=<file>` | Record this session to `<file>`. |
| `-flush` | With `-record`, write every change to disk straight away, so a recording survives a crash. |
| `-play=<file>` | Play back a recording. Playback stops with an error if the game stops matching the recording (a desync). |
| `-ignore_desyncs` | With `-play`, keep going after a desync instead of stopping (it logs one warning). |

### Automation

`-automation=<file>` runs a script of commands, so a tool (or a test) can drive the game
without anyone watching: start a new game at any camera, wait, take screenshots and dump the
game's state and objects as JSON. See `Source/relive_lib/Automation.hpp` for the commands. With
`SDL_VIDEO_DRIVER=offscreen` it needs no display. Display options given with it aren't saved to
`relive.ini`.

## Mods

A mod is a directory in `relive_data/mods` (it is created by the data conversion). Run it with
`-mod=<name>` (see [Command Line Options](#command-line-options)); the editor makes them too.
A mod only contains the files it changes: everything else comes from the game's converted data.

### modinfo.json

The directory must hold a `modinfo.json`, or it isn't seen as a mod:

```json
{
  "name": "My Mod",
  "author": "Me",
  "target_game": "AE"
}
```

- `name`: shown in the window title. `-mod=` takes this or the directory's name.
- `author`: who made it.
- `target_game`: `AE` or `AO` (any case). Running the mod with the other game is a fatal error, as
  is a `-mod=` that matches no mod.

All three are required; a `modinfo.json` that is missing one is skipped.

### Where files are looked for

Each resource is looked for in these data directories, in order, and the first one that has it is
used, so a mod's file replaces the game's one with the same path:

1. `relive_data/mods/<mod>` (if `-mod` is given)
2. `relive_data/ae` or `relive_data/ao`, the game being run
3. the other game's directory

A resource found nowhere is reported as missing, with every path that was tried. The paths in the
directory are the same as in `relive_data/ae` and `relive_data/ao`:

| Resource | Path (below the data directory) |
| --- | --- |
| Animation | `animations/<group>/<name>.json` and `.png` (and `_mask.png`). With a sound theme or other theme, `animations/<group>/<theme>/<name>`. |
| Animation on a sprite page | `animations/<group>/<name>.packed.json`, written by the conversion, says which page in `animations/pages` it is on |
| Camera | `levels/<level>/<path>/<camera>.png`, and `<camera>.json` with its foreground layers |
| Paths of a level | `levels/<level>/level_info.json` and the path jsons it lists |
| Palette | `<name>.pal` |
| Font | `<name>.png` |
| Sound | `sounds/<theme>/<file>` |
| Movie | `fmvs/<name>.webm`, else `fmvs/<name>.DDV` |

Some details:

- **Sprite sheets and pages.** An animation's `.png` and `.json` are the source. For speed the
  conversion packs the sheets of a group into pages (`animations/pages`), and the game then reads
  the `.packed.json` if there is one. A mod's sheet is never drawn from the game's pages, since
  those don't have it. Running `relive -mod=<name> -convert` makes the mod's own pages (it is also
  done at the first start), and a mod with no sheets of its own uses the game's. Delete the mod's
  `animations/pages` and `.packed.json` files to go back to plain sheets.
- **Movies.** A `.webm` is played in place of the movie with the same name, from any of the
  directories above, even if the game's own movie file is in a directory earlier in the list. So an
  HD movie in a mod replaces the original. The movies the conversion copies are the game's own
  files (AE's DDV, AO's PSX STR, which is also named `.DDV`). `-encode_webm` makes the conversion
  encode them to webm instead.
- **Sounds.** `-ps1_data=<dir>` replaces the sound files in all of the above.

## Contributing

Anyone who wishes to contribute is encouraged to join the project's [Discord](
https://discord.gg/khs6KKS), where most of the communication happens.

By contributing to this project, the contributor agrees and accepts that their code will be licensed under a GPL-compatible license (most likely the [MIT/Expat](https://opensource.org/licenses/MIT) license) in the future.

## Building R.E.L.I.V.E.

### Cloning

Since the project uses third-party repositories for some of its functions, you need to clone the project using the `--recursive` flag.

```
git clone --recursive https://github.com/AliveTeam/alive_reversing.git
```

#### **Regardless of your platform, you need to create a folder called `build` in your repository root! `.gitignore` is configured to ignore this folder. This helps to prevent polluting the commits with binaries.**

<details>
<summary>Build on Windows using Visual Studio 2022</summary>

#### Prerequisites
1. [CMake](https://cmake.org/)
2. [SDL3](https://github.com/libsdl-org/SDL/releases)
3. [Straweberry Perl](https://strawberryperl.com/)
4. Qt 5.15.2


Optionally, install Qt 5.15.2 with [aqtinstall](https://github.com/miurahr/aqtinstall/releases) if you don't want to create an account on their website.
```
aqt install-qt windows desktop 5.15.2 win64_msvc2019_64
```

### Building
1. Cd into the build directory:
```
cd build
```

2. Generate the solution file:
```
cmake -S .. -B . -DSDL3_DIR=YOUR_SDL3_PATH -DCMAKE_PREFIX_PATH=YOUR_QT5_PREFIX_PATH
```

For example, if you installed Qt5 at `C:\Qt` and SDL3 at `C:\SDL3` you would run:
```
cmake -S .. -B . -DSDL3_DIR=C:\SDL3\cmake -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
```

3. After cmake is done, open the generated `relive.sln` file within your `build` folder with Visual Studio 2022.
4. To start the build, click on `Build` -> `Build Solution` and wait for the build to finish.
5. Once the build has finished successfully, you'll find the relive executable in `build/Source/relive/Debug` and the editor executable in `build/Source/Tools/editor/Debug`.
</details>


<details>
<summary>Build on Linux</summary>

#### Prerequisites

1. [CMake](https://cmake.org/)
2. [SDL3](https://www.libsdl.org/)
3. [Perl](https://www.perl.org/)
4. Qt 5.15.x

#### Ubuntu
```
sudo apt install cmake libsdl3-dev perl qtdeclarative5-dev qtmultimedia5-dev qttools5-dev
```

1. Cd into the build directory:
```
cd build
```

2. Generate the makefile:
```
cmake -S .. -B .
```

3. Build relive:
```
make -j$(nproc)
```

4. Once the build has finished successfully, you'll find the relive executable in `build/Source/relive/Debug` and the editor executable in `build/Source/Tools/editor/Debug`.
5. You can optionally install the package using `make install` or create a Debian-compatible package using `cpack -G DEB`.

</details>

## Editor translations

The editor's translations live in `Source/Tools/editor/Source/rsc/translations/*.ts`. The build
only compiles them into `.qm` files; it never changes the `.ts` files.

After adding, changing or removing any `tr()` string (in code or `.ui` files):

1. Update the `.ts` files from the sources:
   ```
   cmake --build build --target update_translations
   ```
2. Fill in the new entries (e.g. with Qt Linguist). New strings show up as `type="unfinished"`.
3. Commit the updated `.ts` files along with your change.

Don't edit the `.ts` files by hand to add or remove strings, and don't reintroduce
`qt5_create_translation`: it re-runs `lupdate` on every build and has wiped existing
translations before.
