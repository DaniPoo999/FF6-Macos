#include "ff6/sequencing.hpp"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/audio_cadence_choices.macros.inc"
#include "rom-materials/audio_note_release_lead.macros.inc"

namespace ff6 {
namespace {
bool accumulate(std::uint8_t& phase, std::uint8_t increment) {
    const auto sum = unsigned{phase} + increment;
    phase = static_cast<std::uint8_t>(sum & 255U);
    return sum > 255U;
}
void validate_effect_voices(std::uint8_t voices) {
    if ((voices & 15U) != 0)
        throw std::invalid_argument("Effects may occupy only voices four through seven");
}
void append(SequenceOrder& order, SequenceKind kind, unsigned voice) {
    order.channels[order.count++] = {kind, static_cast<std::uint8_t>(voice)};
}
} // namespace

SequenceTick begin_sequence_tick(std::uint8_t& control_countdown, bool paused) {
    if (paused)
        return {};
    // An imported zero countdown wraps; it does not fire immediately.
    control_countdown = static_cast<std::uint8_t>((unsigned{control_countdown} + 255U) & 255U);
    const bool due = control_countdown == 0;
    if (due)
        control_countdown = FF6_ROM_VALUE_AUDIO_CADENCE_CHOICES_1;
    return {true, due};
}

std::uint8_t adjusted_music_tempo(std::uint8_t tempo, std::uint8_t modifier) {
    if (modifier == 0)
        return tempo;
    const auto scaled = (unsigned{tempo} * modifier) / 256U;
    // The high-bit branch is a different operation, not signed arithmetic.
    const auto value =
        modifier < 128 ? std::min(255U, unsigned{tempo} + 2U * scaled) : std::max(1U, scaled);
    return static_cast<std::uint8_t>(value);
}

SequenceServiceSelection begin_sequence_service(std::uint8_t& control_countdown, bool paused,
                                                bool echo_enabled, bool fade_status) {
    SequenceServiceSelection result;
    result.tick = begin_sequence_tick(control_countdown, paused);
    result.buffered_loop_end = echo_enabled ? 986 : 298;
    // FFVI's alternate fade-status report uses a shorter overlapped encoding.
    // Preserve its aggregate duration without exposing executable bytes or ports.
    // See SERVICE_BEGIN_WORK.md for the exact semantic boundaries.
    result.status_end =
        static_cast<std::uint16_t>(result.buffered_loop_end + (fade_status ? 90U : 94U));
    const unsigned selection = paused ? 42U : result.tick.advance_control_envelopes ? 52U : 46U;
    result.work = static_cast<std::uint16_t>(result.status_end + selection);
    return result;
}

SequencePhaseResult advance_music_phase(std::uint8_t& phase, std::uint8_t tempo,
                                        std::uint8_t modifier, bool forced) {
    if (forced)
        return {true, 14};
    // FFVI's modifier paths have distinct costs. The high-bit zero-product
    // correction balances its shorter branch, so both cases take 70 ticks.
    // See SCRIPT_PASS_WORK.md; these are semantic path totals, not CPU execution.
    const auto scaled = (unsigned{tempo} * modifier) / 256U;
    const unsigned adjustment = modifier == 0                          ? 30U
                                : modifier >= 128                      ? 70U
                                : unsigned{tempo} + 2U * scaled > 255U ? 88U
                                                                       : 80U;
    const bool due = accumulate(phase, adjusted_music_tempo(tempo, modifier));
    return {due, static_cast<std::uint16_t>(adjustment + (due ? 22U : 26U))};
}
SequencePhaseResult advance_effects_phase(std::uint8_t& phase) {
    const bool due = accumulate(phase, FF6_ROM_VALUE_AUDIO_CADENCE_CHOICES_2);
    return {due, static_cast<std::uint16_t>(due ? 26 : 30)};
}

SequenceWaitResult advance_sequence_wait(SequenceWait& wait, SequenceKind kind, bool held,
                                         bool owns_voice) {
    if (kind != SequenceKind::music && kind != SequenceKind::effect)
        throw std::invalid_argument("Unknown sequencing channel kind");
    wait.remaining = static_cast<std::uint8_t>((unsigned{wait.remaining} + 255U) & 255U);
    if (wait.remaining == 0)
        return {SequenceAction::execute_commands, 38};
    const bool music = kind == SequenceKind::music;
    if (wait.remaining != FF6_ROM_VALUE_AUDIO_NOTE_RELEASE_LEAD_0)
        return {SequenceAction::sustain, static_cast<std::uint16_t>(music ? 62 : 66)};
    if (held)
        return {SequenceAction::sustain, static_cast<std::uint16_t>(music ? 82 : 86)};
    if (kind == SequenceKind::music)
        wait.music_playing = false;
    if (kind == SequenceKind::effect || owns_voice)
        wait.release_pending = true;
    // A shadowed music note still clears its own playing bit, but must not
    // release the effect occupying that voice. See SCRIPT_CHANNEL_WORK.md.
    return {SequenceAction::release,
            static_cast<std::uint16_t>(music ? (owns_voice ? 138 : 130) : 106)};
}

SequenceOrder music_sequence_order(std::uint8_t enabled_music) {
    SequenceOrder order;
    for (unsigned voice = 0; voice < 8; ++voice)
        if ((enabled_music & (1U << voice)) != 0)
            append(order, SequenceKind::music, voice);
    return order;
}
SequenceOrder effects_sequence_order(std::uint8_t effect_voices) {
    validate_effect_voices(effect_voices);
    SequenceOrder order;
    for (unsigned voice = 4; voice < 8; ++voice)
        if ((effect_voices & (1U << voice)) != 0)
            append(order, SequenceKind::effect, voice);
    return order;
}
ScriptTraversalPlan script_traversal_plan(SequenceKind kind, std::uint8_t enabled) {
    if (kind != SequenceKind::music && kind != SequenceKind::effect)
        throw std::invalid_argument("Unknown script traversal kind");
    ScriptTraversalPlan plan;
    const bool music = kind == SequenceKind::music;
    plan.order = music ? music_sequence_order(enabled) : effects_sequence_order(enabled);
    unsigned pending = music ? 32U : 52U;
    unsigned cursor = music ? 0U : 4U;
    for (std::uint8_t i = 0; i < plan.order.count; ++i) {
        const unsigned voice = plan.order.channels[i].voice;
        // The captured mask is scanned only through its last enabled slot.
        // Empty scans and the last scan remain observable work. See SCRIPT_PASS_WORK.md.
        plan.before[i] = static_cast<std::uint16_t>(pending + 16U + 36U * (voice - cursor));
        pending = 16;
        cursor = voice + 1;
    }
    plan.after = static_cast<std::uint16_t>(pending + (music ? 40U : 38U));
    return plan;
}
SequenceOrder voice_update_order(std::uint8_t enabled_music, std::uint8_t playing_music,
                                 std::uint8_t effect_voices) {
    validate_effect_voices(effect_voices);
    auto order = music_sequence_order(
        static_cast<std::uint8_t>(enabled_music & playing_music & (effect_voices ^ 255U)));
    for (unsigned reverse = 0; reverse < 4; ++reverse) {
        const auto voice = 7U - reverse;
        if ((effect_voices & (1U << voice)) != 0)
            append(order, SequenceKind::effect, voice);
    }
    return order;
}
VoiceUpdatePlan voice_update_plan(std::uint8_t enabled_music, std::uint8_t playing_music,
                                  std::uint8_t effect_voices) {
    VoiceUpdatePlan plan;
    plan.order = voice_update_order(enabled_music, playing_music, effect_voices);
    unsigned pending = 58, music_cursor = 0, effect_cursor = 7;
    bool effects = false;
    for (std::uint8_t i = 0; i < plan.order.count; ++i) {
        const auto channel = plan.order.channels[i];
        if (!effects && channel.kind == SequenceKind::effect) {
            pending += 58;
            effects = true;
        }
        const unsigned skipped =
            effects ? effect_cursor - channel.voice : channel.voice - music_cursor;
        // Aggregate FFVI traversal work. Selected and skipped slots differ;
        // each phase also performs a final empty scan. No instruction execution
        // or device state is involved. See VOICE_TRAVERSAL.md.
        plan.before[i] = static_cast<std::uint16_t>(pending + 40 + 36 * skipped);
        pending = 26;
        if (effects)
            effect_cursor = channel.voice - 1U;
        else
            music_cursor = channel.voice + 1U;
    }
    if (!effects)
        pending += 58;
    plan.after = static_cast<std::uint16_t>(pending + 40);
    return plan;
}
} // namespace ff6
