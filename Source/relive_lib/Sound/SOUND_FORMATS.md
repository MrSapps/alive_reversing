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
- VH: AE's tone parameters (centre, shift, vol, ADSR, keys, ...) are identical wherever a tone
  exists in both versions. AO's are not: about 100 tones differ. Some have a lower priority and
  volume on the PC (the secret area jingle, R1 program 81: 127/127 on the PS1, 70/70 on the PC,
  so with the PC values an electric wall's buzz takes its voices). `-ps1_sound` puts the PS1
  priority and volume back (`PsxSoundEngine::RestorePs1Tones`). Others have a different centre
  and shift, which are kept: the samples played are the PC's. The PC AE VHs add 3 programs and
  5 tones. The PS1 VH ends with a 512
  byte VAG size table (256 x u16, size = value * 8), which the PC VH drops. AO's `ProgAtr`
  differs only in unused bytes.
- BSQ/SEQ: nearly all identical. AE's PC version replaced 3-6 short jingles per level with
  single notes on the PC only programs. Each AO PS1 BSQ has one extra SEQ.

Conclusion: the poor PC sound comes from the playback code, not the data. The PC code also got
the tone's `shift` backwards (see "Tone pitch" below), so 44% of AE's tones and 36% of AO's played
up to 1.9 semitones flat. Both paths now have it the right way round. The ADSR is updated
every 30 ms from only some of the register bits, there's no SPU reverb, pan and pitch bend range
are ignored, SDL resamples instead of the SPU's interpolation, and AE doubles the read length of
short one shot samples. The fix is a PS1 accurate playback engine that plays the PC PCM as it
is: an emulated SPU (below), then a libsnd style layer on top of it.

## What the engine uses today

Per tone (`VagAtr`, converted to `Converted_Vag` by `SsVabOpenHead`):

- key range (`min`/`max`), `centre` + `shift` (root note + fine tune in 1/128 semitone). The PC
  code subtracted `shift` like a correction to the root note; libsnd adds it, and so does the
  engine now (see "Tone pitch")
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

## Tone pitch

libsnd's `SsPitchFromNote` (AE PS1 executable `SLES_014.80`, 0x80076620; the SEQ note on at
0x80076518 does the same with the tone's `centre`/`shift`):

```c
s = fine + shift;  fine16 = s / 8;  carry = 0;
if (fine16 >= 16) { carry = 1; fine16 -= 16; }
n = note - (centre - 60) + carry;
pitch = ptable[(n % 12) * 16 + fine16];      // 0x8009F090: floor(4096 * 2^(i / 192))
octave = n / 12 - 5;
pitch = octave > 0 ? pitch << octave : pitch >> -octave;
```

So `shift` is ADDED to the note's fine tune: it raises the pitch by shift/128 semitones. The PC
code (`field_A_shift_cen = 2 * (shift + (centre << 7))`, then `note - field_A_shift_cen`)
subtracts it, so every tone with a shift plays 2 * shift / 128 semitones flat on PC. The pitch is
also rounded down to 1/16 semitone. 1510 of AE's 3418 tones and 802 of AO's 2259 have a shift, in
every level's VH (mostly 57 and 70, up to 122). Example: POSITIV9.SEQ plays AE program 27 (centre
90, shift 70), 140/128 = 1.09 semitones flat on PC, which a listener spotted.

`PsxSpu::PitchFromNote` is this function, so `-ps1_sound` plays them at the PS1 pitch, rounded to
1/16 semitone like libsnd. The default (SDL voices) path now adds the shift too
(`field_A_shift_cen = (centre << 8) - 2 * shift`), without the rounding. This is a deliberate change
from the PC release, and it changed the sound gold traces.

The other libsnd pitch paths agree:

- `SsUtChangePitch` (0x80074384, SFX pitch variation) and `SsUtKeyOn` (0x8007480c) call the same
  function as the SEQ note on, with their own note and fine tune.
- The SEQ note on (0x80076444, from the voice allocator at 0x80075998) takes the tone's shift as the
  fine tune, rounded down to 1/16 semitone and capped at 15 steps (no carry). The same as above
  for the shifts the games use (0-127).
