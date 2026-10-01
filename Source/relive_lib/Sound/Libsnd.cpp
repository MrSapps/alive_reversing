#include "../stdafx.h"
#include "Libsnd.hpp"
#include <algorithm>
#include <cstring>

// The pitch of the 16 steps (1/16 semitones) of each semitone of the octave above a tone's
// centre note, 0x1000 = the sample's own rate (libsnd's table, at 0x80032F14 in SOTN)
static const u16 kPitchTable[12 * 16 + 1] = {
    0x1000, 0x100E, 0x101D, 0x102C, 0x103B, 0x104A, 0x1059, 0x1068, 0x1078, 0x1087, 0x1096, 0x10A5, 0x10B5, 0x10C4, 0x10D4, 0x10E3, 0x10F3, 0x1103, 0x1113, 0x1122,
    0x1132, 0x1142, 0x1152, 0x1162, 0x1172, 0x1182, 0x1193, 0x11A3, 0x11B3, 0x11C4, 0x11D4, 0x11E5, 0x11F5, 0x1206, 0x1216, 0x1227, 0x1238, 0x1249, 0x125A, 0x126B,
    0x127C, 0x128D, 0x129E, 0x12AF, 0x12C1, 0x12D2, 0x12E3, 0x12F5, 0x1306, 0x1318, 0x132A, 0x133C, 0x134D, 0x135F, 0x1371, 0x1383, 0x1395, 0x13A7, 0x13BA, 0x13CC,
    0x13DE, 0x13F1, 0x1403, 0x1416, 0x1428, 0x143B, 0x144E, 0x1460, 0x1473, 0x1486, 0x1499, 0x14AC, 0x14BF, 0x14D3, 0x14E6, 0x14F9, 0x150D, 0x1520, 0x1534, 0x1547,
    0x155B, 0x156F, 0x1583, 0x1597, 0x15AB, 0x15BF, 0x15D3, 0x15E7, 0x15FB, 0x1610, 0x1624, 0x1638, 0x164D, 0x1662, 0x1676, 0x168B, 0x16A0, 0x16B5, 0x16CA, 0x16DF,
    0x16F4, 0x170A, 0x171F, 0x1734, 0x174A, 0x175F, 0x1775, 0x178B, 0x17A1, 0x17B6, 0x17CC, 0x17E2, 0x17F9, 0x180F, 0x1825, 0x183B, 0x1852, 0x1868, 0x187F, 0x1896,
    0x18AC, 0x18C3, 0x18DA, 0x18F1, 0x1908, 0x191F, 0x1937, 0x194E, 0x1965, 0x197D, 0x1995, 0x19AC, 0x19C4, 0x19DC, 0x19F4, 0x1A0C, 0x1A24, 0x1A3C, 0x1A55, 0x1A6D,
    0x1A85, 0x1A9E, 0x1AB7, 0x1ACF, 0x1AE8, 0x1B01, 0x1B1A, 0x1B33, 0x1B4C, 0x1B66, 0x1B7F, 0x1B98, 0x1BB2, 0x1BCC, 0x1BE5, 0x1BFF, 0x1C19, 0x1C33, 0x1C4D, 0x1C67,
    0x1C82, 0x1C9C, 0x1CB7, 0x1CD1, 0x1CEC, 0x1D07, 0x1D22, 0x1D3D, 0x1D58, 0x1D73, 0x1D8E, 0x1DA9, 0x1DC5, 0x1DE0, 0x1DFC, 0x1E18, 0x1E34, 0x1E50, 0x1E6C, 0x1E88,
    0x1EA4, 0x1EC1, 0x1EDD, 0x1EFA, 0x1F16, 0x1F33, 0x1F50, 0x1F6D, 0x1F8A, 0x1FA7, 0x1FC5, 0x1FE2, 0x2000};

static u16 ReadU16(const u8* p)
{
    return static_cast<u16>(p[0] | (p[1] << 8));
}

