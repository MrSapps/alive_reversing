#pragma once

#include "../../relive_lib/Types.hpp"
#include <array>
#include <vector>

// The register side of the SPU that Libsnd drives. Libsnd only touches the hardware through this,
// so it can be run against the emulated SPU (PsxSoundEngine) or a recorder (the PS1 comparison
// tool in build-ps1re/ref).
class ILibsndSpu
{
public:
    virtual ~ILibsndSpu() = default;

    // ENVX: the voice's current ADSR level, 0-0x7FFF
    virtual u16 VoiceEnvelope(s32 voice) = 0;
    // VOLL/VOLR, raw registers
    virtual void SetVoiceVolume(s32 voice, u16 left, u16 right) = 0;
    // PITCH. NOT 16 bits: the PC samples can need more than the hardware's 3FFFh (see
    // PsxSpu::kMaxPitch).
    virtual void SetVoicePitch(s32 voice, u32 pitch) = 0;
    // SSA: libsnd writes the VAG's SPU address. Here it's which sample: vag is the VagAtr's vag
    // number (1-based) in VAB vabId.
    virtual void SetVoiceSample(s32 voice, s32 vabId, s32 vag) = 0;
    virtual void SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2) = 0;
    // KON, KOFF and EON (reverb) with a bit per voice, written once a tick in this order: KOFF,
    // KON, EON
    virtual void KeyOff(u32 voices) = 0;
    virtual void KeyOn(u32 voices) = 0;
    virtual void SetReverbVoices(u32 voices) = 0;
};

// A port of the PS1 sound library (PsyQ libsnd: the SEQ player and its voice manager) that both
// games use with -ps1_sound, so the music and sound effects play as the PS1 games played them.
//
// Ported from the MIT licensed libsnd decompilation in the sotn-decomp project
// (src/main/psxsdk/libsnd, https://github.com/Xeeynamo/sotn-decomp) and checked against the real
// code in Abe's Oddysee's PS1 executable (SLES_006.64), run in a MIPS interpreter on the same
// calls: the register writes must be the same. The libsnd names of what's ported are in the
// comments.
//
// Where it differs from libsnd on purpose:
// - Pitches aren't cut to 16 bits (see ILibsndSpu::SetVoicePitch).
// - A note more than 60 semitones below a tone's centre reads before libsnd's pitch table there.
//   Here it gets the right pitch instead.
// - Samples are named by VAG number, not SPU address, and the SEQ data is copied.
// - Only SEQs (SsSeqOpen), not SEP blocks: the games don't use SEPs.
//
// Not thread safe: the owner serialises the calls.
class Libsnd final
{
public:
    // How the game's libsnd differed
    struct Config final
    {
        // Ticks a second (libsnd's VBLANK_MINUS, from SsSetTickMode): 50 on the PAL discs
        s32 mTickRate = 50;
        // Whether a sound effect's voice volume is squared like a SEQ note's: AO's libsnd squares
        // both, AE's only the SEQ notes
        bool mSquareSfxVolume = true;
        // How the games' libsnd handles the SEQ volume. The libsnd of the decompilation only sets
        // it for the SEQ's later notes (SsSeqSetVol); AO's and AE's also work out the volumes of
        // the playing notes again (SpuVmSetSeqVol: 0x8007cf10 in AO's PS1 executable, 0x800776c4
        // in AE's), but not in the same cases.
        enum class SsSeqSetVolMode : u8
        {
            eLaterNotes,  // The decompilation
            eAlways,      // AO (0x80078ee4)
            eWhilePlaying // AE (0x80073ef4): while the SEQ is playing, else it only sets it
        };
        SsSeqSetVolMode mSsSeqSetVol = SsSeqSetVolMode::eAlways;
        // AE's SpuVmSetSeqVol always changes the playing notes, whoever calls it (SsSeqPlay, SsSeqClose)
        bool mSetSeqVolAlwaysUpdates = false;
        // AO's SpuVmSetSeqVol takes a 0 volume as 1
        bool mSeqVolZeroIsOne = true;
        // Whether the playing notes get the volume of their own MIDI channel (AE) or of the channel
        // the SEQ last read an event on (AO)
        bool mSeqVolNoteChannel = false;
        // Whether a SEQ at volume 0 plays no new notes (_SsNoteOn): AE's still does
        bool mNoteOnSkippedAtSeqVolZero = true;
    };

