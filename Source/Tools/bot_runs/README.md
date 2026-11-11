# Bot runs

Scripts that let the bot (`Source/relive_lib/Bot`, see its `BOT_DESIGN.md`) play, for watching or for a
regression run. They are `-automation` scripts: run them from the game's data directory.

```sh
# Watch it (a window, normal speed)
cd /path/to/AE && relive -automation=/path/to/Source/Tools/bot_runs/ae_mines.txt
cd /path/to/AO && relive -AO -automation=/path/to/Source/Tools/bot_runs/ao_rupture_farms.txt

# Headless and fast: add "draw 0" after the start line of a copy of the script, and
SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy relive -automation=<script> -max_fps=0
```

Change the profile on the `bot` line: `save_all`, `kill_all`, `average` or `any`, then `,seed=<n>` for a
different run of the same profile and `,human` for a bot that dawdles and varies like a person. The same
profile and seed always give the same run.

When the bot stops, `bot_report` writes what it was doing, what it saw and why it stopped (`done`, or
`STUCK: no_plan / action_failed ...`), and `bot_inputs` the buttons of the run, which the `replay` command
plays back without the bot.

## What to expect (2026-11-11)

`ae_mines.txt` with `any` gets through the first path of the Mines (about 12,000 game frames: a few minutes
at normal speed, seconds headless) and stops at the start of the second with `STUCK: no_plan`. `kill_all`
slaps to death the Mudokons it can reach whose loss doesn't close the way out, and says in its log which it
left and why. `ao_rupture_farms.txt` is stuck near its start. See "How far the levels get" in
`Source/relive_lib/Bot/BOT_DESIGN.md`.

`bot_log <file>` (before the `bot` line) writes the bot's decisions when the script ends: each goal and plan,
each Mudokon talked to, what the Sligs on the screen do, each death and where it went back to.
