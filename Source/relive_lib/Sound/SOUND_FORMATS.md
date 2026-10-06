# Sound data formats and the sound engine

The games' sound runs on one engine for both: an emulated PS1 SPU (`Sound/Spu`), driven by a port
of the PS1 sound library (`Libsnd`), playing the PC versions' samples. This file records the data
formats the games ship, what is known about the differences between the PC and PS1 data, and how
the engine was checked against the real PS1 code.

## What the games ship (PC versions)

| File | AO | AE |
|---|---|---|
| `*.VH` (VAB header) | `VabHeader` + 128 `ProgAtr` + 16 `VagAtr` per program | same |
| `*.VB` (VAB body) | per sample: `u32 length`, `s32 rate`, then the PCM inline | per sample: `u32 length`, `s32 rate`, `u32 offset` into `sounds.dat` |
| `sounds.dat` | - | 16 bit mono PCM for every VB |
| `*.SEQ` (in `*.BSQ`) | Sony SEQ (`pQES`) | same |

(`Sound/VabFile.hpp` has the `VabHeader`, `ProgAtr`, `VagAtr` and `VabBodyRecord` layouts, and
`GameSound::LoadVab` reads the bodies.)

The PC samples are NOT decoded PS1 ADPCM: only 1.8% of AE's and 0.3% of AO's 28 sample blocks
fit the ADPCM model, while a real decode fits 100%. They are the source audio from before the
PS1 encoding (see "PC vs PS1 data" below).

A VB record's `length` is in bytes. Its second field (`VabBodyRecord::field_4_unused`) is not
flags but the sample's source rate (`s32 rate`: 5512, 8000, 11025, 22050, 44100, ...). A
negative rate means "loop the whole sample". The engine plays every sample as 44100 Hz and sets
the pitch from the tone's `centre`/`shift`, like the PS1, where the rate is only authoring data.
The PS1 ADPCM loop start points are not in the PC data, so a looping sample always loops from its
first sample, which is what the PS1 data does too (see below).

SEQ: the PSX SEQ is SMF track data with a 15 byte `pQES` header (resolution, 24 bit tempo,
time signature) instead of `MThd`/`MTrk`. The event stream uses running status, note on/off,
program change, pitch bend and the libsnd loop controllers (NRPN 99 = 20 loop start, 30 loop
end, 40 callback, value in CC 6/38).

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
  so with the PC values an electric wall's buzz takes its voices). `PsxSoundEngine::RestorePs1Tones`
  puts the PS1 priority and volume back. Others have a different centre
  and shift, which are kept: the samples played are the PC's. The PC AE VHs add 3 programs and
  5 tones. The PS1 VH ends with a 512
  byte VAG size table (256 x u16, size = value * 8), which the PC VH drops. AO's `ProgAtr`
  differs only in unused bytes.
- BSQ/SEQ: nearly all identical. AE's PC version replaced 3-6 short jingles per level with
  single notes on the PC only programs. Each AO PS1 BSQ has one extra SEQ.

Conclusion: the PC data is fine, the poor sound of the PC versions was their playback code. This
engine plays the PC PCM as it is, on an emulated SPU (below) driven by a port of libsnd.

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

`PsxSpu::PitchFromNote` is this function, so tones play at the PS1 pitch, rounded to 1/16 semitone
like libsnd. (The PC releases subtracted `shift`, so every tone with a shift played 2 * shift / 128
semitones flat: 44% of AE's tones and 36% of AO's, up to 1.9 semitones.)

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

  `PsxSoundEngine::PitchBend` is this. A later note on isn't bent.

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
  is dropped.
- Note off (0x80075f18, also a note on with velocity 0): keys off EVERY voice that the same SEQ
  keyed on with the same VAB, program and note. No reference counts; the SEQ's MIDI channel isn't
  compared, only the SEQ; other SEQs' and sound effects' voices aren't touched.
- SEQ stop: keys off the SEQ's voices.

`Libsnd::VmKeyOff` is this. The `stop_mid_note` and `note_off_rules` sound gold scenarios pin it.

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

AO's libsnd is an older version, and differs (AO PS1 executable `SLES_006.64`):

- Its voice setup (0x8007c73c, called by both the SEQ note on and `SsUtKeyOnV`) squares EVERY
  voice's volume, sound effects too: AE's skips the square for a sound effect (voice marked
  `0x21`). Checked in the MIPS interpreter: the formula above with the square always on gave the
  same result for all of 3000 random SEQ notes and sound effects. Sound effects played up to 6 dB
  too loud against the music with AE's rule (velocity 66: 8514 instead of 4424).
- Its `SND_Init` and `SND_Reset` set the master volume to 127 (`SsSetMVol`, 0x800784d4), AE's to
  100 (`GameSound`'s `kMasterVolumeAO`/`kMasterVolumeAE`).
- `SsSeqSetVol` (0x80078ee4) always works out the playing notes' volumes again (`SpuVmSetSeqVol`
  0x8007cf10), with the volume of the channel the SEQ last read an event on, whichever channel
  played the note, and takes a volume of 0 as 1. Its `_SsNoteOn` skips notes at SEQ volume 0, AE's
  doesn't.
