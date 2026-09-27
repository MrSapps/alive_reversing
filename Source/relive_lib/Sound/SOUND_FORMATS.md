# Sound data formats: VAB/SEQ -> SF2/SMF

Goal: one set of sound data formats and one sound code path for both games, with audio that
can reach PS1 quality. This file records what the current code uses, whether SoundFont 2 (SF2)
and Standard MIDI Files (SMF) can hold it, and the staged plan.

## What the games ship (PC versions)

| File | AO | AE |
|---|---|---|
| `*.VH` (VAB header) | `VabHeader` + 128 `ProgAtr` + 16 `VagAtr` per program | same |
| `*.VB` (VAB body) | per sample: `u32 length`, `s32 flags`, then the PCM inline | per sample: `u32 length`, `s32 flags`, `u32 offset` into `sounds.dat` |
| `sounds.dat` | - | 16 bit mono PCM for every VB |
| `*.SEQ` (in `*.BSQ`) | Sony SEQ (`pQES`) | same |

The PC ports decoded the PS1 ADPCM (VAG) samples to 16 bit PCM. The samples are played as
44100 Hz, and each tone's `centre`/`shift` sets the pitch. A negative `flags` value means "loop
the whole sample" (`Converted_Vag::field_C` bit 2 -> `DSBPLAY_LOOPING`). The PS1 ADPCM loop
start points are not in the PC data, so a looping sample always loops from its first sample.

## What the engine uses today

Per tone (`VagAtr`, converted to `Converted_Vag` by `SsVabOpenHead`):

- key range (`min`/`max`), `centre` + `shift` (root note + fine tune in 1/128 semitone)
- `vol` (0-127, linear), `priority` (voice stealing, `MIDI_Allocate_Channel`)
- `adsr1`/`adsr2`: only the attack shift/step, decay shift, sustain level and release shift
  bits. They are turned into millisecond timings for a squared-curve envelope that is updated
  every 30 ms (`MIDI_ADSR_Update_4FDCE0`).
- the sample's loop flag (from the VB, not the VH)
- `pan`: read but ignored (only kept with `ORIGINAL_PS1_BEHAVIOR`, and then unused)

Not used: `mode` (reverb on/off per tone), `vibW`/`vibT`, `porW`/`porT`, pitch bend range
(`pbmin`/`pbmax`), attack mode, the whole sustain rate/direction/mode, release mode, `ProgAtr`
(program volume/pan/priority/mode) and the VAB master volume/pan.

Reverb: `SsUtSetReverbType/Depth/On` are stubs. The path's reverb depth is dropped. The mixer
has a generic comb reverb (`Reverb.cpp`, off by default) that it applies to every voice.

SEQ: the PSX SEQ is SMF track data with a 15 byte `pQES` header (resolution, 24 bit tempo,
time signature) instead of `MThd`/`MTrk`. The event stream uses running status, note on/off,
program change, pitch bend and the libsnd loop controllers (NRPN 99 = 20 loop start, 30 loop
end, 40 callback, value in CC 6/38).

## Can SF2 hold it?

| Need | SF2 | Exact? |
|---|---|---|
| 16 bit PCM samples | `smpl` chunk | yes |
| Loop whole sample | `sampleModes` = 1, `shdr` loop = sample start/end | yes |
| PS1 loop start (if ever taken from PS1 data) | `shdr` loop points | yes |
| Several tones per program, key ranges | instrument zones, `keyRange` | yes |
| Root note + fine tune | `overridingRootKey` + `fineTune` (cents) | no: PS1 fine tune is 1/128 semitone (0.78 cent), SF2 is 1 cent |
| Tone volume (linear 0-127) | `initialAttenuation` (0.1 dB) | no: 126 and 127 are 0.07 dB apart, so they round to the same value |
| Tone pan (0-127) | `pan` (0.1 %) | yes (round trip is exact) |
| Reverb on/off per tone (`mode`) | `reverbEffectsSend` | yes (0 or 100 %) |
| Voice priority | - | no equivalent |
| PS1 ADSR | `delayVolEnv` .. `releaseVolEnv` | no, see below |
| Vibrato | `vibLfoToPitch`, `freqVibLFO`, `delayVibLFO` | approximately |
| Portamento | - | no equivalent (unused by the games as far as we know) |
| Pitch bend range | pitch wheel modulator | only symmetric ranges |
| Program / VAB master volume and pan | preset global zone attenuation/pan | approximately |