static u32 ReadU32(const u8* p)
{
    return static_cast<u32>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

Libsnd::Libsnd(ILibsndSpu& spu, const Config& config)
    : mSpu(spu)
    , mConfig(config)
{
    Reset();
}

void Libsnd::Reset()
{
    for (Vab& vab : mVabs)
    {
        vab = Vab{};
    }
    // SsSetTableSize
    for (Score& score : mScores)
    {
        score = Score{};
    }
    mOpenSeqs = 0;
    mInEvent = false;

    // SpuVmInit
    for (s32 i = 0; i < kNumVoices; i++)
    {
        Voice& v = mVoices[i];
        v = Voice{};
        v.mAge = 0x18;
        v.mSeq = -1;
        v.mVag = 0;
        v.mTone = 0xFF;
        mRegs[i] = VoiceRegs{};
    }
    mCur = Cur{};
    mKeyOn = 0;
    mKeyOff = 0;
    mReverb = 0;
    mEnvelopeHistory.fill(0);
    mEnvelopeHistoryPos = 0;
    mDamper = 0;
    // SpuVmInit ends with a flush. It matters: the flush frees a voice by ANDing history slots 0-14
    // (not the last 15 ticks), so which voices get freed when depends on where the history is.
    Flush();
}

// ---- VABs ----

s16 Libsnd::VabOpenHead(const u8* pVh, u32 size, s16 vabId)
{
    // SsVabOpenHeadWithMode, without the SPU memory: samples are named by VAG number
    if (vabId < 0 || vabId >= kMaxVabs || mVabs[vabId].mUsed || !pVh || size < 0x20)
    {
        return -1;
    }

    const u32 form = ReadU32(pVh);
    if ((form >> 8) != ('V' << 16 | 'A' << 8 | 'B'))
    {
        return -1;
    }
    const u32 version = ReadU32(pVh + 4);
    const s16 maxPrograms = ((form & 0xFF) == 'p' && version >= 5) ? 0x80 : 0x40;
    const s16 numProgs = static_cast<s16>(ReadU16(pVh + 0x12));
    if (numProgs > maxPrograms || size < 0x20u + maxPrograms * 0x10u + numProgs * 0x200u)
    {
        return -1;
    }

    Vab& vab = mVabs[vabId];
    vab = Vab{};
    vab.mMasterVol = pVh[0x18];
    vab.mMasterPan = pVh[0x19];
    vab.mNumProgs = numProgs;
    vab.mMaxPrograms = maxPrograms;

    vab.mProgs.resize(maxPrograms);
    u16 fakeProgram = 0;
    for (s32 i = 0; i < maxPrograms; i++)
    {
        const u8* p = pVh + 0x20 + i * 0x10;
        ProgAtr& prog = vab.mProgs[i];
        prog.mTones = p[0];
        prog.mVol = p[1];
        prog.mPriority = p[2];
        prog.mMode = p[3];
        prog.mPan = p[4];
        prog.mFakeProgram = fakeProgram;
        if (prog.mTones != 0)
        {
            fakeProgram++;
        }
    }

    vab.mTones.resize(numProgs * 16);
    for (s32 i = 0; i < numProgs * 16; i++)
    {
        const u8* t = pVh + 0x20 + maxPrograms * 0x10 + i * 0x20;
        VagAtr& tone = vab.mTones[i];
        tone.mPriority = t[0];
        tone.mMode = t[1];
        tone.mVol = t[2];
        tone.mPan = t[3];
        tone.mCentre = t[4];
        tone.mShift = t[5];
        tone.mMin = t[6];
        tone.mMax = t[7];
        tone.mBendMin = t[0xC];
        tone.mBendMax = t[0xD];
        tone.mAdsr1 = ReadU16(t + 0x10);
        tone.mAdsr2 = ReadU16(t + 0x12);
        tone.mVag = static_cast<s16>(ReadU16(t + 0x16));
    }
    vab.mUsed = true;
    return vabId;
}

void Libsnd::VabClose(s16 vabId)
{
    if (vabId >= 0 && vabId < kMaxVabs)
    {
        mVabs[vabId] = Vab{};
    }
}

bool Libsnd::VSetUp(s16 vabId, s16 prog)
{
    // SpuVmVSetUp: true when it's set up
    if (vabId < 0 || vabId >= kMaxVabs || !mVabs[vabId].mUsed || prog >= mVabs[vabId].mMaxPrograms || prog < 0)
    {
        return false;
    }
    mCur.mVabId = static_cast<s8>(vabId);
    mCur.mProgram = static_cast<u8>(prog);
    mCur.mFakeProgram = static_cast<u8>(mVabs[vabId].mProgs[prog].mFakeProgram);
    return true;
}

// ---- the voice manager ----

Libsnd::Score* Libsnd::GetScore(s16 seqSep)
{
    const s32 seq = seqSep & 0xFF;
    return seq < kMaxSeqs ? &mScores[seq] : nullptr;
}

const Libsnd::Score* Libsnd::GetScore(s16 seqSep) const
{
    const s32 seq = seqSep & 0xFF;
    return seq < kMaxSeqs ? &mScores[seq] : nullptr;
}

u8 Libsnd::Alloc()
{
    // SpuVmAlloc: a free voice (released and silent) if there's one. Else of the voices whose
    // priority is at most the new note's: the lowest priority, then the quietest, then the oldest.
    u8 channel = 99;
    u16 lowestEnvelope = 0xFFFF;
    u8 candidates = 0;
    u16 oldestAge = 0;
    u16 prior = mCur.mPriority;
    u8 best = 99;
    for (u8 i = 0; i < kNumVoices; i++)
    {
        const Voice& v = mVoices[i];
        if (v.mKeyState == 0 && v.mEnvelope == 0)
        {
            channel = i;
            break;
        }
        if (v.mPriority < prior)
        {
            prior = static_cast<u16>(v.mPriority);
            best = i;
            lowestEnvelope = v.mEnvelope;
            oldestAge = static_cast<u16>(v.mAge);
            candidates = 1;
        }
        else if (v.mPriority == prior)
        {
            candidates++;
            if (v.mEnvelope < lowestEnvelope)
            {
                oldestAge = static_cast<u16>(v.mAge);
                lowestEnvelope = v.mEnvelope;
                best = i;
            }
            else if (v.mEnvelope == lowestEnvelope && oldestAge < static_cast<u16>(v.mAge))
            {
                oldestAge = static_cast<u16>(v.mAge);
                best = i;
            }
        }
    }
    if (channel == 99)
    {
        channel = candidates == 0 ? static_cast<u8>(kNumVoices) : best;
    }
    if (channel < kNumVoices)
    {
        for (Voice& v : mVoices)
        {
            v.mAge++;
        }
        mVoices[channel].mAge = 0;
        mVoices[channel].mPriority = mCur.mPriority;
    }
    return channel;
}

void Libsnd::DoAllocate()
{
    // SpuVmDoAllocate: the voice's sample and ADSR, and it counts as at full volume until the next
    // tick reads its envelope
    const s32 voice = mCur.mVoice;
    const Vab& vab = mVabs[mCur.mVabId];
    const VagAtr& tone = vab.mTones[mCur.mFakeProgram * 16 + mCur.mTone];
    mVoices[voice].mEnvelope = 0x7FFF;
    for (u32& h : mEnvelopeHistory)
    {
        h &= ~(1u << voice);
    }
    VoiceRegs& regs = mRegs[voice];
    regs.mVabId = mCur.mVabId;
    regs.mVag = mCur.mVag;
    regs.mAdsr1 = tone.mAdsr1;
    regs.mAdsr2 = static_cast<u16>(tone.mAdsr2 + mDamper);
    regs.mDirty |= 0x8 | 0x30;
}

void Libsnd::KeyOnNow(u32 pitch)
{
    // SpuVmKeyOnNow: the voice volume from the note's velocity, the VAB, program and tone volumes,
    // the SEQ volume and the three pans, then squared
    const s32 voice = mCur.mVoice;
    const Vab& vab = mVabs[mCur.mVabId];
    const u32 masterScaled = vab.mMasterVol * 0x3FFFu;
    u32 left = ((static_cast<u32>(mCur.mVelocity) * masterScaled) / 0x7F) / 0x7F;
    left = ((left * mCur.mProgVol * mCur.mToneVol) / 0x7F) / 0x7F;
    u32 right = left;

    const bool sfx = mCur.mSeq == kSfxSeq;
    if (!sfx)
    {
        const Score* pScore = GetScore(mCur.mSeq);
        left = (left * pScore->mSeqVolLeft) / 0x7F;
        right = (right * pScore->mSeqVolRight) / 0x7F;
    }

    u32 voll;
    u32 volr;
    if (mCur.mTonePan < 0x40)
    {
        voll = left;
        volr = (right * mCur.mTonePan) / 0x3F;
    }
    else
    {
        voll = (left * (0x7F - mCur.mTonePan)) / 0x3F;
        volr = right;
    }
    if (mCur.mProgPan < 0x40)
    {
        volr = (volr * mCur.mProgPan) / 0x3F;
    }
    else
    {
        voll = (voll * (0x7F - mCur.mProgPan)) / 0x3F;
    }
    if (mCur.mChannelPan < 0x40)
    {
        volr = (volr * mCur.mChannelPan) / 0x3F;
    }
    else
    {
        voll = (voll * (0x7F - mCur.mChannelPan)) / 0x3F;
    }

    if (!sfx || mConfig.mSquareSfxVolume)
    {
        voll = (voll * voll) / 0x3FFF;
        volr = (volr * volr) / 0x3FFF;
    }

    VoiceRegs& regs = mRegs[voice];
    regs.mPitch = pitch;
    regs.mVolLeft = static_cast<u16>(voll);
    regs.mVolRight = static_cast<u16>(volr);
    regs.mDirty |= 7;

    mVoices[voice].mPitch = static_cast<s16>(pitch);
    mVoices[voice].mKeyState = 1;
    const u32 bit = 1u << voice;
    if (mCur.mMode & 4)
    {
        mReverb |= bit;
    }
    else
    {
        mReverb &= ~bit;
    }
    mKeyOn |= bit;
    mKeyOff &= ~mKeyOn;
}

u32 Libsnd::PitchFromTable(s32 semitones, s32 step)
{
    // semitones from 5 octaves below the centre. libsnd divides with C's rounding towards zero,
    // which reads before its table for notes more than 5 octaves below the centre: round down
    // instead, which gives the right pitch there.
    s32 octave = semitones / 12;
    s32 pos = semitones % 12;
    if (pos < 0)
    {
        pos += 12;
        octave--;
    }
    u32 pitch = kPitchTable[pos * 16 + step];
    octave -= 5;
    if (octave > 0)
    {
        pitch <<= octave;
    }
    else if (octave < 0)
    {
        pitch >>= std::min(-octave, 31);
    }
    return pitch;
}

u32 Libsnd::NoteToPitch() const
{
    // note2pitch: no fine tune, and the tone's shift rounded down to 1/16 semitones, at most 15
    const s32 semitones = mCur.mNote + 60 - mCur.mCentre;
    const s32 step = std::min(mCur.mShift / 8, 15);
    return PitchFromTable(semitones, step);
}

u32 Libsnd::NoteToPitch2(u16 note, u16 fine) const
{
    // note2pitch2: the fine tune plus the tone's shift, carrying one semitone
    const Vab& vab = mVabs[mCur.mVabId];
    const VagAtr& tone = vab.mTones[mCur.mFakeProgram * 16 + mCur.mTone];
    s32 step = static_cast<s16>(fine + tone.mShift) / 8;
    s32 carry = 0;
    if (step >= 16)
    {
        carry = 1;
        step -= 16;
    }
    const s32 semitones = static_cast<s16>(note + 60 - tone.mCentre + carry);
    return PitchFromTable(semitones, step);
}

s32 Libsnd::VmKeyOn(s16 seqSep, s16 vabId, s16 prog, u16 note, u16 velocity, u16 pan)
{
    // SpuVmKeyOn: a voice for each of the program's tones whose key range has the note
    if (!VSetUp(vabId, prog))
    {
        return -1;
    }
    mCur.mSeq = seqSep;
    mCur.mNote = static_cast<u8>(note);
    mCur.mFine = 0;
    if (seqSep == kSfxSeq)
    {
        mCur.mVelocity = static_cast<s16>(velocity);
    }
    else
    {
        const Score* pScore = GetScore(seqSep);
        mCur.mVelocity = static_cast<s16>((velocity * pScore->mVol[pScore->mChannel]) / 0x7F);
    }
    const Vab& vab = mVabs[vabId];
    mCur.mChannelPan = static_cast<u8>(pan);
    mCur.mProgVol = vab.mProgs[prog].mVol;
    mCur.mProgPan = vab.mProgs[prog].mPan;
    mCur.mToneCount = vab.mProgs[prog].mTones;
    if (mCur.mFakeProgram >= vab.mNumProgs)
    {
        return -1;
    }
    if (velocity == 0)
    {
        return VmKeyOff(seqSep, vabId, prog, note);
    }

    u8 toneIdx[16];
    u8 vagIdx[16];
    u8 vagCount = 0;
    for (u8 i = 0; i < mCur.mToneCount && i < 16; i++)
    {
        const VagAtr& tone = vab.mTones[mCur.mFakeProgram * 16 + i];
        if (tone.mMin > mCur.mNote || mCur.mNote > tone.mMax)
        {
            continue;
        }
        vagIdx[vagCount] = static_cast<u8>(tone.mVag);
        toneIdx[vagCount++] = i;
    }

    s32 result = 0;
    for (u8 i = 0; i < vagCount; i++)
    {
        mCur.mVag = vagIdx[i];
        mCur.mTone = toneIdx[i];
        const VagAtr& tone = vab.mTones[mCur.mFakeProgram * 16 + mCur.mTone];
        mCur.mPriority = tone.mPriority;
        mCur.mToneVol = tone.mVol;
        mCur.mTonePan = tone.mPan;
        mCur.mCentre = tone.mCentre;
        mCur.mShift = tone.mShift;
        mCur.mMode = tone.mMode;
        mCur.mVoice = Alloc();
        if (mCur.mVoice < kNumVoices)
        {
            Voice& v = mVoices[mCur.mVoice];
            v.mKeyState = 1;
            v.mAge = 0;
            v.mSeq = seqSep;
            v.mVabId = mCur.mVabId;
            v.mFakeProgram = mCur.mFakeProgram;
            v.mProgram = prog;
            if (seqSep != kSfxSeq)
            {
                v.mVelocity = static_cast<s16>(velocity);
                v.mChannel = GetScore(seqSep)->mChannel;
            }
            v.mPan = static_cast<u8>(pan);
            v.mTone = mCur.mTone;
            v.mNote = static_cast<s16>(note);
            v.mPriority = mCur.mPriority;
            v.mVag = mCur.mVag;
            DoAllocate();
            // VAG 0xFF is libsnd's noise voice, which the games' VABs don't use
            KeyOnNow(NoteToPitch());
            result |= 1 << mCur.mVoice;
        }
        else
        {
            result = -1;
        }
    }
    return result;
}

void Libsnd::KeyOffVoice(s32 voice)
{
    Voice& v = mVoices[voice];
    v.mKeyState = 0;
    v.mPitch = 0;
    v.mVag = 0;
    mKeyOff |= 1u << voice;
    mKeyOn &= ~mKeyOff;
}

s32 Libsnd::VmKeyOff(s16 seqSep, s16 vabId, s16 prog, u16 note)
{
    // SpuVmKeyOff: every voice the SEQ keyed on with this note, VAB and program
    s32 count = 0;
    for (s32 i = 0; i < kNumVoices; i++)
    {
        const Voice& v = mVoices[i];
        if (v.mNote == static_cast<s16>(note) && v.mProgram == prog && v.mSeq == seqSep && v.mVabId == vabId)
        {
            KeyOffVoice(i);
            count++;
        }
    }
    return count;
}

void Libsnd::SeqKeyOff(s16 seqSep)
{
    // SpuVmSeqKeyOff
    for (s32 i = 0; i < kNumVoices; i++)
    {
        if (mVoices[i].mSeq == seqSep)
        {
            KeyOffVoice(i);
        }
    }
}

void Libsnd::SetSeqVol(s16 seqSep, u16 left, u16 right, s16 updateVoices)
{
    // SpuVmSetSeqVol
    Score* pScore = GetScore(seqSep);
    if (!pScore)
    {
        return;
    }
    mCur.mSeq = seqSep;
    if (mConfig.mSeqVolZeroIsOne)
    {
        left = left == 0 ? 1 : left;
        right = right == 0 ? 1 : right;
    }
    pScore->mSeqVolLeft = left >= 0x80 ? 0x7F : left;
    pScore->mSeqVolRight = right >= 0x80 ? 0x7F : right;
    if (updateVoices != 1 && !mConfig.mSetSeqVolAlwaysUpdates)
    {
        return;
    }
    for (s32 i = 0; i < kNumVoices; i++)
    {
        if (static_cast<u16>(mVoices[i].mSeq) != static_cast<u16>(seqSep))
        {
            continue;
        }
        if (mConfig.mSsSeqSetVol != Config::SsSeqSetVolMode::eLaterNotes)
        {
            SeqVoiceVolume(i, *pScore);
        }
        else
        {
            mRegs[i].mVolLeft = static_cast<u16>(left * 0x81);
            mRegs[i].mVolRight = static_cast<u16>(right * 0x81);
            mRegs[i].mDirty |= 3;
        }
    }
}

void Libsnd::SeqVoiceVolume(s32 voice, const Score& score)
{
    // AO's and AE's SpuVmSetSeqVol (0x8007cf10, 0x800776c4): the volume worked out as at the key
    // on. AO's uses the volume of the channel the SEQ last read an event on, whichever channel
    // played the note, AE's the note's channel.
    const Voice& v = mVoices[voice];
    const u8 channel = mConfig.mSeqVolNoteChannel ? v.mChannel : score.mChannel;
    UpdateVoiceVolume(voice, (v.mVelocity * score.mVol[channel & 0xF]) / 0x7F, score, v.mPan);
}

void Libsnd::UpdateVoiceVolume(s32 voice, s32 velocity, const Score& score, u8 channelPan)
{
    const Voice& v = mVoices[voice];
    if (v.mVabId < 0 || v.mVabId >= kMaxVabs || !mVabs[v.mVabId].mUsed)
    {
        return;
    }
    const Vab& vab = mVabs[v.mVabId];
    const s32 toneIdx = v.mFakeProgram * 16 + v.mTone;
    if (v.mProgram < 0 || v.mProgram >= static_cast<s32>(vab.mProgs.size()) || toneIdx < 0 || toneIdx >= static_cast<s32>(vab.mTones.size()))
    {
        return;
    }
    const VagAtr& tone = vab.mTones[toneIdx];
    const ProgAtr& prog = vab.mProgs[v.mProgram];

    u32 vol = (static_cast<u32>(velocity * 0x3FFF) * vab.mMasterVol) / (0x7F * 0x7F);
    vol = (vol * prog.mVol * tone.mVol) / (0x7F * 0x7F);
    const u32 left = (vol * score.mSeqVolLeft) / 0x7F;
    const u32 right = (vol * score.mSeqVolRight) / 0x7F;

    u32 voll;
    u32 volr;
    if (tone.mPan < 0x40)
    {
        voll = left;
        volr = (right * tone.mPan) / 0x3F;
    }
    else
    {
        voll = (left * (0x7F - tone.mPan)) / 0x3F;
        volr = right;
    }
    if (prog.mPan < 0x40)
    {
        volr = (volr * prog.mPan) / 0x3F;
    }
    else
    {
        voll = (voll * (0x7F - prog.mPan)) / 0x3F;
    }
    if (channelPan < 0x40)
    {
        volr = (volr * channelPan) / 0x3F;
    }
    else
    {
        voll = (voll * (0x7F - channelPan)) / 0x3F;
    }
    voll = (voll * voll) / 0x3FFF;
    volr = (volr * volr) / 0x3FFF;
    mRegs[voice].mVolLeft = static_cast<u16>(voll);
    mRegs[voice].mVolRight = static_cast<u16>(volr);
    mRegs[voice].mDirty |= 3;
}

void Libsnd::SetVol(s16 seqSep, s16 vabId, s16 prog, u16 vol, u16 pan)
{
    // SpuVmSetVol: MIDI CC 7 (volume) or 10 (pan) on the SEQ's playing notes of the program. As AE's
    // (0x80077ec4): the libsnd of the decompilation reads the tone's and program's pans from the
    // wrong entries. AO's SEQs have no controller events.
    VSetUp(vabId, prog);
    mCur.mSeq = seqSep;
    Score* pScore = GetScore(seqSep);
    if (!pScore)
    {
        return;
    }
    for (s32 i = 0; i < kNumVoices; i++)
    {
        const Voice& v = mVoices[i];
        if (v.mSeq != seqSep || v.mProgram != prog || v.mVabId != vabId)
        {
            continue;
        }
        if (pScore->mVol[pScore->mChannel] != static_cast<s16>(vol) && pScore->mVol[pScore->mChannel] == 0)
        {
            pScore->mVol[pScore->mChannel] = 1;
        }
        UpdateVoiceVolume(i, (v.mVelocity * vol) / 0x7F, *pScore, static_cast<u8>(pan));
    }
}

void Libsnd::PitchBend(s16 seqSep, s16 vabId, s16 prog, u16 amount)
{
    // SpuVmPitchBend: every voice the SEQ plays the program on, by the bend's high 7 bits, scaled by
    // each tone's bend range (pbmin/pbmax semitones)
    VSetUp(vabId, prog);
    mCur.mSeq = seqSep;
    if (vabId < 0 || vabId >= kMaxVabs || !mVabs[vabId].mUsed)
    {
        return;
    }
    const Vab& vab = mVabs[vabId];
    const s16 bend = static_cast<s16>(amount - 0x40);
    for (s32 i = 0; i < kNumVoices; i++)
    {
        const Voice& v = mVoices[i];
        if (v.mSeq != seqSep || v.mVabId != vabId || v.mProgram != prog)
        {
            continue;
        }
        const s32 toneIdx = v.mTone + mCur.mFakeProgram * 16;
        if (toneIdx < 0 || toneIdx >= static_cast<s32>(vab.mTones.size()))
        {
            continue;
        }
        const VagAtr& tone = vab.mTones[toneIdx];
        u16 note;
        u16 fine;
        if (bend > 0)
        {
            note = static_cast<u16>(v.mNote + (bend * tone.mBendMax) / 0x3F);
            fine = static_cast<u16>(((bend * tone.mBendMax) % 0x3F) * 2);
        }
        else if (bend < 0)
        {
            note = static_cast<u16>(v.mNote + (bend * tone.mBendMin) / 0x40 - 1);
            fine = static_cast<u16>(((bend * tone.mBendMin) % 0x40) * 2 + 0x7F);
        }
        else
        {
            note = static_cast<u16>(v.mNote);
            fine = 0;
        }
        mCur.mTone = static_cast<u8>(v.mTone);
        mCur.mVoice = static_cast<s16>(i);
        mRegs[i].mPitch = NoteToPitch2(note, fine);
        mRegs[i].mDirty |= 4;
    }
}

void Libsnd::Flush()
{
    // SpuVmFlush: read the envelopes. A voice whose envelope has been 0 for the last 15 ticks is
    // free again (a one shot sample ended without a key off). Then write the registers that
    // changed, and the key offs and ons.
    mEnvelopeHistoryPos = (mEnvelopeHistoryPos + 1) & 0xF;
    mEnvelopeHistory[mEnvelopeHistoryPos] = 0;
    for (s32 i = 0; i < kNumVoices; i++)
    {
        mVoices[i].mEnvelope = mSpu.VoiceEnvelope(i);
        if (mVoices[i].mEnvelope == 0)
        {
            mEnvelopeHistory[mEnvelopeHistoryPos] |= 1u << i;
        }
    }
    u32 silent = 0xFFFFFFFF;
    for (s32 i = 0; i < 0xF; i++)
    {
        silent &= mEnvelopeHistory[i];
    }
    for (s32 i = 0; i < kNumVoices; i++)
    {
        if (silent & (1u << i))
        {
            mVoices[i].mKeyState = 0;
        }
    }

    mKeyOn &= ~mKeyOff;

    for (s32 i = 0; i < kNumVoices; i++)
    {
        VoiceRegs& r = mRegs[i];
        if (r.mDirty & 1)
        {
            mSpu.SetVoiceVolume(i, r.mVolLeft, r.mVolRight);
        }
        if (r.mDirty & 4)
        {
            mSpu.SetVoicePitch(i, r.mPitch);
        }
        if (r.mDirty & 8)
        {
            mSpu.SetVoiceSample(i, r.mVabId, r.mVag);
        }
        if (r.mDirty & 0x10)
        {
            mSpu.SetVoiceAdsr(i, r.mAdsr1, r.mAdsr2);
        }
        r.mDirty = 0;
    }
    mSpu.KeyOff(mKeyOff);
    mSpu.KeyOn(mKeyOn);
    mSpu.SetReverbVoices(mReverb);
    mKeyOff = 0;
    mKeyOn = 0;
}

// ---- the API ----

s32 Libsnd::VoKeyOn(s32 vabProg, s32 pitch, u16 left, u16 right)
{
    // SsVoKeyOn -> SpuVmSeKeyOn: the left/right volume as a velocity and a pan
    u16 pan;
    u16 vol;
    if (left == right)
    {
        pan = 0x40;
        vol = left;
    }
    else if (right < left)
    {
        vol = left;
        pan = static_cast<u16>((right * 0x40) / left);
    }
    else
    {
        vol = right;
        pan = static_cast<u16>(0x7F - (left * 0x40) / right);
    }
    return VmKeyOn(kSfxSeq, static_cast<s16>(vabProg >> 8), static_cast<s16>(vabProg & 0xFF), static_cast<u16>((pitch >> 8) & 0xFF), vol, pan);
}

s16 Libsnd::UtKeyOffV(s16 voice)
{
    if (voice < 0 || voice >= kNumVoices)
    {
        return -1;
    }
    KeyOffVoice(voice);
    return 0;
}

s16 Libsnd::UtChangePitch(s16 voice, s16 vabId, s16 prog, s16 oldNote, s16 /*oldFine*/, s16 newNote, s16 newFine)
{
    if (voice < 0 || voice >= kNumVoices)
    {
        return -1;
    }
    const Voice& v = mVoices[voice];
    if (v.mVabId != vabId || v.mProgram != prog || v.mNote != oldNote)
    {
        return -1;
    }
    VSetUp(v.mVabId, v.mProgram);
    mCur.mSeq = kSfxSeq;
    mCur.mVoice = voice;
    mCur.mTone = static_cast<u8>(v.mTone);
    mRegs[voice].mPitch = NoteToPitch2(static_cast<u16>(newNote), static_cast<u16>(newFine));
    mRegs[voice].mDirty |= 4;
    return 0;
}

void Libsnd::UtAllKeyOff()
{
    // SsUtAllKeyOff: every voice's registers are set back at once, its volume to 0 so it's
    // silent before the key off
    for (s32 i = 0; i < kNumVoices; i++)
    {
        Voice& v = mVoices[i];
        v.mAge = 0x18;
        v.mVag = 0xFF;
        v.mKeyState = 0;
        v.mPitch = 0;
        v.mEnvelope = 0;
        v.mSeq = 0xFF;
        v.mFakeProgram = 0;
        v.mProgram = 0;
        v.mTone = 0xFF;
        mSpu.SetVoiceSample(i, -1, 0);
        mSpu.SetVoicePitch(i, 0x1000);
        mSpu.SetVoiceAdsr(i, 0x80FF, 0x4000);
        mSpu.SetVoiceVolume(i, 0, 0);
        KeyOffVoice(i);
    }
}

// ---- SEQs ----

u8 Libsnd::ReadByte(Score& score)
{
    // Past the end of the data reads 0s, where libsnd would read whatever follows it
    if (score.mReadPos >= score.mEnd)
    {
        return 0;
    }
    return *score.mReadPos++;
}

void Libsnd::UpdateTicksPerTick(Score& score)
{
    // How many SEQ ticks a libsnd tick plays, or when that's under 1, how many libsnd ticks to
    // wait between SEQ ticks (mTickCounter counts them down)
    const s32 rate = mConfig.mTickRate;
    const s32 perMinute = score.mResolution * score.mTempo * 10;
    if (perMinute < rate * 60)
    {
        score.mTicksPerTick = score.mTickCounter = static_cast<s16>((rate * 600) / (score.mResolution * score.mTempo));
    }
    else
    {
        score.mTickCounter = -1;
        score.mTicksPerTick = static_cast<s16>(perMinute / (rate * 60));
        if (rate * 30 < perMinute % (rate * 60))
        {
            score.mTicksPerTick++;
        }
    }
}

s16 Libsnd::InitSoundSeq(s16 seq, s16 vabId, const u8* pSeq, u32 size)
{
    // _SsInitSoundSeq
    Score& score = mScores[seq];
    score.mVabId = vabId;
    score.mResolution = 0;
    score.mRpn1 = 0;
    score.mRpn2 = 0;
    score.mRpnCount = 0;
    score.mNrpnData = 0;
    score.mNrpn2 = 0;
    score.mNrpnCount = 0;
    score.mChannel = 0;
    score.mFirstDelta = 0;
    score.mElapsed = 0;
    score.mInitialTempo = 0;
    score.mInitialTicksPerTick = 0;
    score.mPlayCount = 0;
    score.mPlaying = 0;
    score.mDelta = 0;
    score.mLoopSet = 0;
    score.mLoopCount = 0;
    score.mNrpnLoopActive = 0;
    score.mRunningStatus = 0;
    score.mLastVelocity = 0x7F;
    score.mMutedChannels = 0;
    for (s32 ch = 0; ch < 16; ch++)
    {
        score.mPrograms[ch] = static_cast<u8>(ch);
        score.mPan[ch] = 0x40;
        score.mVol[ch] = 0x7F;
    }
    score.mTickCounter = 1;
    score.mBase = pSeq;
    score.mEnd = pSeq + size;
    score.mReadPos = pSeq;

    if (size < 15 || (pSeq[0] != 'S' && pSeq[0] != 'p'))
    {
        // "This is an old SEQ Data Format."
        return 0;
    }
    if (pSeq[7] != 1)
    {
        // "This is not SEQ Data."
        return -1;
    }
    score.mReadPos += 8;
    score.mResolution = static_cast<s16>(ReadByte(score) << 8);
    score.mResolution = static_cast<s16>(score.mResolution | ReadByte(score));
    s32 tempo = ReadByte(score) << 16;
    tempo |= ReadByte(score) << 8;
    tempo |= ReadByte(score);
    // Microseconds a quarter note to beats a minute, rounded to the nearest
    if (tempo <= 0)
    {
        return -1;
    }
    if (tempo / 2 < 60000000 % tempo)
    {
        score.mInitialTempo = 60000000 / tempo + 1;
    }
    else
    {
        score.mInitialTempo = 60000000 / tempo;
    }
    score.mTempo = score.mInitialTempo;
    score.mReadPos += 2; // The time signature
    score.mDelta = score.mFirstDelta = ReadDeltaValue(seq, 0);
    score.mNextSepPos = score.mReadPos;
    score.mLoopPos = score.mReadPos;
    if (score.mResolution <= 0)
    {
        return -1;
    }
    UpdateTicksPerTick(score);
    score.mInitialTicksPerTick = score.mTicksPerTick;
    return 0;
}

s16 Libsnd::SeqOpen(const u8* pSeq, u32 size, s16 vabId)
{
    if (mOpenSeqs == 0xFFFFFFFF)
    {
        return -1;
    }
    s16 seq = 0;
    while (mOpenSeqs & (1u << seq))
    {
        seq++;
    }
    mOpenSeqs |= 1u << seq;
    Score& score = mScores[seq];
    score.mData.assign(pSeq, pSeq + size);
    if (InitSoundSeq(seq, vabId, score.mData.data(), size) == -1)
    {
        // libsnd leaves it marked open
        return -1;
    }
    return seq;
}

void Libsnd::SeqClose(s16 seq)
{
    // _SsClose
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return;
    }
    SetSeqVol(seq, 0, 0, 1);
    SeqKeyOff(seq);
    mOpenSeqs &= ~(1u << seq);
    Score& score = mScores[seq];
    score.mFlags = 0;
    score.mSeqVolLeft = 0x7F;
    score.mSeqVolRight = 0x7F;
}

