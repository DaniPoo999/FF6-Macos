#include "ff6/driver_updates.hpp"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/audio_vibrato_scale.macros.inc"

namespace ff6 {
namespace {
std::uint8_t byte(unsigned value) {
    return static_cast<std::uint8_t>(value & 255U);
}
std::uint16_t word(unsigned value) {
    return static_cast<std::uint16_t>(value & 65535U);
}
std::uint16_t negate(std::uint16_t value) {
    return word(65536U - value);
}
std::uint8_t high(std::uint16_t value) {
    return static_cast<std::uint8_t>(value >> 8U);
}
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::uint8_t voice_mask(SequenceChannel channel) {
    require((channel.kind == SequenceKind::music || channel.kind == SequenceKind::effect) &&
                channel.voice < 8 && (channel.kind == SequenceKind::music || channel.voice >= 4),
            "Invalid driver update channel");
    return byte(1U << channel.voice);
}
bool ramp(DriverRamp& value) {
    if (value.remaining == 0)
        return false;
    --value.remaining;
    const auto previous = high(value.value);
    value.value = word(unsigned{value.value} + value.step);
    return high(value.value) != previous;
}
unsigned turn_oscillator(DriverOscillator& oscillator) {
    // Aggregate FFVI turn paths, including the helper return. These timings
    // accompany semantic state changes; see VOICE_WORK.md for source and limits.
    const auto previous = oscillator.step;
    const auto quarter = static_cast<std::uint16_t>(oscillator.maximum_step / 4U);
    oscillator.step = negate(previous);
    if (oscillator.gain_remaining == 0)
        return oscillator.shape < 128 ? 102U : oscillator.shape < 192 ? 106U : 92U;
    --oscillator.gain_remaining;
    if (oscillator.shape < 128) {
        if (oscillator.step < 32768) {
            oscillator.step = word(unsigned{oscillator.step} + quarter);
            return 170;
        }
        return 124;
    } else if (oscillator.shape < 192) {
        if (oscillator.step >= 32768) {
            oscillator.step = word(unsigned{oscillator.step} + 65536U - quarter);
            return 206;
        }
        return 128;
    } else {
        const auto magnitude = previous >= 32768 ? negate(previous) : previous;
        const auto grown = word(unsigned{magnitude} + quarter);
        oscillator.step = previous >= 32768 ? grown : negate(grown);
        return 186;
    }
}
bool oscillator_position(DriverOscillator& oscillator) {
    const auto previous = high(oscillator.position);
    oscillator.position = word(unsigned{oscillator.position} + oscillator.step);
    return high(oscillator.position) != previous;
}
unsigned oscillator_period(DriverOscillator& oscillator, bool vibrato) {
    oscillator.period_remaining = byte(unsigned{oscillator.period_remaining} + 255U);
    if (oscillator.period_remaining == 0) {
        const auto turn_work = turn_oscillator(oscillator);
        oscillator.period_remaining = oscillator.period;
        // Surrounding work restores oscillator state and packed gain counters.
        return (vibrato ? 188U : 194U) + turn_work;
    }
    return 34;
}
std::uint16_t vibrato_offset(std::uint16_t base, std::uint16_t position) {
    const unsigned scale = unsigned{base} * FF6_ROM_VALUE_AUDIO_VIBRATO_SCALE_0 / 256U;
    const auto phase = word(unsigned{position} * 2U);
    // The original multiplies separately truncated cross terms and omits the
    // low-byte product. A single full-width multiply gives different rounding.
    const auto result = (scale >> 8U) * high(phase) + (scale & 255U) * high(phase) / 256U +
                        (scale >> 8U) * (phase & 255U) / 256U;
    return word(result + 65536U - (phase >= 32768 ? scale : 0U));
}
struct PreparedPan {
    std::uint8_t value;
    unsigned duration;
};
PreparedPan add_pan(std::uint8_t base, std::uint8_t amount) {
    const int offset = amount < 128 ? int{amount} : int{amount} - 256;
    const int sum = int{base} + offset;
    // FFVI's subtracting path takes the same work whether it clamps or not.
    // Positive saturation adds work. These are aggregate helper durations,
    // including its return, not an instruction executor. See GAIN_PREPARATION.md.
    const unsigned duration = amount >= 128 ? 54U : sum > 255 ? 58U : 50U;
    return {static_cast<std::uint8_t>(std::clamp(sum, 0, 255)), duration};
}
std::int8_t signed_gain(unsigned value) {
    return static_cast<std::int8_t>(value < 128 ? static_cast<int>(value)
                                                : static_cast<int>(value) - 256);
}
DriverGainPreparation prepare_gain(const DriverOutputPolicy& policy,
                                   const DriverChannelControls& channel,
                                   const DriverGlobalControls& global, std::uint8_t mask) {
    const bool music = policy.channel.kind == SequenceKind::music;
    unsigned left_pan = 128;
    unsigned pan_work = policy.center_pan ? 24U : 50U;
    if (!policy.center_pan && !policy.system_effect) {
        auto pan = high(channel.pan.value);
        pan_work = music ? 110U : 136U;
        if (!music) {
            const auto effect = add_pan(pan, byte(policy.effect_pan ^ 128U));
            pan = effect.value;
            pan_work += effect.duration;
        }
        const auto sweep = add_pan(pan, high(channel.pan_sweep.position));
        left_pan = unsigned{sweep.value} ^ 255U;
        pan_work += sweep.duration;
    }
    unsigned amplitude = high(channel.volume.value);
    const auto tremolo = high(channel.tremolo.position);
    unsigned tremolo_work = 40;
    if (tremolo != 0) {
        const auto scaled = (unsigned{tremolo} * 2U & 255U) * amplitude / 256U;
        if (tremolo >= 128) {
            amplitude = scaled;
            tremolo_work = 66;
        } else {
            amplitude = byte(amplitude + scaled);
            if (amplitude >= 128)
                amplitude = 127;
            tremolo_work = 84;
        }
    }
    unsigned scale_work;
    if (music) {
        amplitude = amplitude * policy.music_volume / 256U;
        const bool ignores_song = (global.ignore_song_volume & mask) != 0;
        if (!ignores_song)
            amplitude = amplitude * global.song_volume / 256U;
        scale_work = ignores_song ? 52U : 80U;
    } else {
        amplitude = amplitude * (policy.system_effect ? 255U : policy.effect_volume) / 256U;
        scale_work = policy.system_effect ? 58U : 56U;
    }
    return {byte(amplitude), byte(left_pan), policy.muted,
            static_cast<std::uint16_t>(pan_work + tremolo_work + scale_work)};
}
DriverPitchPreparation prepare_pitch(const DriverOutputPolicy& policy,
                                     const DriverChannelControls& channel,
                                     const DriverPitchMotion& pitch) {
    const auto base = word(unsigned{pitch.base} + channel.vibrato_pitch_offset);
    // FFVI wraps the vibrato sum before global scaling. Effects bypass scaling;
    // music uses either attenuation or a doubled adjustment plus the base.
    // Aggregate work delays are measured at these semantic path boundaries.
    // See PITCH_PREPARATION.md; sample-time range narrowing happens later.
    if (policy.channel.kind == SequenceKind::effect)
        return {base, 104};
    const unsigned adjusted = unsigned{base} * policy.music_pitch_modifier / 256U;
    if (policy.music_pitch_modifier >= 128)
        return {word(adjusted), 206};
    return {word(adjusted * 2U + base), 244};
}
} // namespace

DriverPitchPreparation prepare_driver_pitch(const DriverOutputPolicy& policy,
                                            const DriverChannelControls& channel,
                                            const DriverPitchMotion& pitch) {
    (void)voice_mask(policy.channel);
    return prepare_pitch(policy, channel, pitch);
}

StereoGain DriverGainPreparation::stereo() const {
    if (muted)
        return {};
    return {signed_gain(unsigned{level} * left_weight / 256U),
            signed_gain(unsigned{level} * (left_weight ^ 255U) / 256U)};
}
DriverGainPreparation prepare_driver_gain(const DriverOutputPolicy& policy,
                                          const DriverChannelControls& channel,
                                          const DriverGlobalControls& global) {
    return prepare_gain(policy, channel, global, voice_mask(policy.channel));
}

DriverSongWork advance_driver_song(DriverGlobalControls& global) {
    // FFVI checks each scalar countdown, then advances all eight filter taps
    // under one shared countdown. Work depends on activity, not value/carry.
    // Returning it alongside the state change prevents separate timing logic
    // from accidentally consulting already-decremented counts. See SONG_WORK.md.
    const auto scalar_work = [](const DriverRamp& value) -> std::uint16_t {
        return value.remaining == 0 ? 14 : 48;
    };
    const DriverSongWork work{scalar_work(global.tempo), scalar_work(global.echo_volume),
                              scalar_work(global.echo_feedback),
                              static_cast<std::uint16_t>(global.filter_remaining == 0 ? 14 : 786)};
    (void)ramp(global.tempo);
    (void)ramp(global.echo_volume);
    (void)ramp(global.echo_feedback);
    if (global.filter_remaining != 0) {
        --global.filter_remaining;
        for (auto& tap : global.filter)
            tap.value = word(unsigned{tap.value} + tap.step);
    }
    return work;
}
DriverChannelWork advance_driver_channel(SequenceChannel identity, DriverChannelControls& channel,
                                         DriverPitchMotion& pitch, DriverGlobalControls& global) {
    const auto mask = voice_mask(identity);
    require(pitch.step <= 0xffffffU, "Invalid driver pitch increment");
    DriverChannelWork work;
    work.delays = 36;
    if (channel.vibrato.delay_remaining != 0) {
        --channel.vibrato.delay_remaining;
        work.delays += 6;
    }
    if (channel.tremolo.delay_remaining != 0) {
        --channel.tremolo.delay_remaining;
        work.delays += 6;
    }
    // Aggregate FFVI paths accompany the existing state transitions. Even an
    // idle stage takes work; high-byte changes add dirty-mask work regardless
    // of whether this voice was already dirty. See CHANNEL_WORK.md.
    work.volume = channel.volume.remaining == 0 ? 22 : 150;
    work.pan = channel.pan.remaining == 0 ? 18 : 146;
    const bool volume_changed = ramp(channel.volume);
    const bool pan_changed = ramp(channel.pan);
    if (volume_changed)
        work.volume += 8;
    if (pan_changed)
        work.pan += 8;
    if (volume_changed || pan_changed)
        global.volume_dirty = byte(global.volume_dirty | mask);
    work.pitch = 82;
    if (pitch.step != 0) {
        const auto increment = pitch.step;
        // An increment confined to the upper byte has an extra original zero
        // check. Keep its duration while expressing the motion as one 24-bit sum.
        work.pitch = (increment & 65535U) == 0 ? 218 : 212;
        channel.pitch_slide_remaining = byte(unsigned{channel.pitch_slide_remaining} + 255U);
        if (channel.pitch_slide_remaining == 0) {
            pitch.step = 0;
            work.pitch += 32;
        }
        // The final slide tick still applies the old increment after clearing
        // the stored rate. A zero rate, conversely, leaves the counter alone.
        const auto value = ((unsigned{pitch.base} << 8U) + pitch.fraction + increment) & 0xffffffU;
        pitch.base = static_cast<std::uint16_t>(value >> 8U);
        pitch.fraction = byte(value);
        global.pitch_dirty = byte(global.pitch_dirty | mask);
    }
    auto& sweep = channel.pan_sweep;
    work.sweep_position = 18;
    if (sweep.shape != 0) {
        work.sweep_position = 138;
        const auto previous = high(sweep.position);
        sweep.position = word(unsigned{sweep.position} + sweep.step);
        if (high(sweep.position) != previous) {
            global.volume_dirty = byte(global.volume_dirty | mask);
            work.sweep_position += 8;
        }
        work.sweep_period = 34;
        sweep.remaining = byte(unsigned{sweep.remaining} + 255U);
        if (sweep.remaining == 0) {
            sweep.step = negate(sweep.step);
            sweep.remaining = sweep.period;
            work.sweep_period = 108;
        }
    }
    return work;
}
DriverVoiceRequests update_driver_voice(const DriverOutputPolicy& policy,
                                        DriverChannelControls& channel,
                                        const DriverPitchMotion& pitch,
                                        DriverGlobalControls& global) {
    const auto mask = voice_mask(policy.channel);
    require(channel.vibrato.gain_remaining < 8 && channel.tremolo.gain_remaining < 8,
            "Invalid driver oscillator gain counter");
    unsigned work = channel.vibrato.shape == 0 ? 24U : 42U;
    if (channel.vibrato.shape != 0 && channel.vibrato.delay_remaining == 0) {
        work = 40;
        if (oscillator_position(channel.vibrato)) {
            // Fractional motion can change position without changing the
            // separately rounded pitch offset. Preserve both work paths.
            work += 442 + ((channel.vibrato.position & 0x4000U) != 0 ? 16U : 0U);
            const auto offset = vibrato_offset(pitch.base, channel.vibrato.position);
            if (offset != channel.vibrato_pitch_offset) {
                channel.vibrato_pitch_offset = offset;
                global.pitch_dirty = byte(global.pitch_dirty | mask);
                work += 46;
            }
        } else
            work += 112;
        work += oscillator_period(channel.vibrato, true);
    }
    if (channel.tremolo.shape != 0 && channel.tremolo.delay_remaining == 0) {
        work += 146;
        if (oscillator_position(channel.tremolo)) {
            global.volume_dirty = byte(global.volume_dirty | mask);
            work += 20;
        }
        work += oscillator_period(channel.tremolo, false);
    } else
        work += channel.tremolo.shape == 0 ? 18U : 40U;
    DriverVoiceRequests requests;
    const bool dirty = global.volume_dirty != 0 || global.pitch_dirty != 0;
    if ((global.volume_dirty & mask) != 0) {
        global.volume_dirty = byte(global.volume_dirty & ~unsigned{mask});
        const auto prepared = prepare_gain(policy, channel, global, mask);
        requests.volume = prepared.stereo();
        requests.gain_work = prepared.publication_delay;
    }
    if ((global.pitch_dirty & mask) != 0) {
        global.pitch_dirty = byte(global.pitch_dirty & ~unsigned{mask});
        const auto prepared = prepare_driver_pitch(policy, channel, pitch);
        requests.pitch = prepared.coefficient;
        requests.pitch_work = prepared.publication_delay;
    }
    // These are semantic path durations through FFVI's output checks. Other
    // voices' dirty bits prevent the early return even when this voice is idle.
    if (requests.volume) {
        work += 106;
        requests.work.between = requests.pitch ? 62 : 0;
        requests.work.after_last = requests.pitch ? 18 : 30;
    } else if (requests.pitch) {
        work += 134;
        requests.work.after_last = 18;
    } else
        work += dirty ? 102U : 14U;
    requests.work.before_first = static_cast<std::uint16_t>(work);
    return requests;
}
std::uint16_t driver_frequency_request(const DriverOutputPolicy& policy,
                                       const DriverChannelControls& channel,
                                       const DriverPitchMotion& pitch) {
    return prepare_driver_pitch(policy, channel, pitch).coefficient;
}
} // namespace ff6
