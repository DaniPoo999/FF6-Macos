#include "ff6/driver_loading.hpp"
#include <bit>
#include <stdexcept>
#include "rom-materials/audio_loading_choices.macros.inc"
#include "rom-materials/audio_initial_song_tempo.macros.inc"

namespace ff6 {
namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
std::uint8_t byte(unsigned value) {
    return static_cast<std::uint8_t>(value & 255U);
}
std::uint8_t mask(std::uint8_t voice) {
    return byte(1U << voice);
}
std::uint16_t upper(std::uint16_t value, std::uint8_t replacement) {
    return static_cast<std::uint16_t>((value & 255U) | (unsigned{replacement} << 8U));
}
void validate(const DriverState& state) {
    const auto& o = state.playback.ownership;
    require(((o.effects() | o.paused_effects) & 15U) == 0 &&
                (o.game_effects & o.system_effects) == 0 && std::popcount(o.game_effects) <= 2 &&
                std::popcount(o.paused_effects) <= 2,
            "Invalid driver ownership");
    require((!o.song || *o.song < 85) && (!o.suspended_song || *o.suspended_song < 85),
            "Invalid driver song identity");
    require(!state.loaded_song || *state.loaded_song < 85, "Invalid loaded song identity");
    require(!o.song || o.song == state.loaded_song,
            "Playing song has no matching loaded resources");
    require(state.global.music_noise_rate < 32 && state.global.effects_noise_rate < 32 &&
                state.global.selected_noise_rate < 32,
            "Invalid driver noise rate");
    for (const auto* bank : {&state.music, &state.effects})
        for (unsigned voice = bank == &state.music ? 0U : 4U; voice < 8; ++voice) {
            const auto& c = bank->controls[voice];
            const auto& flow = bank->scripts[voice];
            const auto valid_position = [](const std::optional<ScriptPosition>& p) {
                return !p || p->region <= ScriptRegion::system_effects;
            };
            require(valid_position(flow.position), "Invalid driver script region");
            for (const auto& loop : flow.loops)
                require(valid_position(loop.start), "Invalid driver loop region");
            require(c.vibrato.gain_remaining < 8 && c.tremolo.gain_remaining < 8 &&
                        c.envelope.attack < 16 && c.envelope.decay < 8 &&
                        c.envelope.sustain_level < 8 && c.envelope.sustain_rate < 32 &&
                        bank->notes[voice].pitch.step <= 0xffffffU && flow.loop_slot < 4,
                    "Invalid driver channel state");
        }
}
void initialize(DriverChannels& bank, std::uint8_t voice, ScriptPosition entry) {
    auto& c = bank.controls[voice];
    auto& flow = bank.scripts[voice];
    flow.position = entry;
    flow.loop_slot = 0;
    flow.wait = 1;
    c.pitch_slide_offset = 0;
    bank.notes[voice].pitch.step = 0;
    c.vibrato.shape = c.tremolo.shape = c.pan_sweep.shape = 0;
    c.pan_sweep.position = 0;
    c.detune = c.transpose = 0;
}
std::uint16_t terminate_effect(std::uint8_t voice, DriverState& state) {
    terminate_playback_channel({SequenceKind::effect, voice}, state.playback, state.global,
                               state.music.controls);
    // Includes the helper's return. Remaining raw noise ownership selects the
    // rate path after each termination, including bit zero (PLAYBACK_WORK.md).
    return state.global.noise.effects ? 400 : 404;
}
void clear_effect_articulation(std::uint8_t voices, DriverState& state) {
    for (std::uint8_t voice = 4; voice < 8; ++voice)
        if ((voices & mask(voice)) != 0) {
            state.effects.notes[voice].hold = false;
            state.effects.controls[voice].slur = false;
            state.effects.controls[voice].drum_roll = false;
        }
}
void suppress_selected(std::uint8_t voices, DriverGlobalControls& global) {
    for (auto* routing : {&global.echo, &global.noise, &global.modulation})
        routing->selected = byte(routing->selected & ~unsigned{voices});
}
void entry_valid(const std::optional<ScriptPosition>& entry, ScriptRegion region) {
    require(!entry || entry->region == region, "Invalid effect entry region");
}
SuspendedDriverSong snapshot(const DriverState& state) {
    auto global = state.global;
    global.volume_dirty = global.pitch_dirty = 0;
    global.echo.selected = global.noise.selected = global.modulation.selected = 0;
    global.selected_noise_rate = 0;
    return {*state.playback.ownership.song,
            state.playback.ownership.music,
            state.music,
            state.effects,
            global,
            state.queues,
            state.playback.releases,
            state.music_phase,
            state.effects_phase};
}
void restore(const SuspendedDriverSong& saved, DriverState& state) {
    const auto current = state.global;
    state.music = saved.music;
    state.effects = saved.effects;
    state.global = saved.global;
    state.global.volume_dirty = current.volume_dirty;
    state.global.pitch_dirty = current.pitch_dirty;
    state.global.echo.selected = current.echo.selected;
    state.global.noise.selected = current.noise.selected;
    state.global.modulation.selected = current.modulation.selected;
    state.global.selected_noise_rate = current.selected_noise_rate;
    state.queues = saved.queues;
    state.playback.releases = saved.releases;
    state.playback.ownership.music = saved.enabled_music;
    state.music_phase = saved.music_phase;
    state.effects_phase = saved.effects_phase;
}
} // namespace

