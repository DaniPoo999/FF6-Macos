#include "ff6/synthesis.hpp"
#include <limits>

namespace ff6 {
EchoFilterPresets decode_echo_filters(ByteView bytes) {
    if (bytes.size() != 40)
        throw std::runtime_error("Invalid echo filter table extent");
    EchoFilterPresets result{};
    for (std::size_t preset = 0; preset < result.size(); ++preset)
        for (std::size_t tap = 0; tap < result[preset].size(); ++tap) {
            const int value = bytes[preset * 8 + tap];
            result[preset][tap] = static_cast<std::int8_t>(value < 128 ? value : value - 256);
        }
    return result;
}
EchoFilterPresets load_echo_filters(const ContentPack& pack) {
    return decode_echo_filters(pack.get("audio_echo_filter_table"));
}

Synthesis::Synthesis(std::span<const StereoFrame> initial_delay,
                     const std::array<StereoFrame, 8>& initial_history,
                     std::uint64_t initial_sample_index, std::uint16_t initial_noise_state)
    : noise_(initial_sample_index, initial_noise_state), echo_(initial_delay, initial_history),
      sample_index_(initial_sample_index) {
    for (auto& voice : voices_)
        voice = SampleVoice(initial_sample_index);
}
SynthesisFrame Synthesis::render(const SynthesisFrameInput& input) {
    if (sample_index_ == std::numeric_limits<std::uint64_t>::max())
        throw std::runtime_error("Synthesis timeline exhausted");
    if (input.noise_rate > 31)
        throw std::runtime_error("Invalid shared noise rate");
    if (input.voices[0].pitch_modulation)
        throw std::runtime_error("First voice has no pitch modulation predecessor");
    std::array<VoiceFrameInput, 8> controls{};
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const auto& source = input.voices[i];
        auto& control = controls[i];
        control.pitch = source.pitch;
        control.envelope = source.envelope;
        control.envelope_behavior = source.envelope_behavior;
        control.start = source.start;
        control.trigger = source.trigger;
        control.release = source.release;
        control.pitch_modulation = source.pitch_modulation;
        validate_voice_input(control);
    }

    SynthesisFrame result;
    // All voices share the same cadence/noise value. Modulation uses this
    // sample's preceding mono output, before stereo volume or echo sends.
    result.noise = noise_.advance(input.noise_rate);
    std::array<VoiceContribution, 8> contributions{};
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        auto& control = controls[i];
        if (input.voices[i].noise)
            control.noise = result.noise;
        if (i > 0)
            control.previous_voice_output = result.voices[i - 1].mono;
        const auto mono = voices_[i].render(control);
        result.voices[i] = {mono, voices_[i].envelope_state()};
        contributions[i] = {mono, input.voices[i].volume, input.voices[i].echo};
    }
    result.mix = mix_voices(contributions);
    result.echo = echo_.render(result.mix, input.echo);
    ++sample_index_;
    return result;
}
} // namespace ff6
