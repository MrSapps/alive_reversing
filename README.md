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
| `-ddfps` | Show the frame rate. |
| `-ddnoskip` | Render every frame instead of skipping frames to keep up. |
| `-ddslowload=<ms>` | Make each resource loaded in the background take at least this many milliseconds, to test loading on slow storage. |
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
| `-fastest` | With `-play`, run as fast as possible instead of at normal speed. |
| `-ignore_desyncs` | With `-play`, keep going after a desync instead of stopping (it logs one warning). |

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
