#pragma once
#include "ff6/driver_clock.hpp"
#include "ff6/driver_sequence.hpp"

namespace ff6 {
struct PublishedSample {
    std::optional<SampleIdentity> identity;
    bool operator==(const PublishedSample&) const = default;
};
using PublishedVoiceValue =
    std::variant<StereoGain, DriverPitchRequest, PublishedSample, DriverEnvelope>;
struct PublishedVoiceChange {
    SoundTime at = 0;
    std::uint8_t voice = 0;
    PublishedVoiceValue value;
    bool operator==(const PublishedVoiceChange&) const = default;
};
struct VoicePublication {
    // End of this operation's documented work. Individual output methods stop
    // at publication; update/pause methods include their complete stage/handler.
    // This is never the sample at which synthesis consumes a change.
    SoundTime completed_at = 0;
    std::vector<PublishedVoiceChange> changes;
};
struct VoiceUpdateInterval {
    SequenceChannel channel;
    SoundTime begun = 0, completed = 0;
    bool operator==(const VoiceUpdateInterval&) const = default;
};
struct VoiceTraversalPublication {
    // Completion includes final dirty cleanup, ending at the next poll entry.
    VoicePublication output;
    std::array<VoiceUpdateInterval, 12> updates{};
    std::uint8_t count = 0;
};

struct PublishedEchoFilter {
    std::array<std::int8_t, 8> taps{};
    bool operator==(const PublishedEchoFilter&) const = default;
};
struct PublishedEchoFeedback {
    std::int8_t value = 0;
    bool operator==(const PublishedEchoFeedback&) const = default;
};
struct PublishedEchoGain {
    StereoGain stereo{};
    bool operator==(const PublishedEchoGain&) const = default;
};
enum class VoiceRouting : std::uint8_t { echo, noise, modulation };
struct PublishedRouting {
    VoiceRouting kind;
    std::uint8_t voices = 0;
    bool operator==(const PublishedRouting&) const = default;
};
struct PublishedNoiseRate {
    std::uint8_t value = 0;
    bool operator==(const PublishedNoiseRate&) const = default;
};
enum class NoteGate : std::uint8_t { releases, starts };
struct PublishedNoteGate {
    NoteGate kind;
    std::uint8_t voices = 0;
    bool operator==(const PublishedNoteGate&) const = default;
};
using PublishedGlobalValue =
    std::variant<PublishedEchoFilter, PublishedEchoFeedback, PublishedEchoGain, PublishedRouting,
                 PublishedNoiseRate, PublishedNoteGate>;
struct PublishedGlobalChange {
    SoundTime at = 0;
    PublishedGlobalValue value;
    bool operator==(const PublishedGlobalChange&) const = default;
};
struct GlobalPublication {
    // Last publication, excluding subsequent service/return work. Gate masks
    // are published intentions; effective note consumption is a separate stage.
    SoundTime completed_at = 0;
    std::vector<PublishedGlobalChange> changes;
};
struct DriverLoadStart {
    GlobalPublication release;
    SoundTime awaiting_content_at = 0;
};
struct DriverLoadCompletion {
    VoicePublication output;
    SoundTime discard_at = 0;
    std::uint8_t discarded_opportunities = 0;
};
struct DriverScriptVoiceOutput {
    SequenceChannel channel;
    std::size_t intent_index = 0;
    SoundTime begun = 0;
    VoicePublication publication;
};
struct DriverServiceVoicePass {
    SoundTime begun = 0;
    VoiceTraversalPublication publication;
};
struct DriverServicePublication {
    // Begin: command-poll boundary, immediate effects entry, or paused idle.
    // Finish: next idle poll. Neither endpoint is a synthesis sample boundary.
    SoundTime completed_at = 0;
    std::optional<GlobalPublication> buffered;
    std::vector<DriverScriptVoiceOutput> scripts;
    std::optional<DriverServiceVoicePass> voices;
};

// Serial FFVI output work. Call at the documented semantic publication
// boundaries, after gain/pitch calculation; no device addresses or CPU state.
// Retained values preserve conditional updates and observable intermediate
// pitch/envelope/gain values. Initial history uses the approved cold profile.
class DriverPublisher {
    struct Voice {
        StereoGain gain{};
        std::uint16_t pitch = 0;
        DriverEnvelope envelope;
    };
    std::array<Voice, 8> voices_{};
    PublishedEchoFilter filter_{};
    PublishedEchoGain echo_gain_{};
    SoundTime ready_at_ = 0;
    std::optional<DriverSongOutput> pending_load_;
    void validate_time(SoundTime begun, unsigned duration) const;
    void validate(std::uint8_t voice, SoundTime begun, unsigned duration) const;

