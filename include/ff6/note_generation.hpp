#pragma once
#include "ff6/driver_updates.hpp"

namespace ff6 {
using NotePitchTable = std::array<std::uint16_t, 13>;
NotePitchTable decode_note_pitches(ByteView bytes);
NotePitchTable load_note_pitches(const ContentPack& pack);
std::uint16_t note_frequency(const NotePitchTable& pitches, std::uint8_t note,
                             std::array<std::uint8_t, 2> tuning, std::uint8_t detune);

struct DriverNoteState {
    DriverPitchMotion pitch;
    std::uint8_t note_index = 0;
    bool hold = false;
    bool operator==(const DriverNoteState&) const = default;
};
struct DriverNoteQueues {
    std::uint8_t starts = 0, music_started = 0;
    bool operator==(const DriverNoteQueues&) const = default;
};
struct DriverNoteContext {
    SequenceChannel channel;
    std::uint8_t effect_owners = 0;
};
struct DriverSampleRequest {
    // Resolved by the native owner against its loaded sample identities.
    std::uint8_t slot = 0;
    DriverEnvelope envelope;
    bool operator==(const DriverSampleRequest&) const = default;
};
struct DriverNoteResult {
    std::optional<DriverSampleRequest> sample;
    // Note handoff through return, excluding the nested lookahead body/return.
    // Add ScriptLookahead::work for complete duration; its CALL is included here.
    std::uint16_t work = 0;
    std::optional<std::uint16_t> sample_offset, lookahead_offset;
};
// Duration/flow were handled by step_script. Real notes and ties require the
// result of inspect_next_note; rests require no lookahead. Requests are at
// driver time, not effective synthesis events. Invalid input is atomic.
DriverNoteResult apply_driver_note(std::uint8_t code,
                                   const std::optional<ScriptLookahead>& lookahead,
                                   const DriverNoteContext& context, const NotePitchTable& pitches,
                                   DriverNoteState& note, DriverChannelControls& channel,
                                   DriverGlobalControls& global, DriverNoteQueues& queues);
} // namespace ff6
