#include "../stdafx.h"
#include "SeqMidi.hpp"
#include <algorithm>

static constexpr u32 kDefaultTempo = 500000; // 120 BPM

static u32 ReadBE(const std::vector<u8>& data, size_t pos, size_t bytes)
{
    u32 v = 0;
    for (size_t i = 0; i < bytes; i++)
    {
        v = (v << 8) | data[pos + i];
    }
    return v;
}

static void PutBE(std::vector<u8>& out, u32 value, size_t bytes)
{
    for (size_t i = 0; i < bytes; i++)
    {
        out.push_back(static_cast<u8>(value >> (8 * (bytes - 1 - i))));
    }
}

// Reads a variable length number (at most 4 bytes, as the engine's MIDI_Read_Var_Len does)
static bool ReadVarLen(const std::vector<u8>& data, size_t& pos, size_t end, u32& value)
{
    value = 0;
    for (s32 i = 0; i < 4; i++)
    {
        if (pos >= end)
        {
            return false;
        }
        const u8 b = data[pos++];
        value = (value << 7) | (b & 0x7F);
        if (!(b & 0x80))
        {
            return true;
        }
    }
    return true;
}

// Where the event stream in data[start, end) ends: just after its end of track meta event, the
// way the engine's SEQ player reads events. end, and found false, if there isn't one.
static size_t FindEndOfTrack(const std::vector<u8>& data, size_t start, size_t end, bool& found)
{
    found = false;
    size_t pos = start;
    u8 runningStatus = 0;
    while (pos < end)
    {
        u32 delta = 0;
        if (!ReadVarLen(data, pos, end, delta) || pos >= end)
        {
            return end;
        }

        u8 status = data[pos];
        if (status >= 0xF0)
        {
            pos++;
            if (status == 0xFF)
            {
                if (pos >= end)
                {
                    return end;
                }
                const u8 type = data[pos++];
                u32 len = 0;
                if (!ReadVarLen(data, pos, end, len))
                {
                    return end;
                }
                pos += len;
                if (type == 0x2F)
                {
                    found = pos <= end;
                    return std::min(pos, end);
                }
            }
            else if (status == 0xF0 || status == 0xF7)
            {
                u32 len = 0;
                if (!ReadVarLen(data, pos, end, len))
                {
                    return end;
                }
                pos += len;
            }
            continue;
        }

        if (status >= 0x80)
        {
            runningStatus = status;
            pos++;
        }
        else if (!runningStatus)
        {
            return end;
        }
        const u8 type = runningStatus & 0xF0;
        pos += (type == 0xC0 || type == 0xD0) ? 1 : 2;
    }
    return end;
}

bool SeqMidi::SeqToSmf(const std::vector<u8>& seq, std::vector<u8>& smf, std::string& error)
{
    if (seq.size() < kSeqHeaderSize || seq[0] != 'p' || seq[1] != 'Q' || seq[2] != 'E' || seq[3] != 'S')
    {
        error = "not a SEQ";
        return false;
    }

    const u16 resolution = static_cast<u16>(ReadBE(seq, 8, 2));
    const u32 tempo = ReadBE(seq, 10, 3);
    const u8 timeSigA = seq[13];
    const u8 timeSigB = seq[14];
    bool foundEnd = false;
    const size_t eventsEnd = FindEndOfTrack(seq, kSeqHeaderSize, seq.size(), foundEnd);

    std::vector<u8> track;
    const u8 tempoEvent[] = {0x00, 0xFF, 0x51, 0x03, static_cast<u8>(tempo >> 16), static_cast<u8>(tempo >> 8), static_cast<u8>(tempo)};
    track.insert(track.end(), tempoEvent, tempoEvent + sizeof(tempoEvent));
    // The SEQ's two time signature bytes as the numerator and denominator
    const u8 timeSigEvent[] = {0x00, 0xFF, 0x58, 0x04, timeSigA, timeSigB, 24, 8};
    track.insert(track.end(), timeSigEvent, timeSigEvent + sizeof(timeSigEvent));
    track.insert(track.end(), seq.begin() + kSeqHeaderSize, seq.begin() + eventsEnd);
    if (!foundEnd)
    {
        // No end of track, which a SMF track must have. The engine never read past the SEQ.
        const u8 endOfTrack[] = {0x00, 0xFF, 0x2F, 0x00};
        track.insert(track.end(), endOfTrack, endOfTrack + sizeof(endOfTrack));
    }

    smf.clear();
    smf.insert(smf.end(), {'M', 'T', 'h', 'd'});
    PutBE(smf, 6, 4);
    PutBE(smf, 0, 2); // format 0
    PutBE(smf, 1, 2); // 1 track
    PutBE(smf, resolution, 2);
    smf.insert(smf.end(), {'M', 'T', 'r', 'k'});
    PutBE(smf, static_cast<u32>(track.size()), 4);
    smf.insert(smf.end(), track.begin(), track.end());
    return true;
}

