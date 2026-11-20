#include "ff6/driver_sequence.hpp"
#include <stdexcept>

#include "rom-materials/audio_loading_choices.macros.inc"

namespace ff6 {
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
std::uint8_t upper(std::uint16_t value) {
    return static_cast<std::uint8_t>(value >> 8U);
}
std::uint8_t bit(std::uint8_t voice) {
    return static_cast<std::uint8_t>(1U << voice);
}
void set_bit(std::uint8_t& value, std::uint8_t mask, bool enabled) {
    value = static_cast<std::uint8_t>(enabled ? (value | mask) : (value & (mask ^ 255U)));
}
DriverChannels& channels(DriverState& s, SequenceKind kind) {
    return kind == SequenceKind::music ? s.music : s.effects;
}
void clear_dirty(DriverState& state) {
    state.global.volume_dirty = state.global.pitch_dirty = 0;
}
} // namespace
DriverVoicePass advance_driver_voices(DriverState& state) {
    auto next = state;
    const auto plan = voice_update_plan(next.playback.ownership.music, next.queues.music_started,
                                        next.playback.ownership.effects());
    DriverVoicePass result;
    result.updates.reserve(plan.order.count);
    for (std::uint8_t i = 0; i < plan.order.count; ++i) {
        const auto id = plan.order.channels[i];
        auto& bank = channels(next, id.kind);
        result.updates.push_back(
            {id, plan.before[i],
             update_driver_voice(playback_output_policy(id, next.playback), bank.controls[id.voice],
                                 bank.notes[id.voice].pitch, next.global)});
    }
    result.after = plan.after;
    clear_dirty(next);
    state = next;
    return result;
}
DriverSequencer::DriverSequencer(const MusicScripts& scripts, const SampleBank& samples,
                                 EchoFilterPresets filters, NotePitchTable pitches)
    : scripts_(scripts), samples_(samples), filters_(filters), pitches_(pitches) {
    // Explicit cold initialization: original Init clears only its selected state.
    // Higher channel history is defined here by the approved zero-power-on profile.
    state_.playback.releases = 255;
    state_.playback.controls.effect_volume.value = 0xff00;
}
void DriverSequencer::require_command_boundary() const {
    require(stage_ != Stage::effects, "Command outside a driver polling boundary");
}
DriverSampleDefaults DriverSequencer::defaults(std::uint8_t slot, const DriverState& s) const {
    // Metadata indexing originally doubles a byte. Preserve that alias without
    // treating a script slot as an address or silently inventing sample storage.
    const auto metadata = static_cast<std::uint8_t>(slot & 127U);
    const auto identity = resolve_sample_slot(samples_, s.loaded_song, metadata);
    require(identity.has_value(), "Unassigned sample metadata");
    return driver_sample_defaults(slot, sample_asset(samples_, *identity));
}
DriverSongOutput DriverSequencer::load(SongLoad request) {
    require_command_boundary();
    auto next = state_;
    auto saved = suspended_;
    const auto requests = load_driver_song(scripts_.song(request.identity), request, next, saved);
    DriverSongOutput result{requests.release_all, requests.abandon_service, {}, requests.work};
    if (requests.resume) {
        result.resume.emplace();
        for (std::size_t i = 0; i < 8; ++i) {
            const auto& voice = (*requests.resume)[i];
            const auto identity =
                resolve_sample_slot(samples_, next.loaded_song, voice.sample.slot);
            (*result.resume)[i] = {
                {identity, voice.sample.envelope}, voice.pitch, voice.pitch_work};
        }
    }
    // Resolution is part of the transaction: malformed resource selection must
    // not replace the current state or its suspended snapshot.
    state_ = next;
    suspended_ = saved;
    stage_ = Stage::idle;
    return result;
}
std::uint16_t DriverSequencer::effect(std::uint8_t identity, std::uint8_t pan) {
    require_command_boundary();
    return start_driver_effect(scripts_.effect(identity, false), scripts_.effect(identity, true),
                               pan, driver_sample_defaults(FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_7,
                                   samples_.effects[FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_7]), state_);
}
std::uint16_t DriverSequencer::system_effect(std::uint8_t identity) {
    require_command_boundary();
    return start_driver_system_effect(scripts_.system_effect(identity), state_);
}
PlaybackRequests DriverSequencer::command(PlaybackCommand command, std::uint8_t duration,
                                          std::uint8_t value) {
    require_command_boundary();
    auto result = apply_playback_command(command, duration, value, state_.playback, state_.global,
                                         state_.queues, state_.music.controls);
    if (!state_.playback.ownership.suspended_song)
        suspended_.reset();
    if (result.abandon_service)
        stage_ = Stage::idle;
    return result;
}
void DriverSequencer::set_forced_progression(bool enabled) {
    require_command_boundary();
    state_.forced_progression = enabled;
}
void DriverSequencer::set_echo_enabled(bool enabled) {
    require_command_boundary();
    echo_enabled_ = enabled;
}
std::uint8_t DriverSequencer::set_echo_delay(std::uint8_t delay) {
    require_command_boundary();
    require(delay < 16, "Invalid echo delay");
    echo_delay_ = delay;
    if (state_.playback.ownership.music == 0)
        state_.global.echo_volume.value &= 255U;
    return delay;
}
DriverChannelVisit DriverSequencer::advance_channel(SequenceChannel id, DriverState& s,
                                                    std::vector<DriverVoiceIntent>& output) const {
    DriverChannelVisit visit;
    visit.channel = id;
    auto& bank = channels(s, id.kind);
    auto& cursor = bank.scripts[id.voice];
    auto& controls = bank.controls[id.voice];
    auto& note = bank.notes[id.voice];
    const auto mask = bit(id.voice);
    SequenceWait wait{cursor.wait, (s.queues.music_started & mask) != 0,
                      (s.playback.releases & mask) != 0};
    const auto action = advance_sequence_wait(wait, id.kind, note.hold,
                                              (s.playback.ownership.effects() & mask) == 0);
    visit.wait_work = action.work;
    std::uint32_t work = action.work;
    cursor.wait = wait.remaining;
    set_bit(s.queues.music_started, mask, wait.music_playing);
    set_bit(s.playback.releases, mask, wait.release_pending);
    if (action.action == SequenceAction::execute_commands) {
        require(cursor.position.has_value(), "Active driver channel has no script");
        require(s.loaded_song.has_value(), "Script progression requires loaded song context");
        ScriptContext context{*s.loaded_song, id.kind, {}};
        for (std::size_t i = 0; i < 4; ++i)
            context.music_loop_remaining[i] = s.music.scripts[id.voice].loops[i].remaining;
        ScriptFlow flow{
            *cursor.position, cursor.loops, cursor.loop_slot,
            cursor.wait,      true,         (s.playback.controls.conditional & mask) != 0};
        bool yielded = false, null_redirect = false;
        for (std::size_t count = 0; count < 4096 && !yielded; ++count) {
            const auto step = step_script(scripts_, context, flow);
            work += step.work;
            set_bit(s.playback.controls.conditional, mask, flow.conditional_pending);
            if (step.kind == ScriptStepKind::stopped) {
                null_redirect =
                    step.instruction.code == static_cast<std::uint8_t>(MusicCommand::effect_a) ||
                    step.instruction.code == static_cast<std::uint8_t>(MusicCommand::effect_b);
                work += terminate_playback_channel(id, s.playback, s.global, s.music.controls,
                                                   TerminationOrigin::script);
                yielded = true;
            } else if (step.kind == ScriptStepKind::note) {
                std::optional<ScriptLookahead> lookahead;
                if (step.instruction.code < 0xb6)
                    lookahead = inspect_next_note(scripts_, context, flow,
                                                  {controls.slur, controls.drum_roll});
                auto result = apply_driver_note(step.instruction.code, lookahead,
                                                {id, s.playback.ownership.effects()}, pitches_,
                                                note, controls, s.global, s.queues);
                if (result.sample) {
                    const auto& sample = result.sample;
                    const auto identity =
                        resolve_sample_slot(samples_, s.loaded_song, sample->slot);
                    // A retained slot can be unassigned in the new song. The
                    // original upload clears that entry (C5/0424); note gates
                    // still run, allowing independently enabled noise to sound.
                    visit.publications.push_back({output.size(), work + *result.sample_offset});
                    output.push_back({id.voice, DriverSampleSelection{identity, sample->envelope}});
                }
                work += result.work + (lookahead ? lookahead->work : 0U);
                yielded = true;
            } else if (step.kind == ScriptStepKind::control) {
                const auto command = static_cast<MusicCommand>(step.instruction.code);
                DriverControlContext control_context{id, s.playback.ownership.effects(), {}};
                if (command == MusicCommand::sample || command == MusicCommand::default_envelope)
                    control_context.sample_defaults =
                        defaults(command == MusicCommand::sample ? step.instruction.operands[0]
                                                                 : controls.sample_slot,
                                 s);
                const auto result = apply_driver_control(step.instruction, control_context,
                                                         filters_, controls, s.global);
                if (result.envelope) {
                    visit.publications.push_back(
                        {output.size(), work + step.operand_work + *result.envelope_offset});
                    output.push_back({id.voice, *result.envelope});
                }
                // Deferred operand reads and the next-token branch occur once.
                work += step.operand_work + result.work + 8U;
            }
        }
        require(yielded, "Driver script did not yield within its command budget");
        visit.script_work = work - visit.wait_work;
        work += 8; // Script return to envelope selection.
        cursor = {flow.position, flow.loops, flow.loop_slot, flow.wait};
        // A null redirect terminates after replacing the stored cursor. The
        // standalone grammar keeps an inert continuation; this owner can model
        // absence directly, without ever treating zero as an executable address.
        if (null_redirect)
            cursor.position.reset();
    }
    // Termination does not cancel the current channel's final timed advancement.
    visit.envelope_offset = work + 16; // Envelope CALL.
    visit.work = visit.envelope_offset +
                 advance_driver_channel(id, controls, note.pitch, s.global).duration() + 10;
    return visit;
}
DriverServiceOutput DriverSequencer::begin_service() {
    require(stage_ == Stage::idle, "Driver service already in progress");
    auto next = state_;
    DriverServiceOutput result;
    auto& g = next.global;
    DriverBufferedOutput buffered{{},
                                  next.playback.releases,
                                  next.queues.starts,
                                  g.selected_noise_rate,
                                  g.noise.selected,
                                  g.modulation.selected};
    if (echo_enabled_) {
        DriverEchoFlush echo;
        for (std::size_t i = 0; i < 8; ++i)
            echo.filter[i] = upper(g.filter[i].value);
        echo.feedback = upper(g.echo_feedback.value);
        echo.volume = upper(g.echo_volume.value);
        echo.voices = g.echo.selected;
        buffered.echo = echo;
    }
    result.buffered = buffered;
    next.playback.releases = next.queues.starts = 0;
    auto stage = Stage::idle;
    const auto selection =
        begin_sequence_service(next.control_countdown, next.playback.ownership.paused(),
                               echo_enabled_, next.playback.controls.fade_status);
    const auto tick = selection.tick;
    result.begin_work = DriverServiceBeginWork{
        selection.buffered_loop_end, selection.status_end, {}, 0, {}, selection.work};
    auto& begin = *result.begin_work;
    if (tick.advance_sequences) {
        if (tick.advance_control_envelopes) {
            begin.control_offset = begin.work;
            begin.control_work =
                advance_playback_envelopes(next.playback, g, next.queues, next.music.controls)
                    .duration() +
                10;
            begin.work += begin.control_work + 16; // Return, then enter the script pass.
        }
        begin.script_offset = begin.work;
        const auto phase =
            advance_music_phase(next.music_phase, upper(g.tempo.value),
                                upper(next.playback.controls.tempo.value), next.forced_progression);
        result.music_poll = phase.due;
        result.script_pass = DriverScriptPass{SequenceKind::music, phase.work, {}, phase.work};
        auto& pass = *result.script_pass;
        if (result.music_poll) {
            const auto plan =
                script_traversal_plan(SequenceKind::music, next.playback.ownership.music);
            for (std::uint8_t i = 0; i < plan.order.count; ++i) {
                pass.work += plan.before[i];
                auto visit = advance_channel(plan.order.channels[i], next, result.voices);
                visit.pass_offset = pass.work;
                pass.work += visit.work;
                result.channels.push_back(std::move(visit));
            }
            pass.work += plan.after;
            pass.song_offset = pass.work;
            pass.work += advance_driver_song(g).duration() + 10;
        }
        stage = result.music_poll ? Stage::music_poll : Stage::effects;
        begin.work += pass.work;
    } else
        clear_dirty(next);
    state_ = next;
    stage_ = stage;
    return result;
}
DriverServiceOutput DriverSequencer::finish_service() {
    require(stage_ != Stage::idle, "No driver service in progress");
    auto next = state_;
    DriverServiceOutput result;
    const auto phase = advance_effects_phase(next.effects_phase);
    result.script_pass = DriverScriptPass{SequenceKind::effect, phase.work, {}, phase.work};
    auto& pass = *result.script_pass;
    if (phase.due) {
        const auto plan =
            script_traversal_plan(SequenceKind::effect, next.playback.ownership.effects());
        for (std::uint8_t i = 0; i < plan.order.count; ++i) {
            pass.work += plan.before[i];
            auto visit = advance_channel(plan.order.channels[i], next, result.voices);
            visit.pass_offset = pass.work;
            pass.work += visit.work;
            result.channels.push_back(std::move(visit));
        }
        pass.work += plan.after;
    } else {
        pass.work += 10; // Skipped effects still return from the script pass.
    }
    result.voice_pass = advance_driver_voices(next);
    for (const auto& update : result.voice_pass->updates) {
        const auto id = update.channel;
        const auto& voice = update.requests;
        if (voice.volume)
            result.voices.push_back({id.voice, *voice.volume});
        if (voice.pitch)
            result.voices.push_back({id.voice, DriverPitchRequest{*voice.pitch}});
    }
    state_ = next;
    stage_ = Stage::idle;
    return result;
}
} // namespace ff6