DriverLoadRequests load_driver_song(const SongScript& song, SongLoad request, DriverState& state,
                                    std::optional<SuspendedDriverSong>& suspended) {
    validate(state);
    require(request.identity < 85, "Unknown driver song");
    require(state.playback.ownership.suspended_song ==
                (suspended ? std::optional<std::uint8_t>{suspended->identity} : std::nullopt),
            "Inconsistent suspended song identity");
    // All transitions are prepared on values; invalid restored state cannot
    // damage the previous playing state or its sole suspended snapshot.
    auto next = state;
    auto saved = suspended;
    auto& o = next.playback.ownership;
    auto& c = next.playback.controls;
    auto& g = next.global;
    DriverLoadRequests result{true, true, std::nullopt, {}};
    // Aggregate semantic work, not host execution time. A full suspension copies
    // retained state even when the current identity is invalid; only song 0x2f
    // bypasses it. The transfer wait is deliberately not assigned a duration.
    result.work.release_offset = (request.alternate ? 10U : 18U) + 24U;
    if (request.suspend_current)
        result.work.release_offset += o.song == FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_0 ? 44U : 49518U;
    result.work.content_wait_offset = result.work.release_offset + 82U;
    result.work.preparation = 72;
    if (request.suspend_current && o.song != FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_0) {
        saved = o.song ? std::optional<SuspendedDriverSong>{snapshot(next)} : std::nullopt;
        o.suspended_song = o.song;
    }
    o.song = request.identity;
    next.loaded_song = request.identity;
    c.music_volume.value = upper(c.music_volume.value, request.volume);
    c.music_volume.remaining = 0;
    o.game_effects = o.system_effects = o.paused_music = o.paused_effects = 0;
    next.forced_progression = c.fade_status = false;
    c.conditional = 0;
    bool resumed = false;
    if (saved && saved->identity == request.identity) {
        restore(*saved, next);
        saved.reset();
        o.suspended_song.reset();
        validate(next);
        // ResumeSong's authored fade choices are private ROM-derived inputs.
        c.music_volume.value = upper(c.music_volume.value, FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_1);
        const auto ramp =
            apply_playback_command(PlaybackCommand::music_volume,
                                   FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_2,
                                   FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_3, next.playback, g,
                                   next.queues, next.music.controls);
        // Restore includes the native ramp's existing work, exactly once.
        result.work.preparation += 49546U + ramp.work;
        resumed = o.music != 0;
        if (!resumed)
            result.work.preparation += 4;
    }
    if (resumed) {
        result.work.preparation += 70;
        g.selected_noise_rate = g.music_noise_rate;
        result.resume.emplace();
        for (std::uint8_t voice = 0; voice < 8; ++voice) {
            auto& flow = next.music.scripts[voice];
            flow.wait = byte(unsigned{flow.wait} + 1U);
            const auto& channel = next.music.controls[voice];
            const auto pitch = prepare_driver_pitch(
                playback_output_policy({SequenceKind::music, voice}, next.playback), channel,
                next.music.notes[voice].pitch);
            (*result.resume)[voice] = {{channel.sample_slot, channel.envelope},
                                       pitch.coefficient,
                                       pitch.publication_delay};
        }
        g.echo.effects = g.noise.effects = g.modulation.effects = 0;
        next.queues.starts = next.queues.music_started;
        next.playback.releases = 0;
    } else {
        // Entry relocation and the complete eight-slot scan include absent
        // channels. Each enabled channel adds partial initialization work.
        result.work.preparation += 1266;
        o.music = 0;
        g.echo.music = g.echo.effects = g.noise.music = g.noise.effects = 0;
        g.modulation.music = g.modulation.effects = 0;
        next.queues = {};
        next.playback.releases = 0;
        g.ignore_song_volume = 0;
        g.tempo.value = upper(g.tempo.value, FF6_ROM_VALUE_AUDIO_INITIAL_SONG_TEMPO_0);
        next.music_phase = 255;
        g.song_volume = 255;
        g.output_code = 0;
        for (std::uint8_t voice = 0; voice < 8; ++voice) {
            next.music.notes[voice].hold = false;
            next.music.controls[voice].slur = next.music.controls[voice].drum_roll = false;
            const auto entry = song.entry(voice, request.alternate);
            next.music.scripts[voice].position = entry;
            if (entry) {
                result.work.preparation += 232;
                o.music = byte(o.music | mask(voice));
                initialize(next.music, voice, *entry);
            }
        }
    }
    g.echo.selected = g.echo.music;
    g.noise.selected = g.noise.music;
    g.modulation.selected = g.modulation.music;
    g.volume_dirty = g.pitch_dirty = 0;
    state = next;
    suspended = saved;
    return result;
}

