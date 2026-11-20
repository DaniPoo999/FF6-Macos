#pragma once
#include "ff6/note_generation.hpp"
#include <span>

namespace ff6 {
enum class PlaybackCommand : std::uint8_t {
    volume,
    music_volume,
    effect_volume,
    effect_pan,
    tempo,
    pitch,
    mono,
    mute,
    conditional,
    stop,
    stop_music,
    stop_effects,
    pause
};
struct PlaybackControls {
    DriverRamp music_volume, effect_volume, effect_pan, tempo, pitch;
    bool mono = false, fade_status = false;
    std::uint8_t muted = 0, conditional = 0;
    bool operator==(const PlaybackControls&) const = default;
};
struct PlaybackOwnership {
    std::uint8_t music = 0, game_effects = 0, system_effects = 0;
    std::uint8_t paused_music = 0, paused_effects = 0;
    std::optional<std::uint8_t> song, suspended_song;
    bool paused() const {
        return paused_music != 0 || paused_effects != 0;
    }
    std::uint8_t effects() const {
        return static_cast<std::uint8_t>(game_effects | system_effects);
    }
    bool operator==(const PlaybackOwnership&) const = default;
};
struct PlaybackRequests {
    // true selects pause_hold; false forces ADSR, rather than restoring an old mode.
    std::optional<bool> envelope_hold;
    bool silence_all = false, abandon_service = false, cancel_music_traversal = false;
    // Handler entry through its return to the caller, or transfer to idle when
    // abandon_service is true. Dispatch is separate; units are 1/64 sample.
    std::uint16_t work = 0;
    bool operator==(const PlaybackRequests&) const = default;
};
struct PlaybackState {
    PlaybackControls controls;
    PlaybackOwnership ownership;
    std::uint8_t releases = 0;
    bool operator==(const PlaybackState&) const = default;
};
// Aggregate work in integer sound ticks (1/64 sample), before the outer return.
// Effect-volume work includes automatic stop/termination; no output is published.
struct PlaybackEnvelopeWork {
    std::uint16_t music_volume = 0, effect_volume = 0, effect_pan = 0, tempo = 0, pitch = 0;
    std::uint32_t duration() const {
        return std::uint32_t{music_volume} + effect_volume + effect_pan + tempo + pitch;
    }
    bool operator==(const PlaybackEnvelopeWork&) const = default;
};
// Ramps use duration/value. Mono, mute and pause use value with duration zero;
// stop and conditional commands require both unused arguments to be zero.
// Requests are at driver time; the native owner assigns effective synthesis time.
PlaybackRequests apply_playback_command(PlaybackCommand command, std::uint8_t duration,
                                        std::uint8_t value, PlaybackState& state,
                                        DriverGlobalControls& global, DriverNoteQueues& queues,
                                        std::span<DriverChannelControls, 8> music_channels);
PlaybackEnvelopeWork advance_playback_envelopes(PlaybackState& state, DriverGlobalControls& global,
                                                DriverNoteQueues& queues,
                                                std::span<DriverChannelControls, 8> music_channels);
enum class TerminationOrigin : std::uint8_t { playback, script };
// Termination queues no release. Returns complete handler work in sound ticks;
// script endings include their exit prefix. Dispatch and caller work are separate.
std::uint16_t terminate_playback_channel(SequenceChannel channel, PlaybackState& state,
                                         DriverGlobalControls& global,
                                         std::span<DriverChannelControls, 8> music_channels,
                                         TerminationOrigin origin = TerminationOrigin::playback);
DriverOutputPolicy playback_output_policy(SequenceChannel channel, const PlaybackState& state);
} // namespace ff6
