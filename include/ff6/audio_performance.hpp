#pragma once
#include "ff6/driver_publication.hpp"
#include <memory>

namespace ff6 {
// Converts timed semantic publications into effective sample controls. The
// immutable bank must outlive this owner. Initial voice/global histories are
// quiescent, with all notes released and fixed master gain/delay policy.
// No register protocol, executable input or recorded PCM enters this interface.
class AudioPerformance {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit AudioPerformance(const SampleBank& bank);
    ~AudioPerformance();
    AudioPerformance(const AudioPerformance&) = delete;
    AudioPerformance& operator=(const AudioPerformance&) = delete;
    void append(const PublishedVoiceChange& change);
    void append(const PublishedGlobalChange& change);
    void append(const VoicePublication& publication);
    void append(const GlobalPublication& publication);
    void append(const DriverServicePublication& publication);
    // Consecutive frames are 64 ticks apart; first time must be at least 76.
    // The producer must supply publications through at+38 before this call.
    // consume_notes is the caller's alternating note phase, not device timing.
    SynthesisFrameInput controls(SoundTime at, bool consume_notes);
};
} // namespace ff6