void Libsnd::SeqPlay(s16 seq, s8 mode, s16 count)
{
    // Snd_SetPlayMode
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return;
    }
    Score& score = mScores[seq];
    score.mFlags &= ~0x200u;
    score.mFlags &= ~4u;
    score.mRepeatCount = count;
    if (mode == 1)
    {
        score.mFlags |= 1;
        score.mPlayCount = 0;
        score.mPlaying = 1;
        SetSeqVol(seq, score.mSeqVolLeft, score.mSeqVolRight, 0);
    }
    else if (mode == 0)
    {
        score.mFlags |= 2;
    }
}

void Libsnd::SndStop(s16 seq, s16 sep)
{
    // _SsSndStop
    Score& score = mScores[seq];
    score.mFlags &= ~1u;
    score.mFlags &= ~2u;
    score.mFlags &= ~8u;
    score.mFlags |= 4;
    SeqKeyOff(static_cast<s16>(seq | (sep << 8)));
    score.mPlaying = 0;
    score.mElapsed = 0;
    score.mLoopSet = 0;
    score.mRpn1 = 0;
    score.mRpn2 = 0;
    score.mRpnCount = 0;
    score.mNrpnData = 0;
    score.mNrpn2 = 0;
    score.mNrpnCount = 0;
    score.mChannel = 0;
    score.mPlayCount = 0;
    score.mLoopCount = 0;
    score.mNrpnLoopActive = 0;
    score.mRunningStatus = 0;
    score.mDelta = score.mFirstDelta;
    score.mTempo = score.mInitialTempo;
    score.mTicksPerTick = score.mInitialTicksPerTick;
    score.mReadPos = score.mNextSepPos;
    score.mLoopPos = score.mNextSepPos;
    for (s32 i = 0; i < 16; i++)
    {
        score.mPrograms[i] = static_cast<u8>(i);
        score.mPan[i] = 0x40;
        score.mVol[i] = 0x7F;
    }
    score.mVolLeftCopy = 0x7F;
    score.mVolRightCopy = 0x7F;
}