    static constexpr s32 kNumVoices = 24;
    static constexpr s32 kMaxVabs = 16;
    static constexpr s32 kMaxSeqs = 32;
    // The seq number libsnd gives sound effects (SsVoKeyOn, SsUtKeyOn) in its voice table
    static constexpr s16 kSfxSeq = 0x21;

    Libsnd(ILibsndSpu& spu, const Config& config);

    // _SsInit + SpuVmInit: every voice off, nothing open
    void Reset();

    // SsVabOpenHead with the VAB id the caller wants (the engine picks it). pVh is the VH file,
    // which is read now. Returns vabId, -1 on bad data or a used id.
    s16 VabOpenHead(const u8* pVh, u32 size, s16 vabId);
    // SsVabClose
    void VabClose(s16 vabId);

    // SsSeqOpen, with a copy of the SEQ. Returns the SEQ number, -1 if there's none free or it isn't
    // SEQ data.
    s16 SeqOpen(const u8* pSeq, u32 size, s16 vabId);
    // SsSeqClose
    void SeqClose(s16 seq);
    // SsSeqPlay: mode 1 plays, 0 pauses, count is how many times (0 = forever)
    void SeqPlay(s16 seq, s8 mode, s16 count);
    // SsSeqStop
    void SeqStop(s16 seq);
    // SsSeqPause / SsSeqReplay
    void SeqPause(s16 seq);
    void SeqReplay(s16 seq);
    // SsSeqSetVol, 0-127
    void SeqSetVol(s16 seq, s16 left, s16 right);
    // SsIsEos: 1 while the SEQ is playing
    s16 IsEos(s16 seq, s16 sep) const;
    // SsSeqSetCrescendo / SsSeqSetDecrescendo / SsSeqSetAccelerando / SsSeqSetRitardando aren't
    // used by the games

    // SsVoKeyOn: a sound effect. vabProg = vabId << 8 | program, pitch = note << 8 | fine (libsnd
    // ignores the fine tune). Returns a bit per voice it got, or -1 when there was none.
    s32 VoKeyOn(s32 vabProg, s32 pitch, u16 left, u16 right);
    // SsUtKeyOffV
    s16 UtKeyOffV(s16 voice);
    // SsUtChangePitch
    s16 UtChangePitch(s16 voice, s16 vabId, s16 prog, s16 oldNote, s16 oldFine, s16 newNote, s16 newFine);
    // SsUtAllKeyOff
    void UtAllKeyOff();

    // SsSeqCalledTbyT: one tick. Writes the registers changed since the last one, reads the
    // envelopes and plays the SEQs.
    void Tick();

    // The highest pitch each VAG of the VAB is played at by a SEQ's notes, bent up as far as the
    // tones go if the SEQ bends the channel (maxPitch[vag], VAG numbers from 1), or by a sound
    // effect's note (SfxPitches). For the PS1 sound's filtered samples.
    void SeqPitches(const u8* pSeq, u32 size, s16 vabId, std::array<u32, 256>& maxPitch) const;
    void SfxPitches(s32 vabProg, s32 pitch, std::array<u32, 256>& maxPitch) const;

    // For the traces: what a voice is playing (-1 when nothing)
    struct VoiceInfo final
    {
        s16 mSeq = -1;
        s16 mVabId = -1;
        s16 mProgram = -1;
        s16 mNote = -1;
        u8 mPriority = 0;
        bool mKeyed = false;
        s16 mAge = 0;
        u16 mEnvelope = 0;
    };
    VoiceInfo GetVoiceInfo(s32 voice) const;

private:
    // libsnd's VabHdr/ProgAtr/VagAtr, read from the VH
    struct ProgAtr final
    {
        u8 mTones = 0;
        u8 mVol = 0;
        u8 mPriority = 0;
        u8 mMode = 0;
        u8 mPan = 0;
        // reserved1: the "fake" program, programs with tones counted from 0 (the tone table only
        // has those)
        u16 mFakeProgram = 0;
    };

    struct VagAtr final
    {
        u8 mPriority = 0;
        u8 mMode = 0;
        u8 mVol = 0;
        u8 mPan = 0;
        u8 mCentre = 0;
        u8 mShift = 0;
        u8 mMin = 0;
        u8 mMax = 0;
        u8 mBendMin = 0;
        u8 mBendMax = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        s16 mVag = 0;
    };

    struct Vab final
    {
        bool mUsed = false;
        u8 mMasterVol = 0;
        u8 mMasterPan = 0;
        s16 mNumProgs = 0;
        s16 mMaxPrograms = 0;
        std::vector<ProgAtr> mProgs;
        std::vector<VagAtr> mTones; // fake program * 16 + tone
    };

