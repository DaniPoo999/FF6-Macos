#include "ff6/playback_controls.hpp"
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
std::uint8_t high(std::uint16_t value) {
    return static_cast<std::uint8_t>(value >> 8U);
}
std::uint8_t channel_mask(SequenceChannel channel) {
    require((channel.kind == SequenceKind::music || channel.kind == SequenceKind::effect) &&
                channel.voice < 8 && (channel.kind == SequenceKind::music || channel.voice >= 4),
            "Invalid playback channel");
    return byte(1U << channel.voice);
}
void validate(const PlaybackState& state, const DriverGlobalControls& global) {
    const auto& o = state.ownership;
    require(((o.game_effects | o.system_effects | o.paused_effects) & 15U) == 0,
            "Invalid playback effect ownership");
    require((!o.song || *o.song < 85) && (!o.suspended_song || *o.suspended_song < 85),
            "Invalid playback song identity");
    require(global.music_noise_rate < 32 && global.effects_noise_rate < 32 &&
                global.selected_noise_rate < 32,
            "Invalid playback noise rate");
}
enum class RampSetup { immediate, equal, rising, falling };
RampSetup configure(DriverRamp& ramp, std::uint8_t duration, std::uint8_t value,
                    bool signed_values) {
    const auto ordered = [signed_values](std::uint8_t v) {
        return signed_values && v >= 128 ? int{v} - 256 : int{v};
    };
    const int difference = ordered(value) - ordered(high(ramp.value));
    if (duration == 0 || difference == 0) {
        ramp = {static_cast<std::uint16_t>(std::uint32_t{value} * 256U), 0, 0};
        return duration == 0 ? RampSetup::immediate : RampSetup::equal;
    }
    ramp.value = word(ramp.value & 0xff00U);
    ramp.remaining = duration;
    ramp.step = word(static_cast<std::uint32_t>(difference * 256 / duration));
    return difference > 0 ? RampSetup::rising : RampSetup::falling;
}
std::uint16_t volume_setup_work(RampSetup setup) {
    // Indexed volume setup excludes the shared selection/loop/return work.
    // A falling ramp includes two-stage magnitude conversion in the original.
    // COMMAND_WORK.md derives these paths and their caller selection costs.
    switch (setup) {
    case RampSetup::immediate:
        return 74;
    case RampSetup::equal:
        return 116;
    case RampSetup::rising:
        return 246;
    case RampSetup::falling:
        return 278;
    }
    throw std::logic_error("Invalid volume setup");
}
std::uint16_t single_setup_work(RampSetup setup, bool signed_values) {
    // Complete pan/tempo/pitch handler, including its final return.
    switch (setup) {
    case RampSetup::immediate:
        return 84;
    case RampSetup::equal:
        return signed_values ? 146 : 122;
    case RampSetup::rising:
        return signed_values ? 256 : 232;
    case RampSetup::falling:
        return signed_values ? 288 : 264;
    }
    throw std::logic_error("Invalid single ramp setup");
}
void refresh_routing(const PlaybackOwnership& ownership, DriverGlobalControls& global) {
    const auto refresh = [&](DriverRouting& routing, std::uint8_t effects) {
        routing.selected = byte((routing.music & ~std::uint32_t{ownership.effects()}) | effects);
    };
    refresh(global.echo, global.echo.effects);
    refresh(global.modulation, global.modulation.effects);
    refresh(global.noise, byte(global.noise.effects & 254U));
    global.selected_noise_rate =
        global.noise.effects ? global.effects_noise_rate : global.music_noise_rate;
}
std::uint16_t terminate(SequenceChannel channel, PlaybackState& state, DriverGlobalControls& global,
                        std::span<DriverChannelControls, 8> music_channels) {
    const auto mask = channel_mask(channel);
    auto& o = state.ownership;
    if (channel.kind == SequenceKind::music)
        o.music = byte(o.music & ~std::uint32_t{mask});
    else {
        o.game_effects = byte(o.game_effects & ~std::uint32_t{mask});
        o.system_effects = byte(o.system_effects & ~std::uint32_t{mask});
        // The original clears the music flag, even when ending an effect.
        music_channels[channel.voice].slurred_note = false;
    }
    for (auto* routing : {&global.echo, &global.noise, &global.modulation}) {
        auto& selected = channel.kind == SequenceKind::music ? routing->music : routing->effects;
        selected = byte(selected & ~std::uint32_t{mask});
    }
    refresh_routing(o, global);
    // Count the remaining raw effect-noise mask after clearing this channel;
    // bit zero affects rate selection even though output routing removes it.
    // Includes both nested routing calls and the final echo return.
    return static_cast<std::uint16_t>((channel.kind == SequenceKind::music ? 368U : 400U) +
                                      (global.noise.effects == 0 ? 4U : 0U));
}
std::uint16_t stop_effects(PlaybackState& state, DriverGlobalControls& global,
                           DriverNoteQueues& queues,
                           std::span<DriverChannelControls, 8> music_channels) {
    const auto mask = state.ownership.game_effects;
    state.releases = byte(state.releases | mask);
    queues.starts = byte(queues.starts & ~std::uint32_t{mask});
    global.volume_dirty = byte(global.volume_dirty & ~std::uint32_t{mask});
    global.pitch_dirty = byte(global.pitch_dirty & ~std::uint32_t{mask});
    // The stop scans all four slots, even with no ordinary effects. Each selected
    // slot adds termination work; its noise-rate selection uses the remaining
    // raw effect mask (including bit zero). See PLAYBACK_WORK.md.
    std::uint16_t work = 270;
    // Preserve the original descending termination order for routing refreshes.
    for (std::uint8_t voice = 8; voice > 4;) {
        --voice;
        if ((mask & (1U << voice)) != 0) {
            const auto termination =
                terminate({SequenceKind::effect, voice}, state, global, music_channels);
            work = static_cast<std::uint16_t>(work + termination + 12U);
        }
    }
    return work;
}
bool advance(DriverRamp& ramp) {
    if (ramp.remaining == 0)
        return false;
    --ramp.remaining;
    const auto before = high(ramp.value);
    ramp.value = word(std::uint32_t{ramp.value} + ramp.step);
    return high(ramp.value) != before;
}
std::uint16_t advance_dirty(DriverRamp& ramp, std::uint8_t owners, std::uint8_t& dirty) {
    if (ramp.remaining == 0)
        return 14;
    const bool changed = advance(ramp);
    if (changed)
        dirty = byte(dirty | owners);
    // An already-dirty voice still takes the upper-byte-change path.
    return changed ? 70 : 62;
}
} // namespace

