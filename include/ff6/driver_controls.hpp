#pragma once
#include "ff6/music_scripts.hpp"
#include "ff6/synthesis.hpp"

namespace ff6 {
// Eight fractional bits. Step is a wrapped signed increment, not host signed overflow.
struct DriverRamp {
    std::uint16_t value = 0, step = 0;
    std::uint8_t remaining = 0;
    bool operator==(const DriverRamp&) const = default;
};
struct DriverOscillator {
    std::uint8_t delay = 0, delay_remaining = 0, period = 0, period_remaining = 0;
    std::uint8_t shape = 0, gain_remaining = 0;
    std::uint16_t maximum_step = 0, step = 0, position = 0;
    bool operator==(const DriverOscillator&) const = default;
};
struct DriverPanSweep {
    std::uint8_t period = 0, remaining = 0, shape = 0;
    std::uint16_t step = 0, position = 0;
    bool operator==(const DriverPanSweep&) const = default;
};
struct DriverEnvelope {
    std::uint8_t attack = 0, decay = 0, sustain_level = 0, sustain_rate = 0;
    bool enabled = false;
    bool operator==(const DriverEnvelope&) const = default;
};
struct DriverChannelControls {
    DriverRamp volume, pan;
    DriverOscillator vibrato, tremolo;
    DriverPanSweep pan_sweep;
    std::uint16_t vibrato_pitch_offset = 0;
    std::uint8_t pitch_slide_remaining = 0, pitch_slide_offset = 0;
    std::uint8_t octave = 0, transpose = 0, detune = 0, sample_slot = 0;
    std::array<std::uint8_t, 2> tuning{};
    DriverEnvelope envelope;
    bool slur = false, slurred_note = false, drum_roll = false;
    bool operator==(const DriverChannelControls&) const = default;
};
// Bits name semantic voices. A selected set is refreshed by its own commands.
struct DriverRouting {
    std::uint8_t music = 0, effects = 0, selected = 0;
    bool operator==(const DriverRouting&) const = default;
};
struct DriverFilterTap {
    std::uint16_t value = 0, step = 0;
    bool operator==(const DriverFilterTap&) const = default;
};
struct DriverGlobalControls {
    DriverRamp tempo, echo_volume, echo_feedback;
    std::array<DriverFilterTap, 8> filter{};
    std::uint8_t filter_remaining = 0, song_volume = 0, output_code = 0;
    std::uint8_t volume_dirty = 0, ignore_song_volume = 0;
    std::uint8_t pitch_dirty = 0;
    DriverRouting echo, noise, modulation;
    std::uint8_t music_noise_rate = 0, effects_noise_rate = 0, selected_noise_rate = 0;
    bool operator==(const DriverGlobalControls&) const = default;
};
struct DriverSampleDefaults {
    std::uint8_t slot = 0;
    std::array<std::uint8_t, 2> tuning{};
    DriverEnvelope envelope;
};
DriverSampleDefaults driver_sample_defaults(std::uint8_t slot, const SampleAsset& asset);
struct DriverControlContext {
    SequenceChannel channel;
    std::uint8_t effect_owners = 0;
    // Required for sample/default-envelope commands only. Loading/retaining
    // slots belongs to the song owner, not this command operation.
    std::optional<DriverSampleDefaults> sample_defaults;
};
struct DriverControlResult {
    // Immediate driver request, not a sample-time effective update. Music
    // shadowed by an effect changes stored envelope but emits no request.
    std::optional<DriverEnvelope> envelope;
    // Handler entry through natural return, excluding deferred byte-reader
    // bodies. Add ScriptStep::operand_work for complete handler duration.
    std::uint16_t work = 0;
    // Relative start of envelope publication, present exactly when requested.
    std::optional<std::uint16_t> envelope_offset;
    bool operator==(const DriverControlResult&) const = default;
};
bool is_driver_control(MusicCommand command);
// Rejects flow commands, malformed operands and invalid context atomically.
DriverControlResult apply_driver_control(const ScriptInstruction& instruction,
                                         const DriverControlContext& context,
                                         const EchoFilterPresets& filters,
                                         DriverChannelControls& channel,
                                         DriverGlobalControls& global);
} // namespace ff6
