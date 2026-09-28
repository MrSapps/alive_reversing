# Sound data formats

Goal: one set of sound data formats and one sound code path for both games, with audio that
can reach PS1 quality. This file records what the current code uses, why SoundFont 2 (SF2) was
tried and rejected, and the staged plan.

## What the games ship (PC versions)

| File | AO | AE |
|---|---|---|
| `*.VH` (VAB header) | `VabHeader` + 128 `ProgAtr` + 16 `VagAtr` per program | same |
| `*.VB` (VAB body) | per sample: `u32 length`, `s32 rate`, then the PCM inline | per sample: `u32 length`, `s32 rate`, `u32 offset` into `sounds.dat` |
| `sounds.dat` | - | 16 bit mono PCM for every VB |
| `*.SEQ` (in `*.BSQ`) | Sony SEQ (`pQES`) | same |

The PC samples are NOT decoded PS1 ADPCM: only 1.8% of AE's and 0.3% of AO's 28 sample blocks
fit the ADPCM model, while a real decode fits 100%. They are the source audio from before the
PS1 encoding (see "PC vs PS1 data" below).

A VB record's `length` is in bytes. Its second field (`VabBodyRecord::field_4_unused`) is not
flags but the sample's source rate (`s32 rate`: 5512, 8000, 11025, 22050, 44100, ...). A
negative rate means "loop the whole sample" (`Converted_Vag::field_C` bit 2 ->
`DSBPLAY_LOOPING`). The engine plays every sample as 44100 Hz and sets the pitch from the tone's
`centre`/`shift`, like the PS1, where the rate is only authoring data. The PS1 ADPCM loop start
points are not in the PC data, so a looping sample always loops from its first sample, which is
what the PS1 data does too (see below).

## PC vs PS1 data

Compared (locally, with the game files): the EU PS1 discs against the GOG PC data.

- Samples: about 98% (AO) and 99.8% (AE) of the PS1 VAGs are the PC sample, ADPCM encoded. The
  correlation is above 0.99 and the difference is 17-41 dB below the signal (median about
  21-23 dB). Quantising the PC PCM with each PS1 block's own filter/shift reproduces about 99% of
  the PS1 nibbles, so the PC PCM is exactly what the PS1 encoder was given: the PC samples are
  the cleaner copy. Each PS1 VAG starts with one 28 sample zero block.
- AO: the PS1 stored some samples at a lower rate (for example 8000 vs 22050 Hz) with a higher
  centre note. The PC kept the full rate and lowered the centre by the same amount, so the pitch
  matches.
- AE vag 8 is used by the sound effects SecurityOrb (program 0 note 63) and PortalOpening
  (program 10 note 36). It has 2x the samples on PC with the same centre note, so the PC
  probably plays it an octave lower than the PS1. Not yet confirmed by ear.
- Unclear or different content: about 24 AO samples (mostly RFENDER), and AE PARVAULT vags
  106-110.
- Loops: every PS1 loop starts at block 1 (just after the zero block) and ends at the sample
  end. That equals the PC's "loop the whole sample", so no loop data is missing.
- VH: the tone parameters (centre, shift, vol, ADSR, keys, ...) are identical wherever a tone
  exists in both versions. The PC AE VHs add 3 programs and 5 tones. The PS1 VH ends with a 512
  byte VAG size table (256 x u16, size = value * 8), which the PC VH drops. AO's `ProgAtr`
  differs only in unused bytes.
- BSQ/SEQ: nearly all identical. AE's PC version replaced 3-6 short jingles per level with
  single notes on the PC only programs. Each AO PS1 BSQ has one extra SEQ.

Conclusion: the poor PC sound comes from the playback code, not the data. The ADSR is updated
every 30 ms from only some of the register bits, there's no SPU reverb, pan and pitch bend range
are ignored, SDL resamples instead of the SPU's interpolation, and AE doubles the read length of
short one shot samples. The fix is a PS1 accurate playback engine that plays the PC PCM as it
is: an emulated SPU (below), then a libsnd style layer on top of it.

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

## Decision: not SF2

SF2 was implemented and then dropped. Because its own generators can't hold the PS1 values
exactly (see above), each tone had to store them twice: the nearest standard SF2 generators, so
other SF2 players could use the file, plus the raw PS1 values in private generator numbers,
which the engine read instead. That gives two copies of every value that can disagree. Editing a
tone in an SF2 tool (Polyphone etc.) changed the standard generator, which the engine ignored, so
the edit silently did nothing. A tool that drops unknown generators silently changed the sound
instead. The engine also only used the SF2 as a container: it rebuilt a VH from the private
values. No other sampler format (SFZ, DLS, XI/ITI, ...) can hold the PS1 ADSR, voice priority or
1/128 semitone tuning either, so all of them would have the same problem.