void Libsnd::SeqStop(s16 seq)
{
    if (seq >= 0 && seq < kMaxSeqs)
    {
        SndStop(seq, 0);
    }
}

void Libsnd::SeqPause(s16 seq)
{
    if (seq >= 0 && seq < kMaxSeqs)
    {
        mScores[seq].mFlags |= 2;
    }
}

void Libsnd::SeqReplay(s16 seq)
{
    if (seq >= 0 && seq < kMaxSeqs)
    {
        mScores[seq].mFlags |= 8;
    }
}

void Libsnd::SeqSetVol(s16 seq, s16 left, s16 right)
{
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return;
    }
    switch (mConfig.mSsSeqSetVol)
    {
        case Config::SsSeqSetVolMode::eAlways:
            SetSeqVol(seq, static_cast<u16>(left), static_cast<u16>(right), 1);
            break;
        case Config::SsSeqSetVolMode::eWhilePlaying:
            if (mScores[seq].mFlags == 1)
            {
                SetSeqVol(seq, static_cast<u16>(left), static_cast<u16>(right), 1);
            }
            else
            {
                // Not clamped to 127
                mScores[seq].mSeqVolLeft = static_cast<u16>(left);
                mScores[seq].mSeqVolRight = static_cast<u16>(right);
            }
            break;
        case Config::SsSeqSetVolMode::eLaterNotes:
            SetSeqVol(seq, static_cast<u16>(left), static_cast<u16>(right), 0);
            break;
    }
}

