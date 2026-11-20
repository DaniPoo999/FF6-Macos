#pragma once
#include "ff6/samples.hpp"

namespace ff6 {
enum class EnvelopeStage : std::uint8_t { attack, decay, sustain, release };
enum class EnvelopeBehavior : std::uint8_t { adsr, pause_hold };
struct EnvelopeState {
    std::uint16_t gain = 0;
    EnvelopeStage stage = EnvelopeStage::release;
};
// Pure sample-time cadence; no per-note timer reset or device cycle scheduler.
bool envelope_rate_due(std::uint8_t rate, std::uint64_t sample_index);
void advance_envelope(EnvelopeState& state, SampleEnvelope shape, std::uint64_t sample_index,
                      EnvelopeBehavior behavior = EnvelopeBehavior::adsr);
std::int16_t interpolate_gaussian(const std::array<std::int16_t, 4>& samples,
                                  std::uint8_t fraction);

struct VoiceFrameInput {
    std::uint16_t pitch = 0; // 12 fractional bits; unmodulated range 0..16383.
    SampleEnvelope envelope{15, 7, 7, 0};
    // FFVI's pause hold affects envelopes only; source playback continues.
    EnvelopeBehavior envelope_behavior = EnvelopeBehavior::adsr;
    // Effective events, applied after this frame's mono output. Simultaneous
    // on/off starts a note. Driver write/latch latency is outside this API.
    std::shared_ptr<const BrrSample> start;
    bool release = false;
    std::optional<std::int16_t> noise;
    bool pitch_modulation = false;
    std::int16_t previous_voice_output = 0;
    // Trigger the envelope even without a sampled waveform (authored noise).
    bool trigger = false;
};
// Stateless validation permits a multivoice owner to reject a complete frame
// before advancing any voice. The owner's timeline is checked separately.
void validate_voice_input(const VoiceFrameInput& input);

class SampleVoice {
    std::uint64_t sample_index_;
    EnvelopeState envelope_;
    std::shared_ptr<const BrrSample> sample_, requested_sample_;
    std::optional<BrrStream> stream_;
    std::array<std::int16_t, 12> lookahead_{};
    std::uint16_t position_ = 0;
    std::uint8_t priming_remaining_ = 0;

    bool terminal_block() const;
    void advance_source();

  public:
    explicit SampleVoice(std::uint64_t initial_sample_index = 0)
        : sample_index_(initial_sample_index) {}
    // One logical sample. No allocation or file/device access. All supported
    // FFVI samples start unfiltered, so priming is independent of old history.
    std::int16_t render(const VoiceFrameInput& input);
    EnvelopeState envelope_state() const {
        return envelope_;
    }
    std::uint64_t next_sample_index() const {
        return sample_index_;
    }
};
} // namespace ff6