- The game clamps SEQ volumes to 10-127 in both.

`Libsnd::Config` (set in `PsxSoundEngine.cpp`) holds these per game.

`SsUtAllKeyOff` (AE 0x80074244, AO 0x80079160) doesn't only key the voices off: it first sets each
voice's volume to 0, pitch to 1000h and ADSR to 80FFh/4000h, so they go silent at once. With a plain
key off, the tones with a release shift of 29 or 30 (20% of AO's tones, 48% of AE's) take
minutes to fade, so looping ones kept playing over the FMVs (`SND_StopAll` is the only caller).
`Libsnd::UtAllKeyOff` does the same.

Found while chasing a buzz in POSITIV9's last notes, which play a noisy looped sample at 3.89x
(pitch `3e2c`) at velocity 66: squared, they sit 10 dB below the rest of the jingle instead of 6 dB.
Not yet confirmed that this is all of the difference from the PS1.

## The emulated SPU (`Sound/Spu`)

`PsxSpu` is a PS1 SPU implemented from psx-spx ("Sound Processing Unit (SPU)"), in its own
small library (`psx_spu`, no dependencies, `cmake --build build --target psx_spu`), unit tested by
`tests/PsxSpuTests.cpp`.

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

`PsxSpu::Interpolation::BandLimited` (the default, `-spu_filter=hq`) keeps the gaussian at
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

### Wired in