- Pitch bend: the SEQ handler `_SsSndPitchBend` (0x80072668) reads ONE data byte, the MIDI bend's
  high 7 bits (the dispatcher already read the low 7, which libsnd ignores), and bends every voice
  of the SEQ's VAB and program through `SsUtPitchBend` (0x80077264):

  ```c
  d = pbend - 64;
  if (d > 0)      { x = d * tone.pbmax; note += x / 63;     fine = (x % 63) * 2; }
  else if (d < 0) { x = d * tone.pbmin; note += x / 64 - 1; fine = (x % 64) * 2 + 127; }
  else            { fine = 0; }
  pitch = <SEQ note on pitch>(note, fine);   // adds the tone's shift
  ```

  `PsxSoundEngine::PitchBend` is this. A later note on isn't bent. The default path still uses
  the PC's bends, which decode the event differently in each game.

Checking the sign from the AO tones the PS1 re-tuned (lower sample rate, higher centre) was
inconclusive: the PS1 sample lengths only give the rate ratio to about half a semitone.

## SEQ note on and off

libsnd (AE PS1 executable):

- Note on: always a new voice (the voice allocator at 0x800761b4 only looks at voice age and
  priority), so a note struck again keeps sounding.
- Voice allocation (AO PS1 executable 0x8007bd30, `Libsnd::Alloc`): a voice is
  free when its key is released and its envelope is 0. The envelope is read back from the SPU
  every tick (0x8007abe4, before the tick plays the SEQs), and key on sets it to 0x7FFF until then. A
  voice whose envelope has been 0 in history slots 0-14 (of a 16 slot ring, so 15 or 16 ticks)
  counts as released (a one shot that ended without a key off). With no free
  voice, it steals among the voices whose priority is at most the new tone's: the lowest priority,
  then the lowest envelope, then the oldest (counted in allocations). If none qualifies, the note
  is dropped. The PC's `MIDI_Allocate_Channel` instead takes the quietest channel whatever its
  priority, and gives up if that one's priority is higher.
- Note off (0x80075f18, also a note on with velocity 0): keys off EVERY voice that the same SEQ
  keyed on with the same VAB, program and note. No reference counts; the SEQ's MIDI channel isn't
  compared, only the SEQ; other SEQs' and sound effects' voices aren't touched.
- SEQ stop: keys off the SEQ's voices.

The PC code differs, and `-ps1_sound` follows libsnd instead (`Libsnd::VmKeyOff`); the default
path keeps the PC behaviour:

- AE: reference counts per MIDI channel, so overlapping identical notes sustain until the last note
  off. Its handler for event 0x80 keys off channel `v31` (always 0) instead of the matching one;
  most SEQs use velocity 0 note ons, which don't go through it.
- AO: a note on first keys off the same note (any SEQ's), the note off only releases the first
  match of any SEQ, and the 0x80 handler compares against a mangled VAB id, so it never matches.
  AO also never recorded which SEQ owned a channel, so SsSeqStop didn't release a stopped SEQ's
  notes (only a stop of SEQ 0 matched, by accident): a looping note hung when the game stopped the
  music, e.g. the secret area jingle when Abe dies. Fixed for both paths.

The `stop_mid_note` and `note_off_rules` sound gold scenarios pin this.

## Voice volume

libsnd's voice setup (0x80076d94 in `SLES_014.80`, `Libsnd::KeyOnNow`), checked by running it in
a MIPS interpreter, in VOLL/VOLR register units (0-0x3FFF), all divisions rounding down:

```c
v = velocity * vabVol * 0x3FFF / (127 * 127);
v = v * progVol * toneVol / (127 * 127);
L = R = v;
if (seqNote) { L = v * seqVolL / 127; R = v * seqVolR / 127; }
for (pan : tonePan, progPan, channelPan)   // the VAB's master pan isn't used
    if (pan < 64) R = R * pan / 63; else L = L * (127 - pan) / 63;
if (seqNote) { L = L * L / 0x3FFF; R = R * R / 0x3FFF; }   // SEQ notes are SQUARED
```

- A SEQ note's velocity is first scaled by its channel volume (CC 7, default 127), and its channel
  pan is CC 10 (default 64). AE's `SsSeqSetVol` (0x80073ef4) works out the SEQ's playing notes'
  volumes again only while the SEQ plays (its flags are exactly "playing"), with each note's own
  channel volume; otherwise it stores the volume, unclamped, for the notes to come. Its
  `SpuVmSetSeqVol` (0x800776c4) always changes the playing notes, so `SsSeqPlay` and
  `SsSeqClose` do too.
- A sound effect (`SsVoKeyOn`, 0x80076080): velocity = the larger of its left/right volume, and
  channel pan = 64 if they're equal, else `volR * 64 / volL` (right quieter) or
  `127 - volL * 64 / volR`. No square.
- The PC code used linear volumes for both, a fixed SFX velocity of 96, and a SEQ volume of
  `112 * vol >> 7` with channel volumes of 112. `-ps1_sound` uses libsnd's.

AO's libsnd is an older version, and differs (AO PS1 executable `SLES_006.64`):

- Its voice setup (0x8007c73c, called by both the SEQ note on and `SsUtKeyOnV`) squares EVERY
  voice's volume, sound effects too: AE's skips the square for a sound effect (voice marked
  `0x21`). Checked in the MIPS interpreter: the formula above with the square always on gave the
  same result for all of 3000 random SEQ notes and sound effects. Sound effects played up to 6 dB
  too loud against the music with AE's rule (velocity 66: 8514 instead of 4424).
- Its `SND_Init` and `SND_Reset` set the master volume to 127 (`SsSetMVol`, 0x800784d4), AE's to
  100, as the PC code does for both. With 100 everything was 2.1 dB quiet. The SDL voices keep
  100, because the PC code also scales their volume by it.
- `SsSeqSetVol` (0x80078ee4) always works out the playing notes' volumes again (`SpuVmSetSeqVol`
  0x8007cf10), with the volume of the channel the SEQ last read an event on, whichever channel
  played the note, and takes a volume of 0 as 1. Its `_SsNoteOn` skips notes at SEQ volume 0, AE's
  doesn't.
