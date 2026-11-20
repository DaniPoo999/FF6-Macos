#pragma once
#include "ff6/voice.hpp"
#include "rom-materials/audio_initial_master_gain.macros.inc"

namespace ff6 {
using StereoFrame = std::array<std::int16_t, 2>;
using StereoGain = std::array<std::int8_t, 2>;

// Shared by all voices. Changing its rate never restarts sequence or cadence.
class NoiseSequence {
    std::uint16_t state_;
    std::uint64_t sample_index_;

  public:
    explicit NoiseSequence(std::uint64_t initial_sample_index = 0,
                           std::uint16_t initial_state = 0x4000);
    std::int16_t advance(std::uint8_t rate);
    std::uint16_t state() const {
        return state_;
    }
};

struct VoiceContribution {
    std::int16_t mono = 0;
    StereoGain volume{};
    bool echo = false;
};
struct StereoMix {
    StereoFrame dry{}, send{};
};
// Voice order is significant: each addition saturates independently.
StereoMix mix_voices(std::span<const VoiceContribution> voices);

struct EchoSettings {
    std::array<std::int8_t, 8> coefficients{}; // Oldest through newest history.
    StereoGain master_volume{FF6_ROM_VALUE_AUDIO_INITIAL_MASTER_GAIN_0,
                            FF6_ROM_VALUE_AUDIO_INITIAL_MASTER_GAIN_0};
    StereoGain echo_volume{};
    std::int8_t feedback = 0;
    std::array<bool, 2> write_enabled{true, true};
    bool mute = false;
};
struct EchoResult {
    StereoFrame output{}, filtered{}, feedback{};
};

// Fixed-length, explicitly seeded delay. One frame represents the original
// zero-delay case; other supported lengths are 512..7680 in steps of 512.
// Changing a game's delay/region requires a separate, researched transition
// contract. Reconstructing this object with zeroes is not that transition.
class EchoProcessor {
    std::vector<StereoFrame> delay_;
    std::array<StereoFrame, 8> history_;
    std::size_t position_ = 0;

  public:
    EchoProcessor(std::span<const StereoFrame> initial_delay,
                  const std::array<StereoFrame, 8>& initial_history);
    // History is full signed stereo values, oldest first; halving is internal.
    // No allocation. Write suppression preserves stored values; mute only
    // suppresses returned output, so feedback and histories keep progressing.
    EchoResult render(const StereoMix& mix, const EchoSettings& settings);
    std::span<const StereoFrame> delay_state() const {
        return delay_;
    }
    std::size_t delay_position() const {
        return position_;
    }
};
} // namespace ff6