s16 Libsnd::IsEos(s16 seq, s16 /*sep*/) const
{
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return 0;
    }
    return mScores[seq].mPlaying;
}

s32 Libsnd::ReadDeltaValue(s16 seq, s16 /*sep*/)
{
    // A MIDI variable length number, in SEQ ticks * 10
    Score& score = mScores[seq];
    u32 value = ReadByte(score);
    if (value == 0)
    {
        return 0;
    }
    if (value & 0x80)
    {
        value &= 0x7F;
        u8 b;
        do
        {
            b = ReadByte(score);
            value = (value << 7) + (b & 0x7F);
        }
        while ((b & 0x80) && score.mReadPos < score.mEnd);
    }
    const s32 ticks = static_cast<s32>(value * 10);
    score.mElapsed += ticks;
    return ticks;
}

void Libsnd::SeqPlayTick(s16 seq, s16 sep)
{
    // _SsSeqPlay: play the events due this tick
    Score& score = mScores[seq];
    if (score.mDelta - score.mTicksPerTick > 0)
    {
        if (score.mTickCounter > 0)
        {
            score.mTickCounter--;
        }
        else if (score.mTickCounter == 0)
        {
            score.mTickCounter = score.mTicksPerTick;
            score.mDelta--;
        }
        else
        {
            score.mDelta -= score.mTicksPerTick;
        }
    }
    else if (score.mDelta <= score.mTicksPerTick)
    {
        s32 played = score.mDelta;
        // A SEQ of nothing but 0 deltas would never end: libsnd hangs there
        s32 guard = 0x10000;
        do
        {
            do
            {
                GetSeqData(seq, sep);
            }
            while (score.mDelta == 0 && --guard > 0);
            played += score.mDelta;
        }
        while (played < score.mTicksPerTick && --guard > 0);
        score.mDelta = played - score.mTicksPerTick;
    }
}

