#include "ff6/note_generation.hpp"
#include "driver_oscillator.hpp"
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
std::uint8_t byte(std::uint32_t value) {
    return static_cast<std::uint8_t>(value & 255U);
}
std::uint16_t word(std::uint32_t value) {
    return static_cast<std::uint16_t>(value & 65535U);
}
unsigned frequency_work(std::uint8_t note, std::array<std::uint8_t, 2> tuning,
                        std::uint8_t detune) {
    const auto octave = note / 12U;
    // Both tuning stages truncate separately. Work follows secondary presence,
    // primary sign, and the original number of octave shifts, even after zero.
    return (octave <= 4 ? 324U + 34U * (4U - octave) : 314U + 34U * (octave - 4U)) +
           (tuning[1] == 0 ? 0U : 34U) + (byte(std::uint32_t{tuning[0]} + detune) < 128 ? 6U : 0U);
}
unsigned restart_work(const DriverOscillator& oscillator) {
    const auto shape = oscillator.shape & 0xc0U;
    return (oscillator.delay == 0 ? 64U : 96U) + (shape == 0x80 ? 42U : shape == 0xc0 ? 14U : 0U);
}
} // namespace

NotePitchTable decode_note_pitches(ByteView bytes) {
    require(bytes.size() == 26, "Invalid note pitch table extent");
    NotePitchTable result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = read16(bytes, 2 * i);
    return result;
}
NotePitchTable load_note_pitches(const ContentPack& pack) {
    return decode_note_pitches(pack.get("audio_note_pitch_table"));
}
std::uint16_t note_frequency(const NotePitchTable& pitches, std::uint8_t note,
                             std::array<std::uint8_t, 2> tuning, std::uint8_t detune) {
    const auto multiplier = pitches[note % 12U];
    const auto primary = byte(std::uint32_t{tuning[0]} + detune);
    std::uint32_t value = std::uint32_t{multiplier} * primary / 256U;
    // Secondary tuning uses the high byte of the already-truncated product.
    // Folding these stages into a single multiply changes note frequencies.
    value += (value >> 8U) * tuning[1] / 256U;
    if (primary < 128)
        value += multiplier;
    value &= 65535U;
    const auto octave = note / 12U;
    if (octave < 4)
        return word(value >> (4U - octave));
    const auto shift = octave - 4U;
    return shift >= 16 ? 0 : word(value << shift);
}
DriverNoteResult apply_driver_note(std::uint8_t code,
                                   const std::optional<ScriptLookahead>& lookahead,
                                   const DriverNoteContext& context, const NotePitchTable& pitches,
                                   DriverNoteState& note, DriverChannelControls& channel,
                                   DriverGlobalControls& global, DriverNoteQueues& queues) {
    const bool music = context.channel.kind == SequenceKind::music;
    require(code < 0xc4, "Not a note token");
    require((music || context.channel.kind == SequenceKind::effect) && context.channel.voice < 8 &&
                (music || context.channel.voice >= 4) && (context.effect_owners & 15U) == 0,
            "Invalid note channel or ownership");
    require(channel.vibrato.gain_remaining < 8 && channel.tremolo.gain_remaining < 8 &&
                note.pitch.step <= 0xffffffU,
            "Invalid note motion state");
    const auto& envelope = channel.envelope;
    require(envelope.attack < 16 && envelope.decay < 8 && envelope.sustain_level < 8 &&
                envelope.sustain_rate < 32,
            "Invalid note envelope");
    require(lookahead.has_value() == (code < 0xb6), "Invalid note lookahead presence");
    if (lookahead)
        require(lookahead->next_code < 0xc4 || lookahead->next_code == 0xeb,
                "Invalid next-note token");
    if (code >= 0xb6)
        return {{}, 42, {}, {}};

    const auto mask = byte(1U << context.channel.voice);
    DriverNoteResult result;
    unsigned work = 34; // A tie jumps directly to lookahead.
    if (code < 0xa8) {
        note.note_index =
            byte(std::uint32_t{channel.octave} * 12U + code / 14U + channel.transpose + 256U - 10U);
        note.pitch.base = note_frequency(pitches, note.note_index, channel.tuning, channel.detune);
        work =
            (music ? 314U : 318U) + frequency_work(note.note_index, channel.tuning, channel.detune);
        if (channel.vibrato.shape != 0) {
            work += 242U + restart_work(channel.vibrato);
            detail::restart_oscillator(channel.vibrato, channel.vibrato.gain_remaining);
        }
        if (channel.tremolo.shape != 0) {
            work += 222U + restart_work(channel.tremolo);
            detail::restart_oscillator(channel.tremolo, channel.vibrato.gain_remaining);
        }
        channel.vibrato_pitch_offset = 0;
        channel.tremolo.position = 0;
        // Only the integer portion of the old slide rate is cleared. Its low
        // byte and the fractional position survive until a new slide replaces them.
        note.pitch.step &= 255U;
        if (music)
            queues.music_started = byte(queues.music_started | mask);
        if (music)
            work += (context.effect_owners & mask) != 0 ? 38U : 34U;
        if (!music || (context.effect_owners & mask) == 0) {
            bool retrigger = true;
            work += channel.drum_roll      ? 20U
                    : !channel.slur        ? 36U
                    : channel.slurred_note ? 46U
                    : music                ? 62U
                                           : 54U;
            if (channel.slur && !channel.drum_roll) {
                retrigger = !channel.slurred_note;
                channel.slurred_note = true;
            }
            if (retrigger) {
                queues.starts = byte(queues.starts | mask);
                work += 12;
            }
            global.volume_dirty = byte(global.volume_dirty | mask);
            global.pitch_dirty = byte(global.pitch_dirty | mask);
            result.sample = DriverSampleRequest{channel.sample_slot, channel.envelope};
            work += 40; // Dirty flags and sample-publication CALL.
            result.sample_offset = static_cast<std::uint16_t>(work);
            work += music ? 218U : 196U;
        }
    }
    work += 16; // Lookahead CALL; the native scan owns its body and return.
    result.lookahead_offset = static_cast<std::uint16_t>(work);
    channel.slur = lookahead->articulation.slur;
    channel.drum_roll = lookahead->articulation.drum_roll;
    const auto next = lookahead->next_code;
    if (next >= 0xb6)
        channel.slur = channel.drum_roll = false;
    note.hold = (next >= 0xa8 && next < 0xb6) || channel.slur || channel.drum_roll;
    work += next >= 0xb6   ? (music ? 104U : 100U)
            : next >= 0xa8 ? (music ? 54U : 58U)
                           : (music ? 88U : 84U);

    if (channel.pitch_slide_offset != 0) {
        note.note_index = byte(std::uint32_t{note.note_index} + channel.pitch_slide_offset);
        const auto target =
            note_frequency(pitches, note.note_index, channel.tuning, channel.detune);
        const bool descending = target < note.pitch.base;
        const std::uint32_t difference =
            descending ? note.pitch.base - target : target - note.pitch.base;
        // Zero duration deliberately keeps the unscaled difference. It is not
        // an immediate jump, nor a division by zero or a duration of 256.
        const auto magnitude = channel.pitch_slide_remaining == 0
                                   ? difference
                                   : difference * 256U / channel.pitch_slide_remaining;
        work += 288U + frequency_work(note.note_index, channel.tuning, channel.detune) +
                (channel.pitch_slide_remaining == 0 ? 0U : 106U) +
                (descending ? 74U + ((magnitude & 65535U) == 0 ? 4U : 0U) : 0U);
        note.pitch.step = descending ? (0x1000000U - magnitude) & 0xffffffU : magnitude;
        note.pitch.fraction = 0;
        channel.pitch_slide_offset = 0;
    } else
        work += 28;
    result.work = static_cast<std::uint16_t>(work);
    return result;
}
} // namespace ff6
