#include "ff6/driver_controls.hpp"
#include "driver_oscillator.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
std::uint8_t byte(unsigned value) {
    return static_cast<std::uint8_t>(value & 255U);
}
std::uint16_t bits(int value) {
    return static_cast<std::uint16_t>(static_cast<unsigned>(value) & 65535U);
}
std::uint16_t rate(int difference, std::uint8_t duration) {
    require(duration != 0, "Zero driver slide duration");
    return bits(difference * 256 / duration);
}
int high(std::uint16_t value) {
    return value >> 8U;
}
int signed_high(std::uint16_t value) {
    const auto result = high(value);
    return result < 128 ? result : result - 256;
}
void set(DriverRamp& ramp, std::uint8_t target) {
    ramp.value = static_cast<std::uint16_t>(unsigned{target} * 256U);
    ramp.remaining = 0; // The previous slope remains stored.
}
enum class SlideSetup { immediate, equal, rising, falling };
unsigned rate_work(int difference) {
    // Magnitude conversion surrounds the two divisions only on a falling ramp.
    return difference < 0 ? 162U : 130U;
}
SlideSetup slide(DriverRamp& ramp, std::uint8_t duration, std::uint8_t target,
                 bool signed_value = false) {
    if (duration == 0) {
        set(ramp, target);
        return SlideSetup::immediate;
    }
    const int destination = signed_value && target >= 128 ? int{target} - 256 : int{target};
    const int difference =
        destination - (signed_value ? signed_high(ramp.value) : high(ramp.value));
    ramp.remaining = difference == 0 ? 0 : duration;
    if (difference != 0)
        ramp.step = rate(difference, duration);
    return difference == 0  ? SlideSetup::equal
           : difference > 0 ? SlideSetup::rising
                            : SlideSetup::falling;
}
unsigned slide_work(SlideSetup setup, unsigned immediate, unsigned equal, unsigned rising) {
    return setup == SlideSetup::immediate ? immediate
           : setup == SlideSetup::equal   ? equal
           : setup == SlideSetup::rising  ? rising
                                          : rising + 32U;
}
void envelope_valid(const DriverEnvelope& e) {
    require(e.attack < 16 && e.decay < 8 && e.sustain_level < 8 && e.sustain_rate < 32,
            "Invalid stored driver envelope");
}
unsigned initialize_oscillator(DriverOscillator& oscillator, DriverChannelControls& channel,
                               const std::array<std::uint8_t, 3>& operands) {
    oscillator.delay = oscillator.delay_remaining = operands[0];
    oscillator.period = oscillator.period_remaining = operands[1];
    oscillator.shape = operands[2];
    const auto amplitude = unsigned{operands[2] & 63U} + 1U;
    const auto quotient = operands[1] == 0 ? amplitude : amplitude * 256U / operands[1];
    oscillator.maximum_step = static_cast<std::uint16_t>(std::max(1U, quotient / 4U) * 4U);
    detail::restart_oscillator(oscillator, channel.vibrato.gain_remaining);
    // Shared rate and restart helpers; caller-specific stores remain separate.
    const unsigned shape = operands[2] & 0xc0U;
    const auto rate_duration = (operands[1] == 0 ? 134U : 190U) + (quotient < 4 ? 4U : 0U);
    const auto delay_duration = (operands[0] == 0 ? 64U : 96U) + (shape == 0x80   ? 42U
                                                                  : shape == 0xc0 ? 14U
                                                                                  : 0U);
    return rate_duration + delay_duration;
}
void refresh(DriverRouting& routing, std::uint8_t owners) {
    routing.selected = byte((routing.music & ~unsigned{owners}) | routing.effects);
}
} // namespace