bool SeqMidi::SmfToSeq(const std::vector<u8>& smf, std::vector<u8>& seq, std::string& error)
{
    if (smf.size() < 14 || smf[0] != 'M' || smf[1] != 'T' || smf[2] != 'h' || smf[3] != 'd')
    {
        error = "not a MIDI file";
        return false;
    }
    const u32 headerLen = ReadBE(smf, 4, 4);
    const u32 format = ReadBE(smf, 8, 2);
    const u32 tracks = ReadBE(smf, 10, 2);
    const u16 division = static_cast<u16>(ReadBE(smf, 12, 2));
    if (headerLen < 6 || (format != 0 && !(format == 1 && tracks == 1)))
    {
        error = "only type 0 (or single track) MIDI files are supported";
        return false;
    }

    // The first MTrk
    size_t pos = 8 + static_cast<size_t>(headerLen);
    size_t trackStart = 0;
    size_t trackEnd = 0;
    while (pos + 8 <= smf.size())
    {
        const u32 len = ReadBE(smf, pos + 4, 4);
        if (smf[pos] == 'M' && smf[pos + 1] == 'T' && smf[pos + 2] == 'r' && smf[pos + 3] == 'k')
        {
            trackStart = pos + 8;
            trackEnd = std::min(trackStart + len, smf.size());
            break;
        }
        pos += 8 + static_cast<size_t>(len);
    }
    if (!trackStart)
    {
        error = "no track";
        return false;
    }

    // The meta events at the start of the track: tempo and time signature go in the header, the
    // events start at the first other event
    u32 tempo = kDefaultTempo;
    u8 timeSigA = 4;
    u8 timeSigB = 2;
    pos = trackStart;
    while (pos < trackEnd)
    {
        size_t p = pos;
        u32 delta = 0;
        if (!ReadVarLen(smf, p, trackEnd, delta) || delta != 0 || p + 1 >= trackEnd || smf[p] != 0xFF || smf[p + 1] == 0x2F)
        {
            break;
        }
        const u8 type = smf[p + 1];
        p += 2;
        u32 len = 0;
        if (!ReadVarLen(smf, p, trackEnd, len) || p + len > trackEnd)
        {
            break;
        }
        if (type == 0x51 && len >= 3)
        {
            tempo = ReadBE(smf, p, 3);
        }
        else if (type == 0x58 && len >= 2)
        {
            timeSigA = smf[p];
            timeSigB = smf[p + 1];
        }
        pos = p + len;
    }

    seq.clear();
    seq.insert(seq.end(), {'p', 'Q', 'E', 'S'});
    PutBE(seq, 1, 4); // version
    PutBE(seq, division, 2);
    PutBE(seq, tempo, 3);
    seq.push_back(timeSigA);
    seq.push_back(timeSigB);
    seq.insert(seq.end(), smf.begin() + pos, smf.begin() + trackEnd);
    return true;
}
