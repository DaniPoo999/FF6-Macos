#pragma once
#include "ff6/driver_controls.hpp"

namespace ff6 {
struct DriverPitchMotion {
    std::uint16_t base = 0;
    std::uint8_t fraction = 0;
    // Wrapped signed 16.8 increment, bounded to 24 bits.
    std::uint32_t step = 0;
    bool operator==(const DriverPitchMotion&) const = default;
};
struct DriverOutputPolicy {
    SequenceChannel channel;
    bool center_pan = false, muted = false, system_effect = false;
    std::uint8_t music_volume = 0, effect_volume = 0, effect_pan = 128;
    std::uint8_t music_pitch_modifier = 0;
};
struct DriverVoiceRequests {
    std::optional<StereoGain> volume;
    // Driver coefficient before sample-time narrowing to the fourteen-bit
    // playback range. No device registers or host audio clock enter this API.
    std::optional<std::uint16_t> pitch;
    // Work around requested gain/pitch calculations, in integer sound ticks.
    // With no requests, before_first is the whole operation. Completion is
    // before the caller return; calculation/publication work is separate.
    struct Work {
        std::uint16_t before_first = 0, between = 0, after_last = 0;
        bool operator==(const Work&) const = default;
    } work;
    // Calculated with the values above, after oscillator advancement. A later
    // publisher consumes these without consulting or advancing driver state.
    std::uint16_t gain_work = 0, pitch_work = 0;
    bool operator==(const DriverVoiceRequests&) const = default;
};
struct DriverPitchPreparation {
    std::uint16_t coefficient = 0;
    std::uint16_t publication_delay = 0;
};
DriverPitchPreparation prepare_driver_pitch(const DriverOutputPolicy& policy,
                                            const DriverChannelControls& channel,
                                            const DriverPitchMotion& pitch);
struct DriverGainPreparation {
    std::uint8_t level = 0, left_weight = 128;
    bool muted = false;
    // Integer sound ticks to the publication stage, excluding publication itself.
    std::uint16_t publication_delay = 0;
    StereoGain stereo() const;
};
// Computes both the native mix and its game-specific work duration. center_pan
// means FFVI's mono policy; the unsupported waveform-debug path is not modeled.
DriverGainPreparation prepare_driver_gain(const DriverOutputPolicy& policy,
                                          const DriverChannelControls& channel,
                                          const DriverGlobalControls& global);
// Call these at the separate logical stages established in SEQUENCING.md.
// These functions do not calculate new-note pitch or advance a synthesis clock.
struct DriverSongWork {
    // Song-wide advancement in 1/64-sample ticks, before the outer return.
    // Even inactive ramps consume selection work; the filter uses one counter.
    std::uint16_t tempo = 0, echo_volume = 0, echo_feedback = 0, filter = 0;
    std::uint32_t duration() const {
        return std::uint32_t{tempo} + echo_volume + echo_feedback + filter;
    }
    bool operator==(const DriverSongWork&) const = default;
};
DriverSongWork advance_driver_song(DriverGlobalControls& global);
struct DriverChannelWork {
    // Aggregate native advancement stages, in 1/64-sample sound ticks.
    // Completion excludes the outer return and caller/traversal work.
    std::uint16_t delays = 0, volume = 0, pan = 0, pitch = 0;
    std::uint16_t sweep_position = 0, sweep_period = 0;
    std::uint32_t duration() const {
        return std::uint32_t{delays} + volume + pan + pitch + sweep_position + sweep_period;
    }
    bool operator==(const DriverChannelWork&) const = default;
};
DriverChannelWork advance_driver_channel(SequenceChannel identity, DriverChannelControls& channel,
                                         DriverPitchMotion& pitch, DriverGlobalControls& global);
DriverVoiceRequests update_driver_voice(const DriverOutputPolicy& policy,
                                        DriverChannelControls& channel,
                                        const DriverPitchMotion& pitch,
                                        DriverGlobalControls& global);
// Also used when resuming a song, without advancing any oscillator or dirty set.
std::uint16_t driver_frequency_request(const DriverOutputPolicy& policy,
                                       const DriverChannelControls& channel,
                                       const DriverPitchMotion& pitch);
} // namespace ff6
