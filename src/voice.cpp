#include "ff6/voice.hpp"
#include "audio_math.hpp"
#include "rom-materials/audio_pause_gain_mode.macros.inc"
#include <algorithm>
#include <limits>

namespace ff6 {
namespace {
using audio_math::floor_shift;
#include "gaussian_coefficients.inc"
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void validate(SampleEnvelope shape) {
    require(shape.attack_rate <= 15 && shape.decay_rate <= 7 && shape.sustain_level <= 7 &&
                shape.sustain_rate <= 31,
            "Invalid ADSR shape");
}
void validate(EnvelopeBehavior behavior) {
    require(behavior == EnvelopeBehavior::adsr || behavior == EnvelopeBehavior::pause_hold,
            "Invalid envelope behavior");
}
std::int16_t clear_low_bit(std::int32_t value) {
    return static_cast<std::int16_t>(floor_shift(value, 1) * 2);
}
} // namespace

bool envelope_rate_due(std::uint8_t rate, std::uint64_t sample_index) {
    require(rate <= 31, "Invalid envelope rate");
    if (rate == 0)
        return false;
    if (rate == 31)
        return true;
    // Interleaved power-of-two, 3/4 and 5/8 interval families. Phase residues
    // preserve the shared cadence without a hardware counter or clock steps.
    const unsigned group = (rate - 1U) / 3U;
    const unsigned kind = (rate - 1U) % 3U;
    const unsigned interval = (kind == 0 ? 2048U : kind == 1 ? 1536U : 1280U) >> group;
    const unsigned offset = rate == 30 ? 0U : kind == 1 ? 1040U : kind == 2 ? 536U : 0U;
    return sample_index % interval == (offset + interval - 1) % interval;
}

void advance_envelope(EnvelopeState& state, SampleEnvelope shape, std::uint64_t sample_index,
                      EnvelopeBehavior behavior) {
    validate(shape);
    validate(behavior);
    require(state.gain <= 2047, "Invalid envelope gain");
    std::int32_t next = state.gain;
    std::uint8_t rate = 0;
    switch (state.stage) {
    case EnvelopeStage::release:
        state.gain = static_cast<std::uint16_t>(std::max(next - 8, 0));
        return;
    case EnvelopeStage::attack:
        if (behavior == EnvelopeBehavior::adsr) {
            next += shape.attack_rate == 15 ? 1024 : 32;
            rate = static_cast<std::uint8_t>(shape.attack_rate * 2U + 1U);
        } else {
            next -= (next + 255) / 256;
        }
        break;
    case EnvelopeStage::decay:
    case EnvelopeStage::sustain:
        next -= (next + 255) / 256;
        if (behavior == EnvelopeBehavior::adsr)
            rate = state.stage == EnvelopeStage::decay
                       ? static_cast<std::uint8_t>(shape.decay_rate * 2U + 16U)
                       : shape.sustain_rate;
        break;
    default:
        throw std::runtime_error("Invalid envelope stage");
    }
    // Stage transitions use the candidate even on samples without a gain pulse.
    // Interrupt_f5 pauses with a zero-rate exponential hold. Gain does not
    // change, but its candidate can still finish decay at the level selected by
    // its initialized gain mode. Release above overrides the hold. This is not a general gain-register interface.
    const auto sustain_level = behavior == EnvelopeBehavior::pause_hold
                                   ? (FF6_ROM_VALUE_AUDIO_PAUSE_GAIN_MODE_0 >> 5U)
                                   : shape.sustain_level;
    if (state.stage == EnvelopeStage::decay && next / 256 == sustain_level)
        state.stage = EnvelopeStage::sustain;
    if (state.stage == EnvelopeStage::attack && next > 2047)
        state.stage = EnvelopeStage::decay;
    if (envelope_rate_due(rate, sample_index))
        state.gain = static_cast<std::uint16_t>(std::clamp(next, 0, 2047));
}

std::int16_t interpolate_gaussian(const std::array<std::int16_t, 4>& samples,
                                  std::uint8_t fraction) {
    const auto term = [&](std::size_t coefficient, std::size_t sample) {
        return floor_shift(std::int32_t{gaussian_coefficients[coefficient]} * samples[sample], 11);
    };
    const auto first =
        term(255U - fraction, 0) + term(511U - fraction, 1) + term(256U + fraction, 2);
    const auto wrapped = signed16(wrap16(static_cast<std::uint32_t>(first + 65536)));
    return clear_low_bit(std::clamp(wrapped + term(fraction, 3), -32768, 32767));
}

bool SampleVoice::terminal_block() const {
    if (!stream_)
        return false;
    const auto block = stream_->next_source_block();
    return !block || (sample_->blocks()[*block].end && !sample_->blocks()[*block].loop);
}
void SampleVoice::advance_source() {
    std::move(lookahead_.begin() + 4, lookahead_.end(), lookahead_.begin());
    auto tail = std::span(lookahead_).last(4);
    std::fill(tail.begin(), tail.end(), 0);
    if (stream_)
        (void)stream_->read(tail);
}
void validate_voice_input(const VoiceFrameInput& input) {
    validate(input.envelope);
    validate(input.envelope_behavior);
    require(input.pitch <= 16383, "Invalid voice pitch");
    require(!input.noise || *input.noise % 2 == 0, "Noise input must have a zero low bit");
    if (input.start)
        require(input.start->blocks().front().filter == 0,
                "FFVI voice requires an unfiltered start");
}
std::int16_t SampleVoice::render(const VoiceFrameInput& input) {
    validate_voice_input(input);
    require(sample_index_ < std::numeric_limits<std::uint64_t>::max(), "Voice timeline exhausted");
    std::int32_t pitch = input.pitch;
    if (input.pitch_modulation)
        pitch += floor_shift(floor_shift(input.previous_voice_output, 5) * pitch, 10);

    bool terminal = terminal_block();
    if (priming_remaining_ > 0) {
        if (priming_remaining_ == 5) {
            sample_ = requested_sample_;
            stream_.reset();
            if (sample_)
                stream_.emplace(sample_);
            lookahead_.fill(0);
            terminal = false;
        }
        envelope_.gain = 0;
        --priming_remaining_;
        // Five silent frames: reset, prefetch three groups, then begin the
        // envelope. The unfiltered prefix makes the new note self-contained.
        position_ = priming_remaining_ > 0 && priming_remaining_ < 4 ? 16384 : 0;
        pitch = 0;
    }
    const std::size_t index = position_ / 4096;
    std::array<std::int16_t, 4> neighborhood{};
    std::copy_n(lookahead_.begin() + static_cast<std::ptrdiff_t>(index), 4, neighborhood.begin());
    const auto source = input.noise.value_or(
        interpolate_gaussian(neighborhood, static_cast<std::uint8_t>((position_ / 16) % 256)));
    const auto output = clear_low_bit(floor_shift(std::int32_t{source} * envelope_.gain, 11));

    if (terminal)
        envelope_ = {0, EnvelopeStage::release};
    if (input.release)
        envelope_.stage = EnvelopeStage::release;
    if (input.start || input.trigger) {
        requested_sample_ = input.start;
        priming_remaining_ = 5;
        envelope_.stage = EnvelopeStage::attack;
    }
    if (priming_remaining_ == 0)
        advance_envelope(envelope_, input.envelope, sample_index_, input.envelope_behavior);
    if (position_ >= 16384)
        advance_source();
    position_ = static_cast<std::uint16_t>(std::min((position_ % 16384) + pitch, 32767));
    ++sample_index_;
    return output;
}
} // namespace ff6