    // _ss_score: a SEQ's play state (libsnd's field names in the comments)
    struct Score final
    {
        std::vector<u8> mData;
        const u8* mBase = nullptr;
        const u8* mEnd = nullptr;
        const u8* mReadPos = nullptr;    // read_pos
        const u8* mNextSepPos = nullptr; // next_sep_pos: the first event
        const u8* mLoopPos = nullptr;    // loop_pos
        u8 mNrpnLoopActive = 0;          // unk10
        u8 mRunningStatus = 0;           // unk11
        u8 mChannel = 0;                 // channel
        u8 mRpn1 = 0;                    // unk13
        u8 mRpn2 = 0;                    // unk14
        u8 mNrpnData = 0;                // unk15
        u8 mNrpn2 = 0;                   // unk16
        std::array<u8, 16> mPan = {};    // panpot
        u8 mLoopSet = 0;                 // unk27
        u8 mLoopCount = 0;               // unk28
        u8 mRpnCount = 0;                // unk29
        u8 mNrpnCount = 0;               // unk2a
        u8 mPlaying = 0;                 // unk2b, what SsIsEos returns
        std::array<u8, 16> mPrograms = {};
        s16 mVabId = 0;                  // unk4c
        s16 mResolution = 0;             // unk4a: ticks a quarter note
        s16 mRepeatCount = 0;            // unk46
        s16 mPlayCount = 0;              // unk48
        std::array<s16, 16> mVol = {};   // vol
        s16 mTickCounter = 0;            // unk6E
        s16 mTicksPerTick = 0;           // unk70: SEQ ticks a libsnd tick (or tick wait, see _SsSeqPlay)
        s16 mInitialTicksPerTick = 0;    // unk72
        u16 mSeqVolLeft = 127;           // unk74
        u16 mSeqVolRight = 127;          // unk76
        s16 mVolLeftCopy = 127;          // unk78
        s16 mVolRightCopy = 127;         // unk7A
        s32 mFirstDelta = 0;             // unk7c
        u32 mElapsed = 0;                // unk80
        s32 mInitialTempo = 0;           // unk84: beats a minute
        s32 mDelta = 0;                  // delta_value
        s32 mTempo = 0;                  // unk8c
        u32 mFlags = 0;                  // unk90: 1 play, 2 pause, 4 stopped, 8 replay, 0x200 ended
        s16 mLastVelocity = 127;         // padA6
        u16 mMutedChannels = 0;          // padaa
    };

    // libsnd's _svm_voice
    struct Voice final
    {
        s16 mVag = 0;          // unk0: 0xFF a noise voice
        s16 mAge = 0;          // unk2: allocations since this voice's
        s16 mPitch = 0;        // unk04
        u16 mEnvelope = 0;     // unk6: ENVX at the last tick, 0x7FFF from key on
        s16 mVelocity = 0;     // unk8
        u8 mPan = 64;          // unka
        s16 mNote = 0;         // note
        s16 mSeq = -1;         // unke: seq | sep << 8, kSfxSeq for sound effects
        u8 mChannel = 0;       // A SEQ note's MIDI channel (AE's libsnd keeps it)
        s16 mFakeProgram = 0;  // unk10
        s16 mProgram = 0;      // prog
        s16 mTone = 0;         // tone
        s16 mVabId = 0;        // vabId
        s16 mPriority = 0;     // unk18
        u8 mKeyState = 0;      // unk1b: 0 free, 1 keyed on, 2 noise
    };

    // _svm_cur: the note being set up
    struct Cur final
    {
        u8 mToneCount = 0;   // field_0
        s8 mVabId = 0;       // field_1
        u8 mNote = 0;        // field_2
        u8 mFine = 0;        // field_0x3
        s16 mVelocity = 0;   // field_4_voll (velocity, scaled by the channel volume)
        u8 mChannelPan = 0;  // field_0x5
        u8 mProgram = 0;     // field_6
        u8 mFakeProgram = 0; // field_7
        u8 mProgVol = 0;     // field_A
        u8 mProgPan = 0;     // field_B
        u8 mTone = 0;        // field_C
        u8 mToneVol = 0;     // field_D
        u8 mTonePan = 0;     // field_E
        u8 mPriority = 0;    // field_F
        u8 mCentre = 0;      // field_10
        u8 mShift = 0;       // field_11
        u8 mMode = 0;        // field_14 (VagAtr mode, bit 2 = reverb)
        s16 mSeq = 0;        // field_16
        s16 mVag = 0;        // field_18
        s16 mVoice = 0;      // field_0x1a
    };