std::uint16_t terminate_playback_channel(SequenceChannel channel, PlaybackState& state,
                                         DriverGlobalControls& global,
                                         std::span<DriverChannelControls, 8> music_channels,
                                         TerminationOrigin origin) {
    validate(state, global);
    (void)channel_mask(channel);
    require(origin == TerminationOrigin::playback || origin == TerminationOrigin::script,
            "Invalid termination origin");
    const auto work = terminate(channel, state, global, music_channels);
    return static_cast<std::uint16_t>(work + (origin == TerminationOrigin::script ? 16U : 0U));
}
PlaybackRequests apply_playback_command(PlaybackCommand command, std::uint8_t duration,
                                        std::uint8_t value, PlaybackState& state,
                                        DriverGlobalControls& global, DriverNoteQueues& queues,
                                        std::span<DriverChannelControls, 8> music_channels) {
    validate(state, global);
    require(command <= PlaybackCommand::pause, "Unsupported playback command");
    const bool ramp = command <= PlaybackCommand::pitch;
    require(ramp || duration == 0, "Unexpected playback duration");
    require(command < PlaybackCommand::conditional || command == PlaybackCommand::pause ||
                value == 0,
            "Unexpected playback argument");
    auto& c = state.controls;
    auto& o = state.ownership;
    PlaybackRequests request;
    switch (command) {
    case PlaybackCommand::volume:
    case PlaybackCommand::music_volume:
    case PlaybackCommand::effect_volume:
        c.fade_status = command != PlaybackCommand::effect_volume && value == 0;
        request.work = command == PlaybackCommand::volume         ? 188
                       : command == PlaybackCommand::music_volume ? 126
                                                                  : 134;
        if (command != PlaybackCommand::effect_volume)
            request.work = static_cast<std::uint16_t>(
                request.work +
                volume_setup_work(configure(c.music_volume, duration, value, false)));
        if (command != PlaybackCommand::music_volume)
            request.work = static_cast<std::uint16_t>(
                request.work +
                volume_setup_work(configure(c.effect_volume, duration, value, false)));
        global.volume_dirty = 255;
        break;
    case PlaybackCommand::effect_pan:
        request.work = single_setup_work(configure(c.effect_pan, duration, value, false), false);
        global.volume_dirty = 255;
        break;
    case PlaybackCommand::tempo:
        request.work = single_setup_work(configure(c.tempo, duration, value, true), true);
        global.pitch_dirty = 255;
        break;
    case PlaybackCommand::pitch:
        request.work = single_setup_work(configure(c.pitch, duration, value, true), true);
        global.pitch_dirty = 255;
        break;
    case PlaybackCommand::mono:
        request.work = value ? 42 : 46;
        c.mono = value != 0;
        global.volume_dirty = 255;
        break;
    case PlaybackCommand::mute:
        request.work = 30;
        c.muted = value;
        global.volume_dirty = 255;
        break;
    case PlaybackCommand::conditional:
        request.work = 30;
        c.conditional = o.music;
        break;
    case PlaybackCommand::stop:
    case PlaybackCommand::stop_music:
    case PlaybackCommand::stop_effects:
        // stop_effects already includes the 14-tick effects-only selection.
        // Combined stop substitutes its 176-tick music cleanup prefix and adds
        // the final 10-tick return: 176 - 14 + 10 = 172.
        request.work = command == PlaybackCommand::stop_music     ? 190
                       : command == PlaybackCommand::stop_effects ? 10
                                                                  : 172;
        if (command != PlaybackCommand::stop_effects) {
            const auto mask = byte(~std::uint32_t{o.effects()});
            state.releases = byte(state.releases | mask);
            queues.starts = byte(queues.starts & ~std::uint32_t{mask});
            for (auto* routing : {&global.echo, &global.noise, &global.modulation}) {
                routing->selected = byte(routing->selected & ~std::uint32_t{mask});
                routing->music = 0;
            }
            o.music = o.paused_music = 0;
            o.song.reset();
            o.suspended_song.reset();
            c.fade_status = false;
            request.cancel_music_traversal = true;
        }
        if (command != PlaybackCommand::stop_music)
            request.work = static_cast<std::uint16_t>(
                request.work + stop_effects(state, global, queues, music_channels));
        break;
    case PlaybackCommand::pause:
        // The original pause resets gain history and abandons its caller. Ownership
        // snapshots are replaced only when music/ordinary effects are active.
        // Unpause always restores ADSR, even with an empty saved ownership set.
        request.work =
            value ? ((o.music || o.game_effects) ? 1372 : 1338) : (o.paused() ? 482 : 442);
        request.envelope_hold = value != 0;
        if (value != 0) {
            request.silence_all = request.abandon_service = true;
            if (o.music != 0 || o.game_effects != 0) {
                o.paused_music = o.music;
                o.paused_effects = o.game_effects;
                o.music = o.game_effects = global.volume_dirty = 0;
            }
            queues.starts = 0;
        } else if (o.paused()) {
            o.music = o.paused_music;
            o.game_effects = o.paused_effects;
            o.paused_music = o.paused_effects = 0;
            global.volume_dirty = 255;
        }
        break;
    }
    return request;
}
PlaybackEnvelopeWork
advance_playback_envelopes(PlaybackState& state, DriverGlobalControls& global,
                           DriverNoteQueues& queues,
                           std::span<DriverChannelControls, 8> music_channels) {
    validate(state, global);
    PlaybackEnvelopeWork work;
    auto& c = state.controls;
    work.music_volume = advance_dirty(c.music_volume, state.ownership.music, global.volume_dirty);
    const bool effect_active = c.effect_volume.remaining != 0;
    const bool effect_changed = advance(c.effect_volume);
    if (effect_changed)
        global.volume_dirty = byte(global.volume_dirty | state.ownership.game_effects);
    work.effect_volume = effect_active ? (effect_changed ? 78 : 70) : 14;
    if (effect_active && c.effect_volume.remaining == 0)
        work.effect_volume = effect_changed ? 86 : 78;
    if (effect_active && c.effect_volume.remaining == 0 && high(c.effect_volume.value) == 0) {
        // Retain the fractional remainder while restoring the effect master level.
        c.effect_volume.value = word(c.effect_volume.value | 0xff00U);
        work.effect_volume = static_cast<std::uint16_t>(
            (effect_changed ? 128U : 120U) + stop_effects(state, global, queues, music_channels));
    }
    work.effect_pan =
        advance_dirty(c.effect_pan, state.ownership.game_effects, global.volume_dirty);
    work.tempo = c.tempo.remaining ? 48 : 14;
    (void)advance(c.tempo);
    work.pitch = advance_dirty(c.pitch, state.ownership.music, global.pitch_dirty);
    return work;
}
DriverOutputPolicy playback_output_policy(SequenceChannel channel, const PlaybackState& state) {
    const auto mask = channel_mask(channel);
    return {channel,
            state.controls.mono,
            (state.controls.muted & mask) != 0,
            (state.ownership.system_effects & mask) != 0,
            high(state.controls.music_volume.value),
            high(state.controls.effect_volume.value),
            high(state.controls.effect_pan.value),
            high(state.controls.pitch.value)};
}
} // namespace ff6