The bank format still has to be chosen. It must hold the PS1 values exactly, with one copy of each
(for example the `VabHeader`/`ProgAtr`/`VagAtr` fields as JSON with the samples as WAVs, or VAB
itself with AE's `sounds.dat` samples moved into the VB). An SF2/SFZ export for listening in other
tools could come later, one way only.

SEQ -> SMF type 0 would be lossless and needs nothing private: the PSX SEQ is SMF track data with
a different header, and the libsnd loop markers are ordinary controller events.

## Plan

1. **Gold traces of the current code** (`relive_lib_tests`, `SoundGold*`). A deterministic,
   offline run of the real sound code: the mixer renders into memory, and the sound clock is
   driven by the number of samples rendered, not wall time. Synthetic VH/VB/SEQ data is
   generated in code for both games. Each scenario writes a trace (the loaded tone table, a
   checksum of every sample, then every change to the 24 MIDI channels and 32 voices over time)
   and a WAV, and compares the trace with the committed gold file.
2. **Pick the bank format** (see the decision above), and write its reader/writer with round
   trip unit tests.
3. **Switch data conversion and the engine** to it, drop `sounds.dat`/VB loading and AO's own
   `SsVabTransBody`. The gold traces must not change.
4. **Real data check** (needs the game files, see below).
5. Later, each a separate, deliberate behaviour change with new gold files:
   - PS1 ADSR from the raw registers, stepped per sample instead of every 30 ms (in the
     emulated SPU, not wired in yet: see below)
   - PS1 SPU reverb (in the emulated SPU), per tone `mode` + path reverb depth
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

- MONK.VH/VB (the loading sound) is never loaded until the sound system has been shut down
  once: `sMonkVh_Vb`'s initialiser leaves `mVabId` at 0 and it is only loaded when it's -1. It
  also has no sound theme yet (see the TODO in `Midi.cpp`).

## The emulated SPU (`Sound/Spu`)

`PsxSpu` is a PS1 SPU implemented from psx-spx ("Sound Processing Unit (SPU)"), in its own
small library (`psx_spu`, no dependencies, `cmake --build build --target psx_spu`). It isn't
used by the game yet: nothing calls it outside its unit tests (`tests/PsxSpuTests.cpp`), so the
sound gold traces are unchanged.

- 24 voices, 44100 Hz stereo, deterministic integer arithmetic. `Render(out, frames)` mixes
  into an interleaved `s16` buffer; it never allocates or locks, so it can run in the audio
  callback. The SPU isn't thread safe: register writes and `Render` must be on the same thread
  or serialised by the owner. Output rendered in any block size is identical.
- Voices play 16 bit PCM (`PsxSpuSample`: pointer, length, loop flag, loop start/end in
  samples) instead of decoding ADPCM. The sample is picked up at key on, like `SSA`. A one shot
  sample's end mutes the voice ("End+Mute"), a loop end jumps to the loop start
  ("End+Repeat"); both set the voice's `ENDX` bit, which key on clears.
- Pitch: the `PITCH` register (0x1000 = 44100 Hz, values above 0x3FFF play at 0x4000) drives a
  pitch counter; bits 4-11 index the 512 entry "gaussian" table for the 4 point interpolation.
  `PitchFromNote(note, fine, centre, shift)` and `PitchFromSampleRate(rate)` are the helpers
  for the libsnd layer.
- ADSR (`PsxSpuEnvelope`) stepped every sample from the raw ADSR1/ADSR2 registers, with the
  psx-spx step/shift/counter rules: linear or exponential attack (the exponential one slows down
  above 0x6000), exponential decay to (N + 1) * 0x800, sustain with its own mode, direction and
  rate until key off, linear or exponential release. A rate with all bits set never steps
  (psx-spx says this also holds for a release shift of 0x1F: check that against hardware if a
  voice ever hangs).
- Voice and master volume registers in fixed or sweep mode (the sweep uses the envelope step).
- Reverb (`PsxSpuReverb`): the psx-spx formula at 22050 Hz on a 16 bit work area, with the SPU's
  39 tap resampling filter on the way in and out. It includes the standard presets in libsnd order
  (off, room, studio small/medium/large, hall, space echo, echo, delay, pipe), a per voice
  reverb enable (`EON`), the reverb master enable (`ATTR` bit 7: stops the writes, the reads
  continue) and the output volume (`EVOL`, libsnd's depth; `DepthToVolume` maps 0-127).
  Intermediate values saturate to 16 bits, as psx-spx measured. Left and right are processed on
  the same 22050 Hz tick (hardware alternates them, a 1-2 LSB difference). The `vIIR = -0x8000`
  negation bug isn't emulated (no preset uses it).
- Mixing: voices are summed with the reverb output, clamped to 16 bits, then scaled by the
  master volume.

Not emulated: ADPCM, noise, pitch modulation, CD/external input, IRQs and capture buffers.

`Reverb.cpp` (the generic comb reverb the current mixer uses) is untouched. It stays until the
current mixer is replaced, because changing it would change the current sound.

### Still to do

- The libsnd layer: SEQ player on top of the SPU, VAB tone mapping (key ranges, centre/shift,
  vol/pan, program and VAB master volume/pan), note -> pitch with the pitch bend range,
  vibrato, voice allocation by priority (with `IsVoiceActive`/`Endx`), reverb type/depth from
  `SsUtSetReverbType`/`SsUtSetReverbDepth` and the path, and the tone `mode` reverb bit.
- Driving the SPU from the audio callback (a command queue from the game thread), behind an
  option, with new gold traces for the PS1 sound. The current gold traces stay for the current
  path.
- Check against real hardware or a PS1 recording: the envelope edge cases, the release shift
  0x1F case, the order of the reverb and master volume in the mix, and AE vag 8's pitch.

## Checking against real game data

The repo has no game data, so the real-data check is staged for someone who has it (for
example Claude Code running locally). `relive_sound_gold` renders every VAB tone and every SEQ
of every sound theme in a converted data dir to traces and WAVs:

```sh
# 1. On the commit before the format switch: convert the data and render the gold set
cd /path/to/AE && /path/to/build/Source/relive/relive -convert
build/Source/Tools/sound_gold/relive_sound_gold -data=/path/to/AE -out=sound_gold_before

# 2. On the commit after it: convert again (the data version changed), then compare
cd /path/to/AE && /path/to/build/Source/relive/relive -convert   # reconverts
build/Source/Tools/sound_gold/relive_sound_gold -data=/path/to/AE -out=sound_gold_after -baseline=sound_gold_before
```

Do the same for AO with `-AO`. `-baseline` lists every trace or WAV that differs. With the
format switch alone nothing should differ.