void Libsnd::GetSeqData(s16 seq, s16 sep)
{
    // _SsGetSeqData: one MIDI event
    Score& score = mScores[seq];
    const u8 status = ReadByte(score);
    if (status & 0x80)
    {
        score.mChannel = status & 0xF;
        switch (status & 0xF0)
        {
            case 0x90:
            {
                score.mRunningStatus = 0x90;
                const u8 note = ReadByte(score);
                const u8 velocity = ReadByte(score);
                score.mDelta = ReadDeltaValue(seq, sep);
                NoteOn(seq, sep, note, velocity);
                return;
            }
            case 0xB0:
                score.mRunningStatus = 0xB0;
                SetControlChange(seq, sep, ReadByte(score));
                return;
            case 0xC0:
                score.mRunningStatus = 0xC0;
                score.mPrograms[score.mChannel] = ReadByte(score);
                score.mDelta = ReadDeltaValue(seq, sep);
                return;
            case 0xE0:
            {
                score.mRunningStatus = 0xE0;
                score.mReadPos++; // The bend's low 7 bits
                const u8 bend = ReadByte(score);
                PitchBend(static_cast<s16>(seq | (sep << 8)), score.mVabId, score.mPrograms[score.mChannel], bend);
                score.mDelta = ReadDeltaValue(seq, sep);
                return;
            }
            case 0xF0:
                score.mRunningStatus = 0xFF;
                GetMetaEvent(seq, sep, ReadByte(score));
                return;
            default:
                // libsnd doesn't read anything else: the data after it is read as events
                return;
        }
    }

    switch (score.mRunningStatus)
    {
        case 0x90:
        {
            const u8 velocity = ReadByte(score);
            score.mDelta = ReadDeltaValue(seq, sep);
            NoteOn(seq, sep, status, velocity);
            return;
        }
        case 0xB0:
            SetControlChange(seq, sep, status);
            return;
        case 0xC0:
            score.mPrograms[score.mChannel] = status;
            score.mDelta = ReadDeltaValue(seq, sep);
            return;
        case 0xE0:
        {
            const u8 bend = ReadByte(score);
            PitchBend(static_cast<s16>(seq | (sep << 8)), score.mVabId, score.mPrograms[score.mChannel], bend);
            score.mDelta = ReadDeltaValue(seq, sep);
            return;
        }
        case 0xFF:
            GetMetaEvent(seq, sep, status);
            return;
        default:
            return;
    }
}