### ADSR

The PS1 SPU envelope (see "SPU ADSR" in psx-spx) is a per-sample state machine:

- ADSR1: attack mode (linear / pseudo exponential), attack shift + step, decay shift
  (always exponential), sustain level
- ADSR2: sustain mode (linear / exponential), sustain direction (increase / decrease), sustain
  shift + step, release mode (linear / exponential), release shift

SF2 has a linear attack, an exponential (linear in dB) decay and release, and a constant
sustain level. It cannot express a sustain phase that keeps rising or falling while the key is
held, a linear release, or the PS1's exponential attack. So SF2's own envelope can only
approximate the PS1 one: good enough for tools, not for PS1 quality.

### Reverb

SF2 only stores the per-zone send amount. The effect itself belongs to the synth, which is
fine: the PS1 reverb is a fixed SPU algorithm with presets (room, studio S/M/L, hall, space
echo, echo, delay, pipe). The engine would implement it and pick the preset/depth from the
path, as `Path_Get_Reverb` already provides.

## Decision

SF2 and SMF are only the file formats. The engine keeps playing them with its own code, the
emulation of the PS1 sound library and SPU in `PsxSpuApi`/`Midi`, not with a general SF2 player
such as FluidSynth: PS1 quality needs PS1 behaviour (its ADSR, its reverb, libsnd's SEQ quirks),
which only our own code can match. Each game also still has its own SEQ parser, note on and key
off (they differ, see the plan).

SF2 and SMF are good containers. Neither loses sample data, key maps, loops or sequences. SF2's
own generators can't hold everything exactly, so each converted file stores two things:

1. **Standard SF2 generators**, the best SF2 equivalent of every value, so the file plays
   correctly in any SF2 player (Polyphone, FluidSynth) and can be edited there.
2. **The raw PS1 values in private generators**, one per `VagAtr`/`ProgAtr`/`VabHeader` field
   (`raw adsr1`, `raw adsr2`, `priority`, `vol`, `centre`, `shift`, `mode`, etc).
   SF2 2.04 8.1.3 says a reader must ignore generator numbers it doesn't know, so other tools
   still load the file.

The engine reads the raw values when they are there, so conversion is lossless and a later
PS1-accurate envelope has the real registers to work from. When a zone has no raw values (an
SF2 made in another tool, or edited by one that drops unknown generators), the engine derives
them from the standard generators.

SEQ -> SMF type 0 is lossless: `MThd` (division = SEQ resolution), then one `MTrk` that starts
with the header's tempo (FF 51) and time signature (FF 58) at delta 0, then the SEQ event
bytes unchanged. The loader skips those leading meta events so the loop/rewind point is the
same byte the SEQ player uses today.

AE's `sounds.dat` goes away: conversion copies each sample into the SF2, so both games load
the same format and AO/AE's separate `SsVabTransBody` versions disappear.

## Plan

1. Done: **gold traces of the current code** (`relive_lib_tests`, `SoundGold*`). A
   deterministic, offline run of the real sound code: the mixer renders into memory, and the
   sound clock is driven by the number of samples rendered, not wall time. Synthetic VH/VB/SEQ
   data is generated in code for both games. Each scenario records a trace (the loaded tone
   table, a checksum of every sample, then every change to the 24 MIDI channels and 32 voices
   over time) and compares it with the committed gold file in `tests/sound_gold`.
2. Done: **SF2/SMF readers and writers** (`SoundFont`, `VabSoundFont`, `SeqMidi`) with round
   trip unit tests.
3. Done: **the data conversion and the engine use `.sf2`/`.mid`**. `sound_info.json` has
   `sound_bank` (the SF2) instead of `vh_file`/`vb_file`, and `seq_files` are MIDI files
   (`OPTAMB.mid` is the `OPTAMB.SEQ` table entry). The engine loads the SF2, makes the VAB
   header its sound code works on from it, and loads the samples with one `SsVabTransBody` for
   both games (AO's and AE's `sounds.dat` one are gone). The MIDI files become SEQs for the
   SEQ player the same way. The gold traces made before this didn't change.
4. **Real data check** (needs the game files, see below).
5. Later, each a separate, deliberate behaviour change with new gold files:
   - PS1 ADSR from the raw registers, stepped per sample instead of every 30 ms
   - PS1 SPU reverb, per tone `mode` + path reverb depth
   - tone pan, pitch bend range, vibrato
   - merge AO's and AE's MIDI parser / note on / key off. They differ in ways that look like
     reversing or OG bugs, for example:
     - AO's note off indexes `sVagCounts` with `seq_idx << 8`, out of bounds for any VAB id
       above 0
     - AE's tempo meta event handler truncates the tempo to a byte
     - AE doubles the length of short one-shot samples; AO doesn't
     - AO only runs the ADSR for looping samples; AE runs it for all samples
     - the two games use different key off release times (AE: at least 300 ms, AO: 125 ms
       when there is no release)
     - AE's controller handler reads the controller number from the status byte
       (`(cmd >> 8) & 0x7F`, always 0), so AE never sees the loop markers or NRPNs; AO does

     The PS1 libsnd behaviour is the reference for which one is right.

Already fixed, because the gold traces can't pin them:

- AO's loop start stored the address of the read pointer instead of the read position, so a
  loop end jumped into the `MIDI_SeqSong` struct and played its bytes as MIDI
- AO indexed the tone table as `table[0][program + (vabId << 7)]` (out of bounds for UBSan),
  now `table[vabId][program]`, which is the same address

Found, not fixed yet:

- MONK.VH/VB (the loading sound, now MONK.sf2) is never loaded until the sound system has been shut down
  once: `sMonkVh_Vb`'s initialiser leaves `mVabId` at 0 and it is only loaded when it's -1. It
  also has no sound theme yet (see the TODO in `Midi.cpp`).

## Checking against real game data

The repo has no game data, so the real data check is staged for someone who has it (for
example Claude Code running locally). One script does all of it:

```sh
Source/Tools/sound_gold/compare_sound_formats.sh --ae /path/to/AE --ao /path/to/AO
```

It builds `relive_sound_gold` in two git worktrees (under `build-sound-compare/`): at the commit
before "Switch the sound data to SF2 and MIDI files" (plus `before_switch_convert.patch`, which
gives that commit's tool `-convert`) and at `HEAD`. Each converts the game with
the engine's own data conversion (`relive_sound_gold -convert`, headless, paths and sounds only)
into a scratch copy of the game dir, so the game dirs themselves aren't changed. Then it plays
every program's tones and every SEQ of every sound theme through the sound code, offline, and
saves a trace and a WAV of each. The "after" results are compared with the "before" ones:
the script lists every trace or WAV that differs and exits with 1 if any do. Nothing should
differ. If something does, the trace shows where (tone table, sample checksum, or the first
channel/voice change that isn't the same), and the WAVs can be listened to.

`--help` lists the options (`--after <ref>` checks another commit, `--sdl3` if the SDL3 build
isn't the one in `build/CMakeCache.txt`). The two builds are full Debug builds, so the first run
takes a while.

Things worth checking in the real data while at it, which decide open questions above:

- whether any SEQ uses the loop markers (NRPN 99 = 20/30): AE ignores them, AO now loops
- whether any tone's `mode` has bit 2 (reverb) set, and whether that matches the looping
  samples (the PC port uses bit 2 of `Converted_Vag::field_C` for "loop")
- which theme has MONK.VH/VB (see the TODO in `Midi.cpp`)