DriverSampleDefaults driver_sample_defaults(std::uint8_t slot, const SampleAsset& asset) {
    const auto& e = asset.envelope;
    DriverSampleDefaults defaults{
        slot,
        asset.pitch_adjustment,
        {e.attack_rate, e.decay_rate, e.sustain_level, e.sustain_rate, true}};
    envelope_valid(defaults.envelope);
    return defaults;
}
bool is_driver_control(MusicCommand command) {
    const auto code = static_cast<std::uint8_t>(command);
    return (code >= 0xc4 && code <= 0xe1) || code == 0xe4 || code == 0xe6 ||
           (code >= 0xf0 && code <= 0xf4) || (code >= 0xf7 && code <= 0xfb);
}
DriverControlResult apply_driver_control(const ScriptInstruction& instruction,
                                         const DriverControlContext& context,
                                         const EchoFilterPresets& filters,
                                         DriverChannelControls& channel,
                                         DriverGlobalControls& global) {
    const auto command = static_cast<MusicCommand>(instruction.code);
    require(is_driver_control(command), "Not a driver control command");
    require(instruction.operand_count == music_operand_count(command),
            "Invalid driver operand count");
    for (std::size_t i = instruction.operand_count; i < instruction.operands.size(); ++i)
        require(instruction.operands[i] == 0, "Nonzero unused driver operand");
    const auto voice = context.channel.voice;
    const bool music = context.channel.kind == SequenceKind::music;
    require((music || context.channel.kind == SequenceKind::effect) && voice < 8 &&
                (music || voice >= 4),
            "Invalid driver channel");
    require((context.effect_owners & 15U) == 0, "Invalid effect voice ownership");
    require(channel.vibrato.gain_remaining < 8 && channel.tremolo.gain_remaining < 8,
            "Invalid oscillator gain counter");
    envelope_valid(channel.envelope);
    const auto mask = byte(1U << voice);
    auto c = channel;
    auto g = global;
    DriverControlResult result;
    unsigned work = 0;
    const auto a = instruction.operands[0], b = instruction.operands[1];
    const auto dirty = [&] { g.volume_dirty = byte(g.volume_dirty | mask); };
    const auto envelope_request = [&](unsigned prefix) {
        if (!music || (context.effect_owners & mask) == 0) {
            result.envelope = c.envelope;
            result.envelope_offset = static_cast<std::uint16_t>(prefix);
            work = prefix + (music ? 158U : 136U);
        } else
            work = prefix + 40U;
    };
    const auto defaults = [&]() -> const DriverSampleDefaults& {
        const auto slot = command == MusicCommand::sample ? a : c.sample_slot;
        require(context.sample_defaults.has_value() && context.sample_defaults->slot == slot,
                "Missing loaded sample defaults");
        envelope_valid(context.sample_defaults->envelope);
        return *context.sample_defaults;
    };
    const auto toggle = [&](DriverRouting& routing, bool enabled) {
        auto& requests = music ? routing.music : routing.effects;
        requests = enabled ? byte(requests | mask) : byte(requests & ~unsigned{mask});
        refresh(routing, context.effect_owners);
    };
    const auto refresh_noise = [&] {
        g.selected_noise_rate = g.noise.effects == 0 ? g.music_noise_rate : g.effects_noise_rate;
        g.noise.selected =
            byte((g.noise.music & ~unsigned{context.effect_owners}) | (g.noise.effects & 254U));
    };
    switch (command) {
    case MusicCommand::volume:
        work = 66;
        set(c.volume, byte(a & 127U));
        dirty();
        break;
    case MusicCommand::volume_slide:
        work = slide_work(slide(c.volume, a, byte(b & 127U)), 120, 94, 258);
        if (a == 0)
            dirty();
        break;
    case MusicCommand::pan:
        work = 66;
        set(c.pan, byte(unsigned{a} * 2U));
        dirty();
        break;
    case MusicCommand::pan_slide:
        work = slide_work(slide(c.pan, a, byte(unsigned{b} * 2U)), 116, 94, 258);
        if (a == 0)
            dirty();
        break;
    case MusicCommand::tempo:
        work = 38;
        set(g.tempo, a);
        break;
    case MusicCommand::tempo_slide:
        work = slide_work(slide(g.tempo, a, b), 84, 78, 228);
        break;
    case MusicCommand::echo_volume:
        work = 50;
        set(g.echo_volume, byte((a >> 1U) | (a & 128U)));
        break;
    case MusicCommand::echo_volume_slide:
        work =
            slide_work(slide(g.echo_volume, a, byte((b >> 1U) | (b & 128U)), true), 96, 114, 264);
        break;
    case MusicCommand::song_volume:
        work = 18;
        g.song_volume = a;
        break;
    case MusicCommand::echo_feedback: {
        const auto difference =
            (b < 128 ? int{b} : int{b} - 256) - signed_high(g.echo_feedback.value);
        work = a == 0 ? 64U : 126U + rate_work(difference);
        g.echo_feedback.remaining = a;
        if (a == 0)
            g.echo_feedback.value =
                static_cast<std::uint16_t>((unsigned{b} << 8U) | (g.echo_feedback.value & 255U));
        else
            g.echo_feedback.step = rate(difference, a);
        break;
    }
    case MusicCommand::echo_filter:
        work = a == 0 ? 406U : 1322U;
        g.filter_remaining = a;
        for (std::size_t i = 0; i < g.filter.size(); ++i) {
            const auto target = filters[b & 3U][i];
            auto& tap = g.filter[i];
            if (a == 0)
                tap.value = static_cast<std::uint16_t>(
                    (unsigned{byte(static_cast<unsigned>(target))} << 8U) | (tap.value & 255U));
            else {
                const auto difference = int{target} - signed_high(tap.value);
                work += rate_work(difference);
                tap.step = rate(difference, a);
                tap.value &= 0xff00U;
            }
        }
        break;
    case MusicCommand::pitch_slide:
        work = 54;
        c.pitch_slide_remaining = byte(unsigned{a} + 1U);
        c.pitch_slide_offset = b;
        break;
    case MusicCommand::transpose:
        work = 22;
        c.transpose = a;
        break;
    case MusicCommand::transpose_add:
        work = 36;
        c.transpose = byte(unsigned{c.transpose} + a);
        break;
    case MusicCommand::detune:
        work = 22;
        c.detune = a;
        break;
    case MusicCommand::octave:
        work = 22;
        c.octave = a;
        break;
    case MusicCommand::octave_up:
        work = 44;
        c.octave = byte(unsigned{c.octave} + 1U);
        break;
    case MusicCommand::octave_down:
        work = 36;
        c.octave = byte(unsigned{c.octave} + 255U);
        break;
    case MusicCommand::vibrato:
        work = 358U + initialize_oscillator(c.vibrato, c, instruction.operands);
        c.vibrato_pitch_offset = 0;
        break;
    case MusicCommand::tremolo:
        work = 332U + initialize_oscillator(c.tremolo, c, instruction.operands);
        break;
    case MusicCommand::vibrato_off:
        work = 46;
        c.vibrato.shape = 0;
        c.vibrato_pitch_offset = 0;
        break;
    case MusicCommand::tremolo_off:
        work = 46;
        c.tremolo.shape = 0;
        c.tremolo.position = 0;
        break;
    case MusicCommand::pan_sweep: {
        c.pan_sweep.period = a == 255 ? 0 : byte(std::max(2U, (unsigned{a} + 1U) & 254U));
        c.pan_sweep.remaining = a == 255 ? 128 : byte(c.pan_sweep.period / 2U);
        const unsigned amplitude = b < 128 ? b : (b & 127U) ^ 127U;
        const auto magnitude =
            a == 255 ? amplitude : std::max(1U, amplitude * 256U / c.pan_sweep.remaining);
        work = (a == 255 ? 230U
                : a == 0 ? 328U
                         : 320U) +
               (b < 128 ? 0U : 28U) + (a != 255 && amplitude == 0 ? 4U : 0U);
        c.pan_sweep.step =
            bits(b < 128 ? static_cast<int>(magnitude) : -static_cast<int>(magnitude));
        c.pan_sweep.shape = b;
        c.pan_sweep.position = 0;
        break;
    }
    case MusicCommand::pan_sweep_off:
        work = 50;
        c.pan_sweep.shape = 0;
        c.pan_sweep.position = 0;
        break;
    case MusicCommand::echo_on:
        work = music ? 74 : 70;
        toggle(g.echo, true);
        break;
    case MusicCommand::echo_off:
        work = music ? 80 : 84;
        toggle(g.echo, false);
        break;
    case MusicCommand::modulation_on:
        work = music ? 74 : 70;
        toggle(g.modulation, true);
        break;
    case MusicCommand::modulation_off:
        work = music ? 80 : 84;
        toggle(g.modulation, false);
        break;
    case MusicCommand::noise_on:
        toggle(g.noise, true);
        refresh_noise();
        work = (music ? 28U : 24U) + (g.noise.effects == 0 ? 106U : 102U);
        break;
    case MusicCommand::noise_off:
        toggle(g.noise, false);
        refresh_noise();
        work = (music ? 34U : 38U) + (g.noise.effects == 0 ? 106U : 102U);
        break;
    case MusicCommand::noise_rate:
        (music ? g.music_noise_rate : g.effects_noise_rate) = byte(a & 31U);
        refresh_noise();
        work = (music ? 28U : 32U) + (g.noise.effects == 0 ? 106U : 102U);
        break;
    case MusicCommand::sample:
        work = music ? 144 : 130;
        c.sample_slot = a;
        c.tuning = defaults().tuning;
        c.envelope = defaults().envelope;
        if (music)
            g.ignore_song_volume = byte(g.ignore_song_volume & ~unsigned{mask});
        break;
    case MusicCommand::attack:
        c.envelope.attack = byte(a & 15U);
        c.envelope.enabled = true;
        envelope_request(48);
        break;
    case MusicCommand::decay:
        c.envelope.decay = byte(a & 7U);
        c.envelope.enabled = true;
        envelope_request(66);
        break;
    case MusicCommand::sustain_level:
        c.envelope.sustain_level = byte(a & 7U);
        envelope_request(66);
        break;
    case MusicCommand::sustain_rate:
        c.envelope.sustain_rate = byte(a & 31U);
        envelope_request(52);
        break;
    case MusicCommand::default_envelope:
        c.envelope = defaults().envelope;
        envelope_request(70);
        break;
    case MusicCommand::ignore_song_volume:
        work = 22;
        g.ignore_song_volume = byte(g.ignore_song_volume | mask);
        break;
    case MusicCommand::slur_on:
        work = music ? 68 : 64;
        c.slur = true;
        c.slurred_note = false;
        c.drum_roll = false;
        break;
    case MusicCommand::drum_roll_on:
        work = music ? 56 : 52;
        c.drum_roll = true;
        c.slur = false;
        break;
    case MusicCommand::output_increment:
        work = 18;
        g.output_code = byte(unsigned{g.output_code} + 1U);
        break;
    case MusicCommand::output_clear:
        work = 20;
        g.output_code = 0;
        break;
    default:
        throw std::runtime_error("Unimplemented driver control command");
    }
    result.work = static_cast<std::uint16_t>(work);
    channel = c;
    global = g;
    return result;
}
} // namespace ff6