    // The register shadow SpuVmFlush writes out (_svm_sreg_buf / _svm_sreg_dirty)
    struct VoiceRegs final
    {
        u16 mVolLeft = 0;
        u16 mVolRight = 0;
        u32 mPitch = 0;
        s16 mVabId = 0;
        s16 mVag = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        u8 mDirty = 0; // 1 volume, 4 pitch, 8 sample, 0x10 ADSR
    };

    Score* GetScore(s16 seqSep);
    const Score* GetScore(s16 seqSep) const;

    // SpuVmVSetUp
    bool VSetUp(s16 vabId, s16 prog);
    // SpuVmAlloc
    u8 Alloc();
    // SpuVmDoAllocate
    void DoAllocate();
    // SpuVmKeyOnNow
    void KeyOnNow(u32 pitch);
    // note2pitch (a SEQ note or SsVoKeyOn: no fine tune) and note2pitch2
    u32 NoteToPitch() const;
    u32 NoteToPitch2(u16 note, u16 fine) const;
    static u32 PitchFromTable(s32 semitones, s32 step);
    // The pitches of the VAB's tones for a note of program prog
    void NotePitches(s16 vabId, s32 prog, s32 note, bool bent, std::array<u32, 256>& maxPitch) const;
    // SpuVmKeyOn / SpuVmKeyOff
    s32 VmKeyOn(s16 seqSep, s16 vabId, s16 prog, u16 note, u16 velocity, u16 pan);
    s32 VmKeyOff(s16 seqSep, s16 vabId, s16 prog, u16 note);
    // SpuVmSeqKeyOff
    void SeqKeyOff(s16 seqSep);
    // SpuVmSetSeqVol
    void SetSeqVol(s16 seqSep, u16 left, u16 right, s16 updateVoices);
    // A playing note's volume worked out again, when the SEQ volume changes
    void SeqVoiceVolume(s32 voice, const Score& score);
    // A playing note's volume from a velocity (scaled by the channel volume), the SEQ volume and
    // a channel pan, as at its key on
    void UpdateVoiceVolume(s32 voice, s32 velocity, const Score& score, u8 channelPan);
    // SpuVmSetVol: a channel volume or pan change on the SEQ's playing notes of the program
    void SetVol(s16 seqSep, s16 vabId, s16 prog, u16 vol, u16 pan);
    // SpuVmPitchBend / SpuVmPBVoice
    void PitchBend(s16 seqSep, s16 vabId, s16 prog, u16 amount);
    void KeyOffVoice(s32 voice);
    // SpuVmFlush
    void Flush();

    // The SEQ player (seqread.c)
    s16 InitSoundSeq(s16 seq, s16 vabId, const u8* pSeq, u32 size);
    void SeqPlayTick(s16 seq, s16 sep);
    void GetSeqData(s16 seq, s16 sep);
    void NoteOn(s16 seq, s16 sep, u8 note, u8 velocity);
    void SetControlChange(s16 seq, s16 sep, u8 control);
    void ContNrpn1(s16 seq, s16 sep, u8 value);
    void ContNrpn2(s16 seq, s16 sep, u8 value);
    void GetMetaEvent(s16 seq, s16 sep, u8 type);
    s32 ReadDeltaValue(s16 seq, s16 sep);
    u8 ReadByte(Score& score);
    void UpdateTicksPerTick(Score& score);
    void SndStop(s16 seq, s16 sep);

    ILibsndSpu& mSpu;
    Config mConfig;

    std::array<Vab, kMaxVabs> mVabs;
    std::array<Score, kMaxSeqs> mScores;
    u32 mOpenSeqs = 0; // _snd_openflag
    bool mInEvent = false; // _snd_ev_flag

    std::array<Voice, kNumVoices> mVoices;
    std::array<VoiceRegs, kNumVoices> mRegs;
    Cur mCur;
    u32 mKeyOn = 0;  // _svm_okon1/2
    u32 mKeyOff = 0; // _svm_okof1/2
    u32 mReverb = 0; // _svm_orev1/2
    std::array<u32, 16> mEnvelopeHistory = {}; // _svm_envx_hist
    s32 mEnvelopeHistoryPos = 0;
    u16 mDamper = 0; // _svm_damper
};