- The game clamps SEQ volumes to 10-127 in both.

`Libsnd::Config` (set in `PsxSoundEngine.cpp`) holds these per game, and
`IPsxSpuApiVars::DefaultMasterVolume` the master volume.

`SsUtAllKeyOff` (AE 0x80074244, AO 0x80079160) doesn't only key the voices off: it first sets each
voice's volume to 0, pitch to 1000h and ADSR to 80FFh/4000h, so they go silent at once. With a plain
key off, the tones with a release shift of 29 or 30 (20% of AO's tones, 48% of AE's) take
minutes to fade, so looping ones kept playing over the FMVs (`SND_StopAll` is the only caller).
`Libsnd::UtAllKeyOff` does the same.

Found while chasing a buzz in POSITIV9's last notes, which play a noisy looped sample at 3.89x
(pitch `3e2c`) at velocity 66: squared, they sit 10 dB below the rest of the jingle instead of 6 dB.
Not yet confirmed that this is all of the difference from the PS1.

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
     emulated SPU, behind `-ps1_sound`: see below)
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

- Both games turned every SEQ event's delta into whole milliseconds on its own (`ticks * tempo /
  1000`), dropping the remainder each time, so SEQs played up to 2% fast: POSITIV9's last note came
  46 ms early, confirmed against DuckStation running libsnd. `MIDI_AddDeltaTime` now carries the
  remainder (in `MIDI_SeqSong::mTimeRemainderUs`, which was padding). This changed the gold traces.

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
small library (`psx_spu`, no dependencies, `cmake --build build --target psx_spu`), unit tested by
`tests/PsxSpuTests.cpp`. The game only uses it with `-ps1_sound` (below), so without it the
sound gold traces are unchanged.

- 24 voices, 44100 Hz stereo, deterministic integer arithmetic. `Render(out, frames)` mixes
  into an interleaved `s16` buffer; it never allocates or locks, so it can run in the audio
  callback. The SPU isn't thread safe: register writes and `Render` must be on the same thread
  or serialised by the owner. Output rendered in any block size is identical.
- Voices play 16 bit PCM (`PsxSpuSample`: pointer, length, loop flag, loop start/end in
  samples) instead of decoding ADPCM. The sample is picked up at key on, like `SSA`. A one shot
  sample's end mutes the voice ("End+Mute"), a loop end jumps to the loop start
  ("End+Repeat"); both set the voice's `ENDX` bit, which key on clears.
- Pitch: the `PITCH` register (0x1000 = 44100 Hz) drives a pitch counter; bits 4-11 index the 512
  entry "gaussian" table for the 4 point interpolation. Not the hardware: pitches go up to
  `kMaxPitch` = 0x10000 (16x), where the hardware plays anything above 0x3FFF at 0x4000. The PC
  data has some samples at a higher rate than the PS1's (with the tone's centre lowered to match),
  so their notes need more: AO's secret area jingle (RFSNDFX vag 43) is 2.75x the PS1 rate, and
  with the hardware limit its high notes played up to 1.4 octaves low.
  `PitchFromNote(note, fine, centre, shift)` and `PitchFromSampleRate(rate)` are the helpers
  for the libsnd layer.
- ADSR (`PsxSpuEnvelope`) stepped every sample from the raw ADSR1/ADSR2 registers, with the
  psx-spx step/shift/counter rules: linear or exponential attack (the exponential one slows down
  above 0x6000), exponential decay to (N + 1) * 0x800, sustain with its own mode, direction and
  rate until key off, linear or exponential release. A rate with all bits set never steps
  (psx-spx says this also holds for a release shift of 0x1F: check that against hardware if a
  voice ever hangs).
- Voice and master volume registers in fixed or sweep mode (the sweep uses the envelope step).
- `KON`/`KOFF` (`KeyOnMask`/`KeyOffMask`) take effect after the next sample, a voice's key off
  before its key on, as DuckStation does it from hardware tests.
- Reverb (`PsxSpuReverb`): the psx-spx formula at 22050 Hz on a 16 bit work area, with the SPU's
  39 tap resampling filter on the way in and out. It includes the standard presets in libsnd order
  (off, room, studio small/medium/large, hall, space echo, echo, delay, pipe), a per voice
  reverb enable (`EON`), the reverb master enable (`ATTR` bit 7: stops the writes, the reads
  continue) and the output volume (`EVOL`, libsnd's depth; `DepthToVolume` maps 0-127), applied
  at 44100 Hz so a change is immediate. Each step rounds and saturates where the hardware does
  (as Mednafen and DuckStation measured it: the comb sum isn't saturated, the two all pass
  filters share one saturation), which matters for loud input. It runs on every second sample
  after an SPU reset; `Clear` (`SpuClearReverbWorkArea`) only zeroes the work area. libsnd's echo
  and delay types (7, 8) work their registers out from a delay and feedback the games never set;
  only type 4 (studio large) is used.
- Mixing: voices are summed with the reverb output, clamped to 16 bits, then scaled by the
  master volume.

Not emulated: ADPCM, noise, pitch modulation, CD/external input, IRQs and capture buffers.

### Interpolation: `-spu_filter`

The SPU's 4 point gaussian only filters well up to 44100 Hz. A voice played faster (PITCH above
1000h) skips source samples, so the sample's high frequencies fold back into the audible range.
On bright samples at high notes that's heard as a buzz: POSITIV9 (the secret area jingle) ends on
AE MINES.VH program 27 (a noisy 8 kHz loop) at 3E2Ch (3.89x), where 4% of the output lands below
300 Hz. DuckStation (running libsnd in the seq player) does the same, so it's what the hardware
does.

`PsxSpu::Interpolation::BandLimited` (the game's default, `-spu_filter=hq`) keeps the gaussian at
and below 1000h, and above it uses a windowed sinc (Blackman, 16 output samples each side, cutoff
at 90% of the output Nyquist) stretched by the pitch ratio, which filters out what would alias. It
is evaluated at the same position as the gaussian (mHistory[1] plus the counter fraction), reading
ahead without moving the voice on, so ENDX, one shot ends and loops are unchanged and a voice bent
across 1000h doesn't jump. `-spu_filter=gaussian` gives the exact hardware behaviour.

Its cost grows with the pitch ratio (16 x ratio taps each side), and the jingle's notes at up to
10.7x made the audio callback miss its deadline. So samples played above 2x get low-passed copies
for 2x, 4x and 8x (`PsxSpu::FilterSample`, `PsxSpuSample::mFiltered`), kept at full length so the
loops stay exact, and a voice reads the matching copy at a stride of 2^k: at most ~64 taps at any
pitch. `PsxSoundEngine` makes them on the game thread, outside the audio lock, for the notes a SEQ
plays when it's opened, and for a sound effect the first time it's played fast.

`Reverb.cpp` (the generic comb reverb the current mixer uses) is untouched. It stays until the
current mixer is replaced, because changing it would change the current sound.

### Wired in: `-ps1_sound`

Both games' PS1 sound is one implementation: `Libsnd`, a port of the PS1 sound library (libsnd's
SEQ player and voice manager), drives the SPU, and `PsxSoundEngine` holds the two with the
samples. `SDLSoundSystem` owns it (only with `relive -ps1_sound`) and adds its output to the SDL3
stream after the old mixer, so FMV audio still goes through the SDL voices.

- The game's libsnd calls (`SsVoKeyOn_4FCF10`, `SsSeqOpen_4FD6D0`, `SsSeqPlay_4FD900`,
  `SsUtKeyOffV_4FE010`, ...) go straight to it; the PC's MIDI channel code doesn't run.
- `Libsnd` was ported from the MIT licensed libsnd decompilation in sotn-decomp
  (`src/main/psxsdk/libsnd`), then corrected where the games' own libsnd differs (see `Voice volume`
  and `Libsnd::Config`): AO's and AE's versions differ in squaring sound effects, `SsSeqSetVol`,
  and `SpuVmSetVol`'s pan lookups (the decompilation's reads the wrong table entries; AE's is
  right, AO's SEQs have no controllers).
- Like the PS1, where the vsync interrupt ran libsnd's tick, it ticks 50 times a second of audio
  (PAL) on the audio thread, between the SPU's samples (`PsxSoundEngine::Mix`). A tick writes the
  registers the calls changed since the last one (so a sound effect starts at the next tick, up
  to 20 ms later, as on the PS1), reads every voice's envelope for the voice allocation, and plays
  the SEQs. A mutex serialises it with the game thread's calls.
- Samples are named by VAB and VAG number instead of SPU addresses, and libsnd's pitch isn't cut
  to 16 bits (see the SPU's pitch). Not used by the games, so not ported: SEP blocks, noise
  voices, vibrato/portamento, crescendo/tempo changes, and the controllers other than bank,
  volume, pan, the loop NRPNs and RPNs.
- One deliberate difference: `SsVoKeyOn` returns 0, not -1, when there's no voice, because the
  games take the result as a voice mask and would stop or re-pitch every voice with -1.

The `ps1_*` gold traces (`SoundGold.PS1_*`) pin it, with each SPU voice's phase, pitch, volume,
ADSR, reverb bit and what libsnd plays on it; `PsxSoundEngineTests.cpp` tests `Libsnd` on its own.

### Checked against the real PS1 code

`build-ps1re/ref` (local only: it uses DuckStation's SPU, which is CC-BY-NC-ND, and needs the PS1
executables) runs each game's real libsnd out of its PS1 executable in a MIPS interpreter, on
DuckStation's SPU. `RELIVE_SOUND_RECORD=<file> relive -ps1_sound` records the game's libsnd
calls with the sound clock; `ps1snd [-ae] cmp <file>` replays them on the real libsnd and on
`Libsnd` (on the same SPU, with the PS1 VH/VB/SEQ data) and diffs every SPU register write, and
with `CMP_AUDIO=<prefix>` also renders `Libsnd` on relive's `PsxSpu` (the PS1 samples, ADPCM
decoded) and compares the audio with DuckStation's. `relive_sound_gold -replay=<file>` plays a
recording on the engine itself.

Results (October 2026): every register write identical, and every sound effect got the same
voices, for an AO recording (Rupture Farms' secret area: 567 sound effects, the jingle, voice
stealing) and three AE ones (Mines, Barracks, Bonewerkz), plus synthetic ones for the SEQ volume
and channel volume paths (AE's Scrab Vault SEQs, the only ones that use CC 7). The audio of
relive's SPU differs from DuckStation's by 48-62 dB under the signal (at most 122 of 32767): the
envelopes, interpolation, reverb and key on timing match.

A real PS1 recording of the secret area (DuckStation, `build-ps1re/rec`) confirms the jingle's
pitch and tempo.

### Still to do

- Loudness: with the real AE data a few loud SEQs (e.g. MI_6_1, NEGATIV3) hit the SPU's 16 bit
  clamp before the master volume, up to 0.1% of samples. Check against a PS1 recording before
  scaling anything.
- The PC samples are brighter than the PS1's (no ADPCM, and AO's PS1 data had some at lower rates),
  so the same notes sound clearer and a little louder (1-2 dB on the jingle): the PS1's gaussian
  interpolation dulls a low rate sample.
- Game logic: the PS1 games run at 25/50 Hz on PAL, relive at 30 fps, so how often the game
  starts sound effects differs.

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
