#pragma once
#include <array>
#include <cstdint>

namespace ff6 {
// Logical service clocks. Call music and effects at their separate stages;
// commands between them may replace a song or change channel ownership.
struct SequenceTick {
    bool advance_sequences = false;
    bool advance_control_envelopes = false;
    bool operator==(const SequenceTick&) const = default;
};
SequenceTick begin_sequence_tick(std::uint8_t& control_countdown, bool paused);
struct SequenceServiceSelection {
    SequenceTick tick;
    // Relative to accepted service entry. The buffered loop ends after its last
    // publication; status_end is before pause/countdown selection.
    std::uint16_t buffered_loop_end = 0, status_end = 0;
    // Ends at slow-ramp entry when due, script entry otherwise, or next idle
    // poll entry when paused. Slow-ramp/script bodies are separate.
    std::uint16_t work = 0;
};
SequenceServiceSelection begin_sequence_service(std::uint8_t& control_countdown, bool paused,
                                                bool echo_enabled, bool fade_status);
std::uint8_t adjusted_music_tempo(std::uint8_t tempo, std::uint8_t modifier);
struct SequencePhaseResult {
    bool due = false;
    // Phase entry through traversal preparation or the phase's skipped endpoint.
    std::uint16_t work = 0;
};
SequencePhaseResult advance_music_phase(std::uint8_t& phase, std::uint8_t tempo,
                                        std::uint8_t modifier, bool forced);
SequencePhaseResult advance_effects_phase(std::uint8_t& phase);

enum class SequenceKind : std::uint8_t { music, effect };
enum class SequenceAction : std::uint8_t { sustain, execute_commands, release };
struct SequenceWait {
    std::uint8_t remaining = 0;
    bool music_playing = false;
    bool release_pending = false;
    bool operator==(const SequenceWait&) const = default;
};
// owns_voice distinguishes music shadowed by an effect from audible music.
// The returned release action describes note completion even when that music
// must not send a release to the physical voice currently occupied by an effect.
struct SequenceWaitResult {
    SequenceAction action;
    // Selected channel entry through script entry or envelope-call selection.
    // Script bodies and the envelope CALL/return are separate.
    std::uint16_t work = 0;
};
SequenceWaitResult advance_sequence_wait(SequenceWait& wait, SequenceKind kind, bool held,
                                         bool owns_voice);

struct SequenceChannel {
    SequenceKind kind = SequenceKind::music;
    std::uint8_t voice = 0;
    bool operator==(const SequenceChannel&) const = default;
};
struct SequenceOrder {
    std::array<SequenceChannel, 12> channels{};
    std::uint8_t count = 0;
};
// Each mask's bit is the semantic voice index. Effects occupy voices 4..7;
// a lower bit in effect_voices is rejected before any result is produced.
SequenceOrder music_sequence_order(std::uint8_t enabled_music);
SequenceOrder effects_sequence_order(std::uint8_t effect_voices);
struct ScriptTraversalPlan {
    SequenceOrder order;
    std::array<std::uint16_t, 8> before{};
    // Music ends at song advancement entry (including its CALL); effects end
    // after the script-pass return. Selected channel bodies are separate.
    std::uint16_t after = 0;
};
ScriptTraversalPlan script_traversal_plan(SequenceKind kind, std::uint8_t enabled);
SequenceOrder voice_update_order(std::uint8_t enabled_music, std::uint8_t playing_music,
                                 std::uint8_t effect_voices);
struct VoiceUpdatePlan {
    SequenceOrder order;
    // Sound ticks before each update, measured from pass entry or the preceding
    // update's completion. after includes final dirty cleanup and poll transfer.
    std::array<std::uint16_t, 12> before{};
    std::uint16_t after = 0;
};
VoiceUpdatePlan voice_update_plan(std::uint8_t enabled_music, std::uint8_t playing_music,
                                  std::uint8_t effect_voices);
} // namespace ff6
