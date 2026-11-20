#include "ff6/driver_publication.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace ff6 {
namespace {
void require_service(bool value, const char* reason) {
    if (!value)
        throw std::invalid_argument(reason);
}
SoundTime add_work(SoundTime at, std::uint32_t work) {
    require_service(at <= std::numeric_limits<SoundTime>::max() - work,
                    "Service publication time overflows");
    return at + work;
}
void check_envelope(const DriverEnvelope& envelope) {
    if (envelope.attack > 15 || envelope.decay > 7 || envelope.sustain_level > 7 ||
        envelope.sustain_rate > 31)
        throw std::invalid_argument("Invalid publication envelope");
}
void check_sample(const DriverSampleSelection& sample) {
    check_envelope(sample.envelope);
    if (sample.identity &&
        ((sample.identity->kind != SampleKind::instrument &&
          sample.identity->kind != SampleKind::effect) ||
         sample.identity->index >= (sample.identity->kind == SampleKind::instrument ? 63 : 8)))
        throw std::invalid_argument("Invalid published sample identity");
}
unsigned envelope_delay(SequenceChannel channel) {
    if ((channel.kind != SequenceKind::music && channel.kind != SequenceKind::effect) ||
        channel.voice >= 8 || (channel.kind == SequenceKind::effect && channel.voice < 4))
        throw std::invalid_argument("Invalid publication channel");
    // Music verifies physical ownership before publishing; effects own their
    // selected voice. Shadowed music emits no request at the logical stage.
    return channel.kind == SequenceKind::music ? 102U : 80U;
}
DriverEnvelope first_envelope(const DriverEnvelope& old, const DriverEnvelope& requested) {
    auto result = old;
    result.attack = requested.attack;
    result.decay = requested.decay;
    result.enabled = requested.enabled;
    return result;
}
} // namespace
void DriverPublisher::validate_time(SoundTime begun, unsigned duration) const {
    if (pending_load_ || begun < ready_at_ ||
        begun > std::numeric_limits<SoundTime>::max() - duration)
        throw std::invalid_argument("Invalid or overlapping publication time");
}
void DriverPublisher::validate(std::uint8_t voice, SoundTime begun, unsigned duration) const {
    if (voice >= voices_.size())
        throw std::invalid_argument("Invalid publication voice");
    validate_time(begun, duration);
}
DriverServicePublication DriverPublisher::service(SoundTime begun,
                                                  const DriverServiceOutput& requested) {
    validate_time(begun, 0);
    const bool beginning = requested.begin_work.has_value();
    require_service(requested.buffered.has_value() == beginning &&
                        requested.voice_pass.has_value() != beginning &&
                        (beginning || !requested.music_poll),
                    "Inconsistent service publication stage");
    auto next = *this;
    DriverServicePublication result;
    auto script_start = begun;
    if (beginning) {
        const auto& work = *requested.begin_work;
        result.buffered = next.buffered(begun, *requested.buffered);
        require_service(work.buffered_loop_end == result.buffered->completed_at - begun + 8 &&
                            work.status_end >= work.buffered_loop_end &&
                            work.status_end <= work.work &&
                            work.control_offset.has_value() == (work.control_work != 0) &&
                            work.script_offset.has_value() == requested.script_pass.has_value(),
                        "Inconsistent service prelude work");
        result.completed_at = add_work(begun, work.work);
        if (work.script_offset) {
            require_service(*work.script_offset >= work.status_end,
                            "Script precedes service status work");
            script_start = add_work(begun, *work.script_offset);
        }
        if (work.control_offset)
            require_service(work.script_offset && *work.control_offset >= work.status_end &&
                                std::uint64_t{*work.control_offset} + work.control_work <=
                                    *work.script_offset,
                            "Slow controls extend outside the prelude");
    } else
        require_service(requested.script_pass.has_value(), "Missing effect script pass");
    std::size_t consumed = 0;
    if (requested.script_pass) {
        const auto& pass = *requested.script_pass;
        const auto kind = beginning ? SequenceKind::music : SequenceKind::effect;
        require_service(pass.kind == kind && pass.phase_work > 0 && pass.phase_work <= pass.work &&
                            pass.song_offset.has_value() == (beginning && requested.music_poll),
                        "Inconsistent service script pass");
        const auto script_end = add_work(script_start, pass.work);
        require_service(!beginning || result.completed_at == script_end,
                        "Music stage does not end with its script pass");
        auto previous_end = add_work(script_start, pass.phase_work);
        // Both script banks scan upward. Only the later final-voice pass scans
        // effects downward; these are separate traversal rules (SCRIPT_PASS_WORK).
        std::optional<std::uint8_t> previous_voice;
        for (const auto& visit : requested.channels) {
            const auto start = add_work(script_start, visit.pass_offset);
            const auto end = add_work(start, visit.work);
            require_service(visit.channel.kind == kind && visit.work > 0 && start >= previous_end &&
                                end <= script_end &&
                                (!previous_voice || visit.channel.voice > *previous_voice),
                            "Invalid script visit order or extent");
            (void)envelope_delay(visit.channel);
            for (const auto& handoff : visit.publications) {
                require_service(handoff.intent_index == consumed &&
                                    consumed < requested.voices.size() &&
                                    handoff.offset < visit.work,
                                "Missing or misplaced script publication");
                const auto& intent = requested.voices[consumed++];
                require_service(intent.voice == visit.channel.voice, "Foreign script voice");
                DriverScriptVoiceOutput output{
                    visit.channel, handoff.intent_index, add_work(start, handoff.offset), {}};
                if (const auto* sample = std::get_if<DriverSampleSelection>(&intent.value))
                    output.publication = next.sample(output.begun, visit.channel, *sample);
                else if (const auto* envelope = std::get_if<DriverEnvelope>(&intent.value))
                    output.publication = next.envelope(output.begun, visit.channel, *envelope);
                else
                    throw std::invalid_argument("Invalid script publication kind");
                require_service(output.publication.completed_at <= end,
                                "Publication extends beyond its script visit");
                result.scripts.push_back(std::move(output));
            }
            next.validate_time(end, 0);
            next.ready_at_ = previous_end = end;
            previous_voice = visit.channel.voice;
        }
        if (pass.song_offset)
            require_service(add_work(script_start, *pass.song_offset) >= previous_end &&
                                *pass.song_offset <= pass.work,
                            "Song advancement lies outside its script pass");
        next.validate_time(script_end, 0);
        next.ready_at_ = script_end;
        if (!beginning) {
            const auto& voices = *requested.voice_pass;
            for (const auto& update : voices.updates) {
                const auto matches = [&](DriverVoiceValue value) {
                    require_service(consumed < requested.voices.size() &&
                                        requested.voices[consumed++] ==
                                            DriverVoiceIntent{update.channel.voice, value},
                                    "Prepared voices disagree with service intentions");
                };
                if (update.requests.volume)
                    matches(*update.requests.volume);
                if (update.requests.pitch)
                    matches(DriverPitchRequest{*update.requests.pitch});
            }
            result.voices =
                DriverServiceVoicePass{script_end, next.publish_voices(script_end, voices)};
            result.completed_at = result.voices->publication.output.completed_at;
        }
    } else
        require_service(requested.channels.empty() && !requested.music_poll,
                        "Paused service contains script work");
    require_service(consumed == requested.voices.size(), "Unaccounted service intention");
    next.validate_time(result.completed_at, 0);
    next.ready_at_ = result.completed_at;
    *this = next;
    return result;
}
DriverLoadStart DriverPublisher::begin_load(SoundTime begun, const DriverSongOutput& requested) {
    const auto& work = requested.work;
    const auto release = work.release_offset;
    const auto fresh = [](std::uint32_t value, std::uint32_t base) {
        return value >= base && value <= base + 8U * 232U && (value - base) % 232U == 0;
    };
    if (!requested.release_all || !requested.abandon_service ||
        (release != 34 && release != 42 && release != 78 && release != 86 && release != 49552 &&
         release != 49560) ||
        work.content_wait_offset != release + 82 ||
        (requested.resume ? work.preparation != 50060
                          : !fresh(work.preparation, 1338) && !fresh(work.preparation, 51260)))
        throw std::invalid_argument("Inconsistent loading preparation request");
    if (requested.resume)
        for (const auto& voice : *requested.resume) {
            check_sample(voice.sample);
            if (voice.pitch_work != 206 && voice.pitch_work != 244)
                throw std::invalid_argument("Invalid resumed pitch preparation");
        }
    validate_time(begun, work.content_wait_offset);
    auto next = *this;
    DriverLoadStart result{next.release_all(begun + release), begun + work.content_wait_offset};
    next.ready_at_ = result.awaiting_content_at;
    next.pending_load_ = requested;
    *this = next;
    return result;
}
DriverLoadCompletion DriverPublisher::finish_load(SoundTime content_ready, DriverClock& clock) {
    if (!pending_load_)
        throw std::invalid_argument("No loading operation awaiting content");
    const auto requested = *pending_load_;
    auto next = *this;
    auto next_clock = clock;
    next.pending_load_.reset();
    next.validate_time(content_ready, requested.work.preparation);
    auto now = content_ready + requested.work.preparation;
    DriverLoadCompletion result;
    if (requested.resume) {
        result.output.changes.reserve(56);
        const auto append = [&](const VoicePublication& output, unsigned following) {
            result.output.changes.insert(result.output.changes.end(), output.changes.begin(),
                                         output.changes.end());
            next.validate_time(output.completed_at, following);
            now = output.completed_at + following;
        };
        for (std::uint8_t voice = 0; voice < 8; ++voice) {
            const auto& selection = (*requested.resume)[voice];
            append(next.sample(now, {SequenceKind::music, voice}, selection.sample), 52);
            append(next.restore_silence(now, voice), 38U + selection.pitch_work);
            // Pitch publication includes its conditional comparisons. Following
            // work finishes the caller and selects the next voice, or cleans up
            // resumed routing/queues after the final voice. LOAD_PUBLICATION.md.
            append(next.pitch(now, voice, selection.pitch), voice == 7 ? 92 : 76);
        }
    }
    next.validate_time(now, 68);
    result.discard_at = now + 62;
    result.output.completed_at = now + 68;
    result.discarded_opportunities = next_clock.discard_after_song_load(result.discard_at);
    next.ready_at_ = result.output.completed_at;
    *this = next;
    clock = next_clock;
    return result;
}
GlobalPublication DriverPublisher::buffered(SoundTime begun,
                                            const DriverBufferedOutput& requested) {
    if (requested.noise_rate >= 32)
        throw std::invalid_argument("Invalid published noise rate");
    validate_time(begun, requested.echo ? 978U : 290U);
    GlobalPublication result{begun + (requested.echo ? 978U : 290U), {}};
    result.changes.reserve(requested.echo ? 17 : 5);
    auto filter = filter_;
    auto gain = echo_gain_;
    unsigned offset = requested.echo ? 50U : 58U;
    const auto emit = [&](PublishedGlobalValue value) {
        result.changes.push_back({begun + offset, std::move(value)});
        offset += 58;
    };
    if (requested.echo) {
        // FFVI changes the filter in descending tap order, then the right and
        // left gains. Intermediate complete values retain preceding history.
        for (std::size_t remaining = filter.taps.size(); remaining > 0; --remaining) {
            const auto index = remaining - 1;
            filter.taps[index] = static_cast<std::int8_t>(signed8(requested.echo->filter[index]));
            emit(filter);
        }
        emit(PublishedEchoFeedback{static_cast<std::int8_t>(signed8(requested.echo->feedback))});
        gain.stereo[1] = static_cast<std::int8_t>(signed8(requested.echo->volume));
        emit(gain);
        gain.stereo[0] = gain.stereo[1];
        emit(gain);
        emit(PublishedRouting{VoiceRouting::echo, requested.echo->voices});
    }
    // Even unchanged or zero masks are published. Simultaneous release/start
    // intentions keep their original ordering; synthesis must not collapse them.
    emit(PublishedNoteGate{NoteGate::releases, requested.releases});
    emit(PublishedNoiseRate{requested.noise_rate});
    emit(PublishedRouting{VoiceRouting::noise, requested.noise});
    emit(PublishedRouting{VoiceRouting::modulation, requested.modulation});
    emit(PublishedNoteGate{NoteGate::starts, requested.starts});
    filter_ = filter;
    echo_gain_ = gain;
    ready_at_ = result.completed_at;
    return result;
}
GlobalPublication DriverPublisher::release_all(SoundTime begun) {
    validate_time(begun, 16);
    GlobalPublication result{begun + 16,
                             {{begun + 16, PublishedNoteGate{NoteGate::releases, 255}}}};
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::pause(SoundTime begun, const PlaybackRequests& requested) {
    if (!requested.envelope_hold)
        throw std::invalid_argument("Missing pause envelope request");
    const bool hold = *requested.envelope_hold;
    if (requested.silence_all != hold || requested.abandon_service != hold ||
        requested.cancel_music_traversal ||
        (hold ? requested.work != 1338 && requested.work != 1372
              : requested.work != 442 && requested.work != 482))
        throw std::invalid_argument("Inconsistent pause handler request");
    validate_time(begun, requested.work);
    VoicePublication result{begun + requested.work, {}};
    result.changes.reserve(hold ? 24 : 8);
    auto next = voices_;
    for (std::uint8_t voice = 0; voice < next.size(); ++voice) {
        next[voice].envelope.enabled = !hold;
        result.changes.push_back(
            {begun + (hold ? 40U : 44U) + 50U * voice, voice, next[voice].envelope});
    }
    if (hold) {
        for (std::uint8_t voice = 0; voice < next.size(); ++voice) {
            auto& gain = next[voice].gain;
            gain[0] = 0;
            result.changes.push_back({begun + 434U + 60U * voice, voice, gain});
            gain[1] = 0;
            result.changes.push_back({begun + 454U + 60U * voice, voice, gain});
        }
    }
    // The original clears its gain cache after silencing all voices. Reserving
    // the entire handler prevents another output from observing an early clear.
    // Stage changes locally so validation/allocation failures retain history.
    voices_ = next;
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::update_voice(SoundTime begun, const DriverOutputPolicy& policy,
                                               DriverChannelControls& channel,
                                               const DriverPitchMotion& pitch,
                                               DriverGlobalControls& global) {
    auto next = *this;
    auto next_channel = channel;
    auto next_global = global;
    const auto requests = update_driver_voice(policy, next_channel, pitch, next_global);
    auto result = next.publish_voice(begun, policy.channel, requests);
    *this = next;
    channel = next_channel;
    global = next_global;
    return result;
}
VoicePublication DriverPublisher::publish_voice(SoundTime begun, SequenceChannel channel,
                                                const DriverVoiceRequests& requests) {
    (void)envelope_delay(channel); // Validate semantic channel even for a quiet call.
    if ((requests.volume.has_value() != (requests.gain_work != 0)) ||
        (requests.pitch.has_value() != (requests.pitch_work != 0)) ||
        (requests.gain_work & 1U) != 0 ||
        (requests.pitch &&
         requests.pitch_work != (channel.kind == SequenceKind::effect ? 104 : 206) &&
         !(channel.kind == SequenceKind::music && requests.pitch_work == 244)) ||
        requests.work.between != (requests.volume && requests.pitch ? 62 : 0) ||
        requests.work.after_last != (requests.pitch    ? 18
                                     : requests.volume ? 30
                                                       : 0))
        throw std::invalid_argument("Inconsistent prepared voice update");
    auto next = *this;
    next.validate(channel.voice, begun, requests.work.before_first);
    auto now = begun + requests.work.before_first;
    VoicePublication result;
    result.changes.reserve(4);
    const auto append = [&](const VoicePublication& publication, unsigned following_work) {
        result.changes.insert(result.changes.end(), publication.changes.begin(),
                              publication.changes.end());
        next.validate_time(publication.completed_at, following_work);
        now = publication.completed_at + following_work;
    };
    if (requests.volume) {
        next.validate_time(now, requests.gain_work);
        append(next.gain(now + requests.gain_work, channel.voice, *requests.volume),
               requests.pitch ? requests.work.between : requests.work.after_last);
    }
    if (requests.pitch) {
        next.validate_time(now, requests.pitch_work);
        append(next.pitch(now + requests.pitch_work, channel.voice, *requests.pitch),
               requests.work.after_last);
    }
    result.completed_at = now;
    next.ready_at_ = now;
    *this = next;
    return result;
}
VoiceTraversalPublication DriverPublisher::update_voices(SoundTime begun, DriverState& state) {
    auto next_state = state;
    const auto pass = advance_driver_voices(next_state);
    auto result = publish_voices(begun, pass);
    state = next_state;
    return result;
}
VoiceTraversalPublication DriverPublisher::publish_voices(SoundTime begun,
                                                          const DriverVoicePass& pass) {
    if (pass.updates.size() > 8)
        throw std::invalid_argument("Too many prepared voice updates");
    unsigned music = 0, effects = 0;
    for (const auto& update : pass.updates) {
        (void)envelope_delay(update.channel);
        const auto mask = 1U << update.channel.voice;
        if (((music | effects) & mask) != 0)
            throw std::invalid_argument("Repeated prepared voice");
        (update.channel.kind == SequenceKind::music ? music : effects) |= mask;
    }
    const auto plan =
        voice_update_plan(static_cast<std::uint8_t>(music), static_cast<std::uint8_t>(music),
                          static_cast<std::uint8_t>(effects));
    if (pass.after != plan.after)
        throw std::invalid_argument("Invalid prepared voice-pass tail");
    auto next = *this;
    VoiceTraversalPublication result;
    result.count = plan.order.count;
    result.output.changes.reserve(4U * plan.order.count);
    auto now = begun;
    for (std::uint8_t i = 0; i < plan.order.count; ++i) {
        const auto& update = pass.updates[i];
        if (update.channel != plan.order.channels[i] || update.before != plan.before[i])
            throw std::invalid_argument("Invalid prepared voice order or gap");
        next.validate_time(now, update.before);
        now += update.before;
        const auto publication = next.publish_voice(now, update.channel, update.requests);
        result.updates[i] = {update.channel, now, publication.completed_at};
        result.output.changes.insert(result.output.changes.end(), publication.changes.begin(),
                                     publication.changes.end());
        now = publication.completed_at;
    }
    next.validate_time(now, pass.after);
    result.output.completed_at = now + pass.after;
    next.ready_at_ = result.output.completed_at;
    *this = next;
    return result;
}
VoicePublication DriverPublisher::gain(SoundTime begun, std::uint8_t voice, StereoGain requested) {
    validate(voice, begun, 0);
    auto next = voices_[voice].gain;
    const bool left = next[0] != requested[0], right = next[1] != requested[1];
    const unsigned left_cost = left ? 4U : 0U;
    const auto duration = 298 + left_cost + (right ? 4U : 0U);
    validate(voice, begun, duration);
    VoicePublication result{begun + duration, {}};
    if (left) {
        next[0] = requested[0];
        result.changes.push_back({begun + 120, voice, next});
    }
    if (right) {
        next[1] = requested[1];
        result.changes.push_back({begun + 264 + left_cost, voice, next});
    }
    voices_[voice].gain = next;
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::gain_from_controls(SoundTime begun,
                                                     const DriverOutputPolicy& policy,
                                                     const DriverChannelControls& channel,
                                                     const DriverGlobalControls& global) {
    const auto prepared = prepare_driver_gain(policy, channel, global);
    validate(policy.channel.voice, begun, prepared.publication_delay);
    return gain(begun + prepared.publication_delay, policy.channel.voice, prepared.stereo());
}
VoicePublication DriverPublisher::pitch_from_controls(SoundTime begun,
                                                      const DriverOutputPolicy& policy,
                                                      const DriverChannelControls& channel,
                                                      const DriverPitchMotion& motion) {
    const auto prepared = prepare_driver_pitch(policy, channel, motion);
    validate(policy.channel.voice, begun, prepared.publication_delay);
    return pitch(begun + prepared.publication_delay, policy.channel.voice, prepared.coefficient);
}
VoicePublication DriverPublisher::pitch(SoundTime begun, std::uint8_t voice,
                                        std::uint16_t requested) {
    validate(voice, begun, 0);
    auto next = voices_[voice].pitch;
    const bool fine = (next & 255U) != (requested & 255U),
               coarse = (next >> 8U) != (requested >> 8U);
    const unsigned fine_cost = fine ? 4U : 0U;
    const auto duration = 62 + fine_cost + (coarse ? 4U : 0U);
    validate(voice, begun, duration);
    VoicePublication result{begun + duration, {}};
    if (fine) {
        // The original publishes the low fractional portion before the rest of
        // the coefficient. Preserve the transient complete pitch, not a pair of
        // raw device writes. Sample-time narrowing belongs to synthesis routing.
        next = static_cast<std::uint16_t>((next & 0xff00U) | (requested & 255U));
        result.changes.push_back({begun + 40, voice, DriverPitchRequest{next}});
    }
    if (coarse) {
        next = requested;
        result.changes.push_back({begun + 66 + fine_cost, voice, DriverPitchRequest{next}});
    }
    voices_[voice].pitch = next;
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::envelope(SoundTime begun, SequenceChannel channel,
                                           DriverEnvelope requested) {
    check_envelope(requested);
    const auto first = envelope_delay(channel);
    validate(channel.voice, begun, first + 46);
    VoicePublication result{begun + first + 46, {}};
    result.changes.push_back(
        {begun + first, channel.voice, first_envelope(voices_[channel.voice].envelope, requested)});
    result.changes.push_back({result.completed_at, channel.voice, requested});
    voices_[channel.voice].envelope = requested;
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::sample(SoundTime begun, SequenceChannel channel,
                                         DriverSampleSelection requested) {
    check_sample(requested);
    const auto first = 60 + envelope_delay(channel);
    validate(channel.voice, begun, first + 46);
    VoicePublication result{begun + first + 46, {}};
    result.changes.push_back({begun + 52, channel.voice, PublishedSample{requested.identity}});
    result.changes.push_back({begun + first, channel.voice,
                              first_envelope(voices_[channel.voice].envelope, requested.envelope)});
    result.changes.push_back({result.completed_at, channel.voice, requested.envelope});
    voices_[channel.voice].envelope = requested.envelope;
    ready_at_ = result.completed_at;
    return result;
}
VoicePublication DriverPublisher::restore_silence(SoundTime begun, std::uint8_t voice) {
    validate(voice, begun, 62);
    auto next = voices_[voice].gain;
    VoicePublication result{begun + 62, {}};
    next[0] = 0;
    result.changes.push_back({begun + 16, voice, next});
    next[1] = 0;
    result.changes.push_back({result.completed_at, voice, next});
    voices_[voice].gain = next;
    ready_at_ = result.completed_at;
    return result;
}
} // namespace ff6
