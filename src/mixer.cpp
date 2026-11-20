#include "ff6/mixer.hpp"
#include "audio_math.hpp"
#include <algorithm>
#include <limits>

namespace ff6 {
namespace {
using audio_math::floor_shift;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::int16_t clip(std::int32_t value) {
    return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}
std::int16_t wrap(std::int32_t value) {
    // Conversion to unsigned deliberately keeps low bits even for negatives.
    return static_cast<std::int16_t>(signed16(wrap16(static_cast<std::uint32_t>(value))));
}
std::int16_t even(std::int16_t value) {
    return static_cast<std::int16_t>(floor_shift(value, 1) * 2);
}
std::int32_t scale(std::int16_t sample, std::int8_t gain) {
    return floor_shift(std::int32_t{sample} * gain, 7);
}
} // namespace

NoiseSequence::NoiseSequence(std::uint64_t initial_sample_index, std::uint16_t initial_state)
    : state_(initial_state), sample_index_(initial_sample_index) {
    require(state_ <= 32767, "Invalid noise sequence state");
}
std::int16_t NoiseSequence::advance(std::uint8_t rate) {
    require(sample_index_ < std::numeric_limits<std::uint64_t>::max(), "Noise timeline exhausted");
    if (envelope_rate_due(rate, sample_index_)) {
        const std::uint32_t feedback = (state_ ^ (state_ / 2U)) & 1U;
        state_ = static_cast<std::uint16_t>(state_ / 2U + feedback * 16384U);
    }
    ++sample_index_;
    return wrap(std::int32_t{state_} * 2);
}

StereoMix mix_voices(std::span<const VoiceContribution> voices) {
    require(voices.size() <= 8, "Too many simultaneous voices");
    StereoMix result;
    for (const auto& voice : voices) {
        require(voice.mono % 2 == 0, "Voice output must have a zero low bit");
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const auto contribution = scale(voice.mono, voice.volume[channel]);
            result.dry[channel] = clip(result.dry[channel] + contribution);
            if (voice.echo)
                result.send[channel] = clip(result.send[channel] + contribution);
        }
    }
    return result;
}

EchoProcessor::EchoProcessor(std::span<const StereoFrame> initial_delay,
                             const std::array<StereoFrame, 8>& initial_history)
    : history_(initial_history) {
    const auto length = initial_delay.size();
    require(length == 1 || (length >= 512 && length <= 7680 && length % 512 == 0),
            "Invalid echo delay length");
    delay_.assign(initial_delay.begin(), initial_delay.end());
}
EchoResult EchoProcessor::render(const StereoMix& mix, const EchoSettings& settings) {
    // Keep history in temporal order, without a device address or register bus.
    std::move(history_.begin() + 1, history_.end(), history_.begin());
    history_.back() = delay_[position_];
    EchoResult result;
    for (std::size_t channel = 0; channel < 2; ++channel) {
        const auto term = [&](std::size_t index) {
            return floor_shift(
                floor_shift(history_[index][channel], 1) * settings.coefficients[index], 6);
        };
        std::int32_t older = 0;
        for (std::size_t i = 0; i < 7; ++i)
            older += term(i);
        result.filtered[channel] = even(clip(std::int32_t{wrap(older)} + wrap(term(7))));
        // Each master/return product wraps separately before the final sum.
        // This matters only at -32768 * -128; see the oracle discrepancy in MIXER.md.
        const auto dry = wrap(scale(mix.dry[channel], settings.master_volume[channel]));
        const auto wet = wrap(scale(result.filtered[channel], settings.echo_volume[channel]));
        result.output[channel] = settings.mute ? 0 : clip(std::int32_t{dry} + wet);
        const auto feedback = wrap(scale(result.filtered[channel], settings.feedback));
        result.feedback[channel] = even(clip(std::int32_t{mix.send[channel]} + feedback));
        if (settings.write_enabled[channel])
            delay_[position_][channel] = result.feedback[channel];
    }
    position_ = (position_ + 1) % delay_.size();
    return result;
}
} // namespace ff6
