#pragma once
#include "ff6/mixer.hpp"

namespace ff6 {
using EchoFilterPresets = std::array<std::array<std::int8_t, 8>, 4>;
// The package preserves all five declared rows; F8 selects only the first four.
EchoFilterPresets decode_echo_filters(ByteView bytes);
EchoFilterPresets load_echo_filters(const ContentPack& pack);

struct SynthesisVoiceInput {
    std::uint16_t pitch = 0;
    SampleEnvelope envelope{15, 7, 7, 0};
    EnvelopeBehavior envelope_behavior = EnvelopeBehavior::adsr;
    std::shared_ptr<const BrrSample> start;
    bool release = false;
    bool noise = false;
    bool pitch_modulation = false; // Only voices 1..7 have a predecessor.
    StereoGain volume{};
    bool echo = false;
    // Trigger the envelope even without a sampled waveform (authored noise).
    bool trigger = false;
};
struct SynthesisFrameInput {
    std::array<SynthesisVoiceInput, 8> voices{};
    std::uint8_t noise_rate = 0;
    EchoSettings echo;
};
struct VoiceObservation {
    std::int16_t mono = 0;
    EnvelopeState envelope;
};
struct SynthesisFrame {
    std::array<VoiceObservation, 8> voices{};
    std::int16_t noise = 0;
    StereoMix mix;
    EchoResult echo;
};

// Semantic effective controls at one logical sample boundary. No driver
// register writes, chip phases, audio callback clock or default history policy.
class Synthesis {
    std::array<SampleVoice, 8> voices_;
    NoiseSequence noise_;
    EchoProcessor echo_;
    std::uint64_t sample_index_;

  public:
    Synthesis(std::span<const StereoFrame> initial_delay,
              const std::array<StereoFrame, 8>& initial_history,
              std::uint64_t initial_sample_index = 0, std::uint16_t initial_noise_state = 0x4000);
    // Rejects invalid controls before changing any synthesis state. Rendering
    // performs no allocation; callers retain the immutable sample bank.
    SynthesisFrame render(const SynthesisFrameInput& input);
    std::uint64_t next_sample_index() const {
        return sample_index_;
    }
    std::span<const StereoFrame> delay_state() const {
        return echo_.delay_state();
    }
    std::size_t delay_position() const {
        return echo_.delay_position();
    }
};
} // namespace ff6