  public:
    // Consume one already-advanced begin/finish service output. Derives every
    // nested publication start and reserves the complete stage, including quiet
    // work. No logical state advances; the entire history commits together.
    DriverServicePublication service(SoundTime begun, const DriverServiceOutput& requested);
    // Consume an already-applied logical load. Release precedes an unbounded
    // external wait; all other publication is blocked until finish_load.
    DriverLoadStart begin_load(SoundTime begun, const DriverSongOutput& requested);
    // A logical ready event resumes work; host I/O latency is not this time.
    // Restores all voices, performs cleanup and discards backlog without phase
    // reset. History and clock commit together, after all validation succeeds.
    DriverLoadCompletion finish_load(SoundTime content_ready, DriverClock& clock);
    bool loading() const {
        return pending_load_.has_value();
    }
    // Complete the voice pass after scripts, including skipped-slot work and
    // final dirty clearing. State/history commit only after the entire pass.
    // This replaces individual voice advancement; do not advance twice.
    VoiceTraversalPublication update_voices(SoundTime begun, DriverState& state);
    // Consume an already-advanced logical pass. No mutable controls enter this
    // path; retained output history supplies conditional publication costs.
    // A rejected publication leaves history unchanged, so the pass can be retried.
    VoiceTraversalPublication publish_voices(SoundTime begun, const DriverVoicePass& pass);
    VoicePublication publish_voice(SoundTime begun, SequenceChannel channel,
                                   const DriverVoiceRequests& requests);
    // Advance one voice's oscillators/dirty decisions and publish its outputs.
    // Commits controls, dirty sets and retained output history together.
    VoicePublication update_voice(SoundTime begun, const DriverOutputPolicy& policy,
                                  DriverChannelControls& channel, const DriverPitchMotion& pitch,
                                  DriverGlobalControls& global);
    GlobalPublication buffered(SoundTime begun, const DriverBufferedOutput& requested);
    // Loading releases all voices through a separate immediate path.
    GlobalPublication release_all(SoundTime begun);
    // Publish the already-applied pause command through its complete handler
    // deadline. Does not advance logical ownership again. Holds preserve shape,
    // then silence left/right and clear retained gains; unpause forces ADSR.
    VoicePublication pause(SoundTime begun, const PlaybackRequests& requested);
    VoicePublication gain(SoundTime begun, std::uint8_t voice, StereoGain requested);
    // Begin at gain calculation, deriving its publication start and values.
    VoicePublication gain_from_controls(SoundTime begun, const DriverOutputPolicy& policy,
                                        const DriverChannelControls& channel,
                                        const DriverGlobalControls& global);
    VoicePublication pitch(SoundTime begun, std::uint8_t voice, std::uint16_t requested);
    VoicePublication pitch_from_controls(SoundTime begun, const DriverOutputPolicy& policy,
                                         const DriverChannelControls& channel,
                                         const DriverPitchMotion& pitch);
    VoicePublication envelope(SoundTime begun, SequenceChannel channel, DriverEnvelope requested);
    VoicePublication sample(SoundTime begun, SequenceChannel channel,
                            DriverSampleSelection requested);
    // Song resumption unconditionally restores left then right silence.
    VoicePublication restore_silence(SoundTime begun, std::uint8_t voice);
};
} // namespace ff6
