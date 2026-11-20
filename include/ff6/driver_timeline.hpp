#pragma once
#include "ff6/driver_poll.hpp"
#include "ff6/driver_publication.hpp"

namespace ff6 {
struct DriverTimedService {
    SoundTime begun = 0;
    DriverServicePublication publication;
    // Present at begin only; coalesced opportunities still authorize one service.
    std::optional<DriverOpportunity> opportunity;
    // Present when effects follow a completed music-stage command inspection.
    std::optional<DriverCommandPoll> command_poll;
};
struct DriverTimedPause {
    DriverCommandPoll command_poll;
    VoicePublication publication;
    // Unpause returns to its caller: idle samples eligibility, music continues
    // directly into effects. Pause abandons either caller without sampling.
    std::optional<DriverIdlePoll> idle;
};

// Owns serial output history, periodic eligibility and load/service timing.
// Logical transitions are prepared once by DriverSequencer and consumed here.
// External input assignment/readiness and the idle polling loop remain caller
// responsibilities; this is not the complete autonomous playback worker.
class DriverTimeline {
    enum class Stage { idle, eligible, music_poll, effects, loading };
    DriverPublisher publisher_;
    DriverClock clock_;
    Stage stage_ = Stage::idle;
    SoundTime available_at_;
    std::optional<DriverIdlePoll> selected_;

  public:
    DriverTimeline(SoundTime activated_at, std::uint16_t until_four_sample_boundary);
    // One idle inspection, optionally including a returning allocation/control handler.
    // Like finish_service, this publishes no immediate command output.
    // A due result is retained until begin_service;
    // a zero result advances to the next inspection. No wall-clock waiting.
    DriverIdlePoll poll_idle(std::optional<DriverHandlerWork> handled = std::nullopt);
    // Normal native service selection after an eligibility sample. Supports full
    // and echo-disabled flushes; original diagnostic output modes are excluded.
    // Rejected/zero polls do not commit clock or publication changes.
    DriverTimedService begin_service(SoundTime sampled_at, const DriverServiceOutput& requested);
    // Derives effects entry from the preceding native stage and, when inspected,
    // one already-applied returning handler. This path publishes no command output;
    // pause/unpause uses publish_pause instead.
    DriverTimedService finish_service(const DriverServiceOutput& requested,
                                      std::optional<DriverHandlerWork> handled = std::nullopt);
    // Consume an already-applied pause/unpause exactly once at an inspection.
    // Publishes envelope/gain changes and selects the proper caller continuation.
    // If idle is due, begin_service must consume its retained opportunity next.
    DriverTimedPause publish_pause(const PlaybackRequests& requested);
    DriverLoadStart begin_load(SoundTime begun, const DriverSongOutput& requested);
    DriverLoadCompletion finish_load(SoundTime content_ready);
    SoundTime available_at() const {
        return available_at_;
    }
    bool loading() const {
        return stage_ == Stage::loading;
    }
};
} // namespace ff6