std::uint16_t start_driver_effect(std::optional<ScriptPosition> first,
                                  std::optional<ScriptPosition> second, std::uint8_t pan,
                                  const DriverSampleDefaults& defaults, DriverState& state) {
    validate(state);
    entry_valid(first, ScriptRegion::game_effects);
    entry_valid(second, ScriptRegion::game_effects);
    require(defaults.slot == FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_7 && defaults.envelope.attack < 16 && defaults.envelope.decay < 8 &&
                defaults.envelope.sustain_level < 8 && defaults.envelope.sustain_rate < 32,
            "Invalid ordinary-effect defaults");
    if (state.playback.ownership.paused())
        return 24;
    auto next = state;
    auto& o = next.playback.ownership;
    const auto candidates = o.game_effects ? o.game_effects : byte(o.system_effects ^ 0xf0U);
    std::uint8_t voice = 7;
    while (voice > 5 && (candidates & mask(voice)) == 0)
        --voice;
    // Fixed work includes both absent-entry decisions and all four cleanup
    // slots. Allocation may stop at the fallback without checking voice five.
    unsigned work = (o.game_effects ? 566U : 572U) + (voice == 7 ? 20U : voice == 6 ? 66U : 88U);
    std::uint8_t started = 0;
    for (const auto& entry : {second, first}) {
        if (!entry)
            continue;
        initialize(next.effects, voice, *entry);
        next.effects.scripts[voice].wait = 2;
        auto& channel = next.effects.controls[voice];
        // Interrupt_18 chooses the initial effect voice settings. Keep those
        // operands out of source while retaining native semantic processing.
        channel.volume.value = upper(channel.volume.value, FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_4);
        channel.pan.value = upper(channel.pan.value, FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_5);
        channel.octave = FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_6;
        channel.volume.remaining = channel.pan.remaining = 0;
        work += 522U + terminate_effect(voice, next);
        channel.sample_slot = defaults.slot;
        channel.tuning = defaults.tuning;
        channel.envelope = defaults.envelope;
        started = byte(started | mask(voice));
        --voice;
    }
    const auto stopped = byte(o.game_effects | started);
    next.queues.starts = byte(next.queues.starts & ~unsigned{stopped});
    next.playback.releases = byte(next.playback.releases | stopped);
    clear_effect_articulation(stopped, next);
    const auto old = o.game_effects;
    for (std::uint8_t v = 8; v > 4;) {
        --v;
        if ((old & mask(v)) != 0)
            work += 12U + terminate_effect(v, next);
    }
    o.game_effects = started;
    suppress_selected(started, next.global);
    next.playback.controls.effect_pan.value = upper(next.playback.controls.effect_pan.value, pan);
    state = next;
    return static_cast<std::uint16_t>(work);
}

std::uint16_t start_driver_system_effect(std::optional<ScriptPosition> entry, DriverState& state) {
    validate(state);
    entry_valid(entry, ScriptRegion::system_effects);
    if (state.playback.ownership.paused())
        return 28;
    auto next = state;
    auto& o = next.playback.ownership;
    const bool replacing = o.effects() == 0xf0;
    const auto candidates = replacing ? o.system_effects : byte(o.effects() ^ 0xf0U);
    require(candidates != 0, "No system-effect replacement voice");
    std::uint8_t voice = 7;
    while ((candidates & mask(voice)) == 0)
        --voice;
    unsigned work = replacing ? 100U + 46U * (7U - voice) : 88U + 38U * (7U - voice);
    if (!entry)
        return static_cast<std::uint16_t>(work + 28U);
    initialize(next.effects, voice, *entry);
    next.effects.scripts[voice].wait = 2;
    work += 388U + terminate_effect(voice, next);
    const auto selected = mask(voice);
    clear_effect_articulation(selected, next);
    next.playback.releases = byte(next.playback.releases | selected);
    next.queues.starts = byte(next.queues.starts & ~unsigned{selected});
    suppress_selected(selected, next.global);
    o.system_effects = byte(o.system_effects | selected);
    state = next;
    return static_cast<std::uint16_t>(work);
}
} // namespace ff6
