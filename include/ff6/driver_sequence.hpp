#pragma once
#include "ff6/driver_loading.hpp"
#include <variant>
#include "rom-materials/audio_initial_echo_delay.macros.inc"

namespace ff6 {
struct DriverEchoFlush {
    std::array<std::uint8_t, 8> filter{};
    std::uint8_t feedback = 0, volume = 0, voices = 0;
    bool operator==(const DriverEchoFlush&) const = default;
};
struct DriverBufferedOutput {
    std::optional<DriverEchoFlush> echo;
    std::uint8_t releases = 0, starts = 0, noise_rate = 0, noise = 0, modulation = 0;
    bool operator==(const DriverBufferedOutput&) const = default;
};
struct DriverPitchRequest {
    std::uint16_t value = 0;
    bool operator==(const DriverPitchRequest&) const = default;
};
struct DriverSampleSelection {
    // Absence clears the selection for a later note; it does not silence a voice.
    std::optional<SampleIdentity> identity;
    DriverEnvelope envelope;
    bool operator==(const DriverSampleSelection&) const = default;
};
struct DriverResumeSelection {
    DriverSampleSelection sample;
    std::uint16_t pitch = 0;
    // Native pitch calculation through publication entry, in sound ticks.
    std::uint16_t pitch_work = 0;
    bool operator==(const DriverResumeSelection&) const = default;
};
struct DriverSongOutput {
    bool release_all = false, abandon_service = false;
    std::optional<std::array<DriverResumeSelection, 8>> resume;
    DriverLoadWork work;
    bool operator==(const DriverSongOutput&) const = default;
};
using DriverVoiceValue =
    std::variant<DriverSampleSelection, DriverEnvelope, StereoGain, DriverPitchRequest>;
struct DriverVoiceIntent {
    std::uint8_t voice = 0;
    DriverVoiceValue value;
    bool operator==(const DriverVoiceIntent&) const = default;
};
struct DriverScriptPublication {
    // Index into this service output's voices; offset from selected-channel entry.
    std::size_t intent_index = 0;
    std::uint32_t offset = 0;
    bool operator==(const DriverScriptPublication&) const = default;
};
struct DriverChannelVisit {
    SequenceChannel channel;
    std::uint32_t wait_work = 0, script_work = 0, envelope_offset = 0, work = 0;
    std::vector<DriverScriptPublication> publications;
    // Derived start relative to the containing script pass.
    std::uint32_t pass_offset = 0;
    bool operator==(const DriverChannelVisit&) const = default;
};
struct DriverScriptPass {
    SequenceKind kind = SequenceKind::music;
    std::uint16_t phase_work = 0;
    std::optional<std::uint32_t> song_offset;
    std::uint32_t work = 0;
    bool operator==(const DriverScriptPass&) const = default;
};
struct DriverServiceBeginWork {
    std::uint16_t buffered_loop_end = 0, status_end = 0;
    std::optional<std::uint32_t> control_offset;
    // Includes the slow-ramp return, excluding the subsequent script CALL.
    std::uint32_t control_work = 0;
    std::optional<std::uint32_t> script_offset;
    // Accepted service entry through the end of this begin_service stage.
    std::uint32_t work = 0;
    bool operator==(const DriverServiceBeginWork&) const = default;
};
struct DriverVoiceUpdate {
    SequenceChannel channel;
    // Gap from pass entry or the preceding voice's completion.
    std::uint16_t before = 0;
    DriverVoiceRequests requests;
    bool operator==(const DriverVoiceUpdate&) const = default;
};
struct DriverVoicePass {
    std::vector<DriverVoiceUpdate> updates;
    // Final selection, dirty cleanup and transfer to the next poll.
    std::uint16_t after = 0;
    bool operator==(const DriverVoicePass&) const = default;
};
// Advance selected voices once and retain their prepared values/work. Commits
// logical state after the entire pass succeeds; publication is a separate stage.
DriverVoicePass advance_driver_voices(DriverState& state);
struct DriverServiceOutput {
    // Present only at begin_service, before any script or control advancement.
    std::optional<DriverBufferedOutput> buffered;
    std::optional<DriverServiceBeginWork> begin_work;
    std::vector<DriverVoiceIntent> voices;
    // Derived during the same logical transitions. Publication offsets are
    // relative to the visit, whose pass_offset includes intervening traversal.
    std::vector<DriverChannelVisit> channels;
    // Music: phase entry through the command-poll CALL site, or effects entry
    // when skipped. Effects: phase entry through return before voice updates.
    std::optional<DriverScriptPass> script_pass;
    // Present only at finish_service, after effects. Does not depend on output
    // history; the publisher derives conditional publication costs later.
    std::optional<DriverVoicePass> voice_pass;
    bool music_poll = false;
    bool operator==(const DriverServiceOutput&) const = default;
};

// Owns continuous native driver state. Immutable content must outlive the owner.
// Call begin_service, optionally issue commands when music_poll is true, then
// finish_service if service_pending(). Commands may also arrive between services.
// No sample clock or audio callback enters this logical scheduling interface.
class DriverSequencer {
    enum class Stage { idle, music_poll, effects };
    const MusicScripts& scripts_;
    const SampleBank& samples_;
    EchoFilterPresets filters_;
    NotePitchTable pitches_;
    DriverState state_;
    std::optional<SuspendedDriverSong> suspended_;
    Stage stage_ = Stage::idle;
    bool echo_enabled_ = true;
    std::uint8_t echo_delay_ = FF6_ROM_VALUE_AUDIO_INITIAL_ECHO_DELAY_0;

    void require_command_boundary() const;
    DriverSampleDefaults defaults(std::uint8_t slot, const DriverState& state) const;
    DriverChannelVisit advance_channel(SequenceChannel identity, DriverState& state,
                                       std::vector<DriverVoiceIntent>& output) const;

  public:
    DriverSequencer(const MusicScripts& scripts, const SampleBank& samples,
                    EchoFilterPresets filters, NotePitchTable pitches);
    const DriverState& state() const {
        return state_;
    }
    const std::optional<SuspendedDriverSong>& suspended_song() const {
        return suspended_;
    }
    bool service_pending() const {
        return stage_ != Stage::idle;
    }
    std::uint8_t echo_delay() const {
        return echo_delay_;
    }
    bool echo_enabled() const {
        return echo_enabled_;
    }
    DriverSongOutput load(SongLoad request);
    // Complete allocation-handler work; dispatch/arrival timing is separate.
    std::uint16_t effect(std::uint8_t identity, std::uint8_t pan);
    std::uint16_t system_effect(std::uint8_t identity);
    PlaybackRequests command(PlaybackCommand command, std::uint8_t duration, std::uint8_t value);
    void set_forced_progression(bool enabled);
    void set_echo_enabled(bool enabled);
    // Returns the native delay request. Its sample-time transition is separate.
    std::uint8_t set_echo_delay(std::uint8_t delay);
    DriverServiceOutput begin_service();
    DriverServiceOutput finish_service();
};
} // namespace ff6
