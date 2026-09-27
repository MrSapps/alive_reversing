#pragma once

#include "../../relive_lib/Types.hpp"
#include <string>
#include <vector>

// Converts the games' PSX SEQ files to Standard MIDI Files (type 0) and back.
//
// A SEQ is SMF track data behind a "pQES" header (resolution, tempo, time signature). The SMF
// gets the resolution as its division and a track that starts with the tempo and time signature
// at delta 0, then the SEQ's events unchanged. Converting back skips those leading meta events,
// so the engine's SEQ player starts (and loops back to) the same event it did with the SEQ. See
// SOUND_FORMATS.md.
class SeqMidi final
{
public:
    // False (with the reason in error) if seq isn't a SEQ
    static bool SeqToSmf(const std::vector<u8>& seq, std::vector<u8>& smf, std::string& error);

    // A SEQ for the engine's SEQ player. False (with the reason in error) if smf isn't a type 0
    // (or single track type 1) SMF.
    static bool SmfToSeq(const std::vector<u8>& smf, std::vector<u8>& seq, std::string& error);

    static constexpr size_t kSeqHeaderSize = 15;
};