Both games' sound is one implementation: `Libsnd`, a port of the PS1 sound library (libsnd's SEQ
player and voice manager), drives the SPU, and `PsxSoundEngine` holds the two with the samples.
`SDLSoundSystem` owns the engine and mixes its output with the audio streams (`AudioStream`: the
movies' audio) into the SDL3 audio device, on a render thread that works a little ahead of the
device. `GameSound` (`Midi.cpp`) is the game's side: it loads a path's VAB and SEQs (AE's samples
are in `sounds.dat`, AO's in the VB) into the engine and plays the SEQs and sound effects the games
ask for (`SND_SEQ_Play`, `SFX_SfxDefinition_Play_Mono`, ...).

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

The `SoundGold` traces pin it, with each SPU voice's phase, pitch, volume, ADSR, reverb bit and
what libsnd plays on it; `PsxSoundEngineTests.cpp` tests `Libsnd` on its own.

### Checked against the real PS1 code

`build-ps1re/ref` (local only: it uses DuckStation's SPU, which is CC-BY-NC-ND, and needs the PS1
executables) runs each game's real libsnd out of its PS1 executable in a MIPS interpreter, on
DuckStation's SPU. `RELIVE_SOUND_RECORD=<file> relive` records the game's libsnd
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

### The music controller and the game's sound tables

Above libsnd, the games follow the PS1's own game code where the PC's differs. Checked
against the PS1 executables (October 2026):

- The SFX table (AO 113, AE 122 entries), the music tables (per level) and the SEQ name and volume
  tables are byte for byte the PS1's, except AE's slig shot and grenade throw (PC program 39, PS1 program 2, which
  the tables have). `build-ps1re/tablecmp/*.py` redoes the check.
- The music clock is the PS1's vblank counter read shifted right by one (`PsxSoundEngine::kVblankHz`
  / 2 units a second: 25 on PAL, 30 on NTSC, which is the PC's rate).
- AO: mono sound effects have a minimum volume of 10 (AE 1); `SND_SEQ_Play` sets the SEQ's volume
  before playing it (the PC plays, then sets); `SetMusicVolumeDelayed` stores the target volume
  itself.
- AE: `UpdateMusic` changes music on a beat grid: only when the time since the last change is a
  multiple of the music type's beat period (`MusicController::UpdateMusic`), where the PC
  waits for the old SEQ to end.
- The PS1 has no `ClearObject`: the controller keeps a dangling owner pointer. relive's pointer =
  1 does the same.

The sound effect functions (`SFX_SfxDefinition_Play_Mono` and `_Stereo`, `SND_MIDI`) are checked the
same way: `RELIVE_MUSIC_LOG` logs their arguments (SFXM, SFXS, SNDMIDI) and the resulting
`SsVoKeyOn` and `SsUtChangePitch` calls, and `ps1music` calls the PS1's own functions with the same
arguments. Every sound effect of both games at the default, a low, a high and a stereo volume,
and seeded random sequences of competing music requests (owners, flags, owner death, level changes),
give identical calls (AO 1099 SFX and 2101 music calls, AE 1010 and 2086).

Known differences from the PS1 that are on purpose: for notes more than 5 octaves below a tone's
centre the real libsnd reads before its pitch table and gets pitch 0 (a silent voice that stays
at full envelope, so it is never freed); `Libsnd` rounds down instead (`PitchFromTable`). `ps1snd
cmp` shows it as a few register writes that differ (AE's Feeco Depot SEQs). Pitches above 0x3FFF
aren't clamped (the AO secret area jingle).

To check the controller itself, `RELIVE_MUSIC_LOG=<file> relive [-AO] -automation=<script>`
logs the controller's inputs (CREATE, ENABLE, MUSIC, CLEAR, VUPDATE, FIND, RAND) and its
`SND_SEQ_*` calls with the music clock's ms (frames of the script, so it's repeatable), and
`build-ps1re/ref/ps1music [-ae] <file>` runs the real PS1 `MusicController` on the same inputs with
only the `SND_SEQ_*` wrappers hooked. `ps1music cmp <relive log> <ps1 calls>` diffs the calls.
`mcmp.sh` / `mcmp_ae.sh` do both for a script. The scripts drive the game with Automation's
`music`, `sfx`, `seq_play` and `seq_stop` commands; `-sound_debug` shows the same as a window.

### The PS1's own data: `-ps1_data`

`relive -ps1_data=<dir>` plays the PS1's VH, VB and SEQ files instead of the PC's, so relive and the
real PS1 code can be compared on the same data. `Ps1SoundData` indexes every `*.LVL` below `<dir>`
(a PS1 disc's files; the container is the PC's), `ResourceManagerWrapper::LoadSoundFile` gives its
files where it has them (a SEQ comes from the BSQ of the LVL that holds the theme's VH: a few SEQ
ids are in several BSQs with different SEQs), and `GameSound::LoadVab` decodes the VB's SPU ADPCM
(`Ps1SoundData::DecodeVab`: the loop from the block with the loop start flag, a second pass of the
loop decoded with the history the first left, as the SPU does) into the PCM the emulated SPU plays.
The PS1's VH has the PS1's tone priorities, volumes and centres, so `RestorePs1Tones` isn't used.

End to end (`build-ps1re/ref/e2e.sh`): the engine with `-ps1_data -spu_filter=gaussian` records its
libsnd calls and audio, the real libsnd on DuckStation's SPU replays the calls, and `snr.py` compares
the audio. AO (menu music, then Rupture Farms): the difference is 51.6 dB below the signal, the SPU
emulations' difference; loudness, spectrum and note onsets match. AE (Mines, music and effects):
44-55 dB below the signal in each second, with bursts at a few note onsets, which the recording's
millisecond timestamps can place a few samples off.

### Clicks: declick

The hardware clicks wherever a voice is cut in a step: a voice stopped while it is loud (a VAB
closed under it, a voice taken for another note, a release with no time), the master volume going
to 0 in one step, and an audio stream stopped while it plays (a skipped movie). The recordings of
the real PS1 code show the same steps, so this is on purpose better than the PS1:
- `PsxSpu::SetDeclick` (on in the engine, `RELIVE_NO_DECLICK=1` for the hardware's output, which the
  comparisons with the real libsnd use; the sound gold tests turn it off): a voice that stops while
  loud keeps adding its last output, fading to 0 over 128 samples (3 ms), which needs no sample data
  so a freed VAB is fine; the master volume moves to a new setting over about 12 ms.
  The envelope level doesn't fall faster than full to nothing in 128 samples in any phase
  (`PsxSpuEnvelope::SetMinimumRelease`), and a voice's volume follows its setting over the same time
  (exponentially, so a quiet voice fades as slowly as a loud one): libsnd's `SsUtAllKeyOff` writes
  the fastest decay into voices that are still sustaining, sets their volume to 0 and only then keys
  them off, and `SeqClose` zeroes the SEQ's volume first, so on the hardware the music drops to
  nothing in a few samples (the thump when the music stops for a movie).
  `PsxSoundEngine::FadeOutNextKeyOffs` lengthens both for a while: a movie starting fades the
  music and sounds out over 250 ms.
- `AudioStream`: starting fades in, `Stop` fades out over 127 samples (it reads as stopped at
  once), and running out of data fades out too.

### Still to do

- Loudness: with the real AE data a few loud SEQs (e.g. MI_6_1, NEGATIV3) hit the SPU's 16 bit
  clamp before the master volume, up to 0.1% of samples. Check against a PS1 recording before
  scaling anything.
- The PC samples are brighter than the PS1's (no ADPCM, and AO's PS1 data had some at lower rates),
  so the same notes sound clearer and a little louder (1-2 dB on the jingle): the PS1's gaussian
  interpolation dulls a low rate sample.
- Game logic: the PS1 games run at 25/50 Hz on PAL, relive at 30 fps, so how often the game
  starts sound effects differs.

## Checking a sound change against real game data

The repo has no game data, so `relive_sound_gold` renders every VAB tone and every SEQ of every
sound theme in a converted data dir to traces and WAVs, to compare before and after a change:

```sh
# 1. On the commit before the change: convert the data and render the gold set
cd /path/to/AE && /path/to/build/Source/relive/relive -convert
build/Source/Tools/sound_gold/relive_sound_gold -data=/path/to/AE -out=sound_gold_before

# 2. After the change: render again and compare
build/Source/Tools/sound_gold/relive_sound_gold -data=/path/to/AE -out=sound_gold_after -baseline=sound_gold_before
```

Do the same for AO with `-AO`. `-baseline` lists every trace or WAV that differs. A change that
isn't meant to alter the sound should differ nowhere.