void Libsnd::NoteOn(s16 seq, s16 sep, u8 note, u8 velocity)
{
    // _SsNoteOn: velocity 0 is a note off
    Score& score = mScores[seq];
    const u8 channel = score.mChannel;
    const u8 pan = score.mPan[channel];
    if (((score.mMutedChannels >> channel) & 1) || (mConfig.mNoteOnSkippedAtSeqVolZero && score.mSeqVolLeft == 0))
    {
        return;
    }
    const s16 seqSep = static_cast<s16>(seq | (sep << 8));
    if (velocity)
    {
        VmKeyOn(seqSep, score.mVabId, score.mPrograms[channel], note, velocity, pan);
        score.mLastVelocity = velocity;
    }
    else
    {
        VmKeyOff(seqSep, score.mVabId, score.mPrograms[channel], note);
    }
}

void Libsnd::SetControlChange(s16 seq, s16 sep, u8 control)
{
    // _SsSetControlChange
    Score& score = mScores[seq];
    const u8 channel = score.mChannel;
    const u8 value = ReadByte(score);
    const s16 seqSep = static_cast<s16>(seq | (sep << 8));
    switch (control)
    {
        case 0x00: // Bank select: the VAB
            score.mVabId = value;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
        case 0x07: // Volume
            SetVol(seqSep, score.mVabId, score.mPrograms[channel], value, score.mPan[channel]);
            score.mVol[channel] = value;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
        case 0x0A: // Pan
            SetVol(seqSep, score.mVabId, score.mPrograms[channel], static_cast<u16>(score.mVol[channel]), value);
            score.mPan[channel] = value;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
        case 0x62:
            ContNrpn1(seq, sep, value);
            break;
        case 0x63:
            ContNrpn2(seq, sep, value);
            break;
        case 0x64:
            score.mRpn1 = value;
            score.mRpnCount++;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
        case 0x65:
            score.mRpn2 = value;
            score.mRpnCount++;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
        default:
            // The games' SEQs use no other controllers (expression 0x0B, damper 0x40, portamento
            // 0x41, reverb 0x5B, data entry 0x06, reset 0x79 would go here)
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
    }
}

void Libsnd::ContNrpn1(s16 seq, s16 sep, u8 value)
{
    // _SsContNrpn1: the loop count, after a loop start
    Score& score = mScores[seq];
    if (score.mLoopSet == 1 && score.mNrpnLoopActive == 0)
    {
        score.mLoopCount = value;
        score.mNrpnLoopActive = 1;
    }
    else if (score.mNrpn2 != 30 && score.mNrpn2 != 20)
    {
        score.mNrpnData = value;
        score.mNrpnCount++;
    }
    score.mDelta = ReadDeltaValue(seq, sep);
}

void Libsnd::ContNrpn2(s16 seq, s16 sep, u8 value)
{
    // _SsContNrpn2: 20 = loop start, 30 = loop end
    Score& score = mScores[seq];
    switch (value)
    {
        case 20:
            score.mNrpn2 = value;
            score.mLoopSet = 1;
            score.mDelta = ReadDeltaValue(seq, sep);
            score.mLoopPos = score.mReadPos;
            break;
        case 30:
            score.mNrpn2 = value;
            if (score.mLoopCount == 0)
            {
                score.mNrpnLoopActive = 0;
                score.mDelta = ReadDeltaValue(seq, sep);
            }
            else if (score.mLoopCount < 0x7F)
            {
                score.mLoopCount--;
                score.mDelta = ReadDeltaValue(seq, sep);
                if (score.mLoopCount != 0)
                {
                    score.mReadPos = score.mLoopPos;
                }
                else
                {
                    score.mNrpnLoopActive = 0;
                }
            }
            else
            {
                ReadDeltaValue(seq, sep);
                score.mDelta = 0;
                score.mReadPos = score.mLoopPos;
            }
            break;
        default:
            score.mNrpn2 = value;
            score.mNrpnCount++;
            score.mDelta = ReadDeltaValue(seq, sep);
            break;
    }
}

void Libsnd::GetMetaEvent(s16 seq, s16 sep, u8 type)
{
    // _SsGetMetaEvent
    Score& score = mScores[seq];
    switch (type)
    {
        case 0x51: // Tempo
        {
            s32 tempo = ReadByte(score) << 16;
            tempo |= ReadByte(score) << 8;
            tempo |= ReadByte(score);
            if (tempo <= 0)
            {
                tempo = 1;
            }
            score.mTempo = 60000000 / tempo;
            if (score.mTempo <= 0)
            {
                score.mTempo = 1;
            }
            UpdateTicksPerTick(score);
            score.mDelta = ReadDeltaValue(seq, sep);
            return;
        }
        case 0x2F: // End of track
        {
            score.mPlayCount++;
            if (score.mRepeatCount == 0 || score.mPlayCount < score.mRepeatCount)
            {
                // Again from the start (0 = forever)
                score.mElapsed = 0;
                score.mLoopSet = 0;
                score.mDelta = 0;
                score.mReadPos = score.mNextSepPos;
                if (score.mRepeatCount != 0)
                {
                    score.mLoopPos = score.mNextSepPos;
                }
                return;
            }
            score.mFlags &= ~1u;
            score.mFlags &= ~8u;
            score.mFlags &= ~2u;
            score.mFlags |= 0x200;
            score.mFlags |= 4;
            score.mPlaying = 0;
            score.mLoopPos = score.mNextSepPos;
            SeqKeyOff(static_cast<s16>(seq | (sep << 8)));
            score.mDelta = score.mTicksPerTick;
            return;
        }
        default:
            return;
    }
}

void Libsnd::Tick()
{
    // SsSeqCalledTbyT
    Flush();
    for (s16 seq = 0; seq < kMaxSeqs; seq++)
    {
        if (!(mOpenSeqs & (1u << seq)))
        {
            continue;
        }
        Score& score = mScores[seq];
        if (score.mFlags & 1)
        {
            SeqPlayTick(seq, 0);
        }
        if (score.mFlags & 2)
        {
            // _SsSndPause
            SeqKeyOff(seq);
            score.mPlaying = 0;
            score.mFlags &= ~2u;
        }
        if (score.mFlags & 8)
        {
            // _SsSndReplay
            score.mPlaying = 1;
            score.mFlags &= ~8u;
        }
        if (score.mFlags & 4)
        {
            SndStop(seq, 0);
            score.mFlags = 0;
        }
    }
}

void Libsnd::NotePitches(s16 vabId, s32 prog, s32 note, bool bent, std::array<u32, 256>& maxPitch) const
{
    if (vabId < 0 || vabId >= kMaxVabs || !mVabs[vabId].mUsed || prog < 0 || prog >= mVabs[vabId].mMaxPrograms)
    {
        return;
    }
    const Vab& vab = mVabs[vabId];
    const ProgAtr& p = vab.mProgs[prog];
    for (s32 i = 0; i < p.mTones && i < 16; i++)
    {
        const size_t idx = p.mFakeProgram * 16u + i;
        if (idx >= vab.mTones.size())
        {
            break;
        }
        const VagAtr& tone = vab.mTones[idx];
        if (note < tone.mMin || note > tone.mMax || tone.mVag <= 0 || tone.mVag > 255)
        {
            continue;
        }
        const s32 semitones = note + (bent ? tone.mBendMax : 0) + 1 + 60 - tone.mCentre;
        u32& m = maxPitch[static_cast<size_t>(tone.mVag)];
        m = std::max(m, PitchFromTable(semitones, 15));
    }
}

void Libsnd::SfxPitches(s32 vabProg, s32 pitch, std::array<u32, 256>& maxPitch) const
{
    // Two semitones over for the games' pitch variations (SsUtChangePitch)
    NotePitches(static_cast<s16>(vabProg >> 8), vabProg & 0x7F, ((pitch >> 8) & 0x7F) + 2, false, maxPitch);
}

void Libsnd::SeqPitches(const u8* pSeq, u32 size, s16 vabId, std::array<u32, 256>& maxPitch) const
{
    // Every note the SEQ plays, with the program of its channel then
    if (!pSeq || size < 16)
    {
        return;
    }
    std::array<u8, 16> programs = {};
    for (u8 i = 0; i < 16; i++)
    {
        programs[i] = i;
    }
    std::array<bool, 16> bent = {};
    // Notes per channel and program, then the pitches once the bends are known
    std::vector<std::array<u8, 3>> notes;
    u32 pos = 15;
    auto readDelta = [&]()
    {
        while (pos < size && (pSeq[pos++] & 0x80))
        {
        }
    };
    readDelta();
    u8 status = 0;
    while (pos < size)
    {
        u8 b = pSeq[pos];
        if (b & 0x80)
        {
            status = b;
            pos++;
        }
        const u8 channel = status & 0xF;
        switch (status & 0xF0)
        {
            case 0x90:
                if (pos + 1 < size && pSeq[pos + 1] != 0)
                {
                    notes.push_back({channel, programs[channel], pSeq[pos]});
                }
                pos += 2;
                break;
            case 0xB0:
                pos += 2;
                break;
            case 0xC0:
                if (pos < size)
                {
                    programs[channel] = pSeq[pos];
                }
                pos += 1;
                break;
            case 0xE0:
                bent[channel] = true;
                pos += 2;
                break;
            case 0xF0:
                if (pos < size && pSeq[pos] == 0x2F)
                {
                    pos = size;
                    continue;
                }
                pos += pSeq[pos] == 0x51 ? 4 : 1;
                break;
            default:
                // What libsnd can't read either
                pos = size;
                continue;
        }
        readDelta();
    }
    for (const std::array<u8, 3>& n : notes)
    {
        NotePitches(vabId, n[1], n[2], bent[n[0]], maxPitch);
    }
}

Libsnd::VoiceInfo Libsnd::GetVoiceInfo(s32 voice) const
{
    VoiceInfo info;
    if (voice >= 0 && voice < kNumVoices)
    {
        const Voice& v = mVoices[voice];
        info.mSeq = v.mSeq;
        info.mVabId = v.mVabId;
        info.mProgram = v.mProgram;
        info.mNote = v.mNote;
        info.mPriority = static_cast<u8>(v.mPriority);
        info.mKeyed = v.mKeyState != 0;
        info.mAge = v.mAge;
        info.mEnvelope = v.mEnvelope;
    }
    return info;
}
