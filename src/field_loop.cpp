#include "rom-materials/field_event_mosaic_start.macros.inc"
#include "rom-materials/field_startup_fade.macros.inc"
#include "rom-materials/field_event_fade_brightness.macros.inc"
#include "rom-materials/field_window_anchor_x.macros.inc"
#include "rom-materials/field_flashlight_anchor_y.macros.inc"
#include "ff6/field_loop.hpp"
#include "ff6/field_party_switch.hpp"
#include <algorithm>

namespace ff6 {
// The original event fade endpoints are opposite unsigned bytes. Their shared
// native magnitude is restored from both authenticated choices, not retained
// as an executable fallback in the Builder template.
static_assert(FF6_ROM_VALUE_FIELD_EVENT_FADE_BRIGHTNESS_0 +
              FF6_ROM_VALUE_FIELD_EVENT_FADE_BRIGHTNESS_1 == 256U);
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
ByteView records(ByteView pointers, ByteView data, unsigned map, unsigned count, unsigned base,
                 unsigned stride) {
    require(map < count && pointers.size() == (count + 1U) * 2U,
            "Invalid field trigger pointer table");
    const unsigned first = read16(pointers, map * 2U), end = read16(pointers, map * 2U + 2U);
    require(first >= base && end >= first && end - base <= data.size() &&
                (first - base) % stride == 0 && (end - first) % stride == 0,
            "Invalid field trigger record range");
    return data.subspan(first - base, end - first);
}
const FieldObject& selected(const FieldObjects& field) {
    require(field.update_order[0] && *field.update_order[0] < field.objects.size(),
            "Field loop needs a selected actor");
    return field.objects[*field.update_order[0]];
}
bool aligned(const FieldObject& actor, bool fractions) {
    return fractions ? actor.x % 4096 == 0 && actor.y % 4096 == 0
                     : ((actor.x >> 8U) & 15U) == 0 && ((actor.y >> 8U) & 15U) == 0;
}
FieldOverlayView overlay_view(const FieldScroll& scroll) {
    const auto origins = field_scroll_origins(scroll);
    return {{wrap16(static_cast<std::uint32_t>(origins[0].x)),
             wrap16(static_cast<std::uint32_t>(origins[0].y))},
            {scroll.layers[0][0].follow_rate, scroll.layers[0][1].follow_rate},
            0};
}
} // namespace

FieldLoopAssets load_field_loop_assets(const ContentPack& pack, std::uint16_t map,
                                       const FieldMap* previous_display) {
    FieldLoopAssets result;
    result.map = previous_display ? load_field_map_destination(pack, map, *previous_display)
                                  : load_field_map(pack, map);
    result.animation = load_field_animation(pack, result.map.definition);
    result.objects = load_field_object_view(pack);
    result.actions = load_field_action_data(pack);
    result.player = load_field_player_data(pack, map);
    result.party = load_party_data(pack);
    result.equipment = load_equipment_data(pack);
    result.menu_items = load_menu_items(pack);
    result.dialogue = load_field_dialogue(pack);
    result.battles = load_field_battle_data(pack);
    result.timer_graphics = load_event_timer_graphics(pack);
    result.window_sine = load_field_window_sine(pack);
    const auto triggers = pack.get("data_c40000_event_triggers");
    constexpr unsigned base = 417U * 2U;
    require(triggers.size() >= base, "Truncated field trigger resource");
    const auto entries = records(slice(triggers, 0, base),
                                 slice(triggers, base, triggers.size() - base), map, 416, base, 5);
    for (std::size_t at = 0; at < entries.size(); at += 5) {
        const auto event = read24(entries, at + 2);
        require(event < result.actions.script.size(), "Field trigger event outside script");
        result.triggers.push_back({{entries[at], entries[at + 1]}, event});
    }
    result.entrances = load_field_entrances(pack, map);
    const auto properties =
        slice(pack.get("field_structure_map_prop_bin"), std::size_t{map} * 33, 33);
    result.random_encounters = (properties[5] & 128U) != 0;
    result.map_title = properties[0];
    result.menu_permissions = properties[1] & 3U;
    result.battle_background = properties[2];
    require(result.map_title < result.dialogue.titles.size(), "Map title outside table");
    return result;
}
std::optional<SongLoad> field_map_song(std::uint8_t map_song, bool override_song,
                                       std::uint8_t& retained_song) {
    if (!override_song) {
        if (map_song == 0)
            return {};
        retained_song = map_song;
    }
    return SongLoad{retained_song, 255, false, false};
}

FieldLoop::FieldLoop(FieldLoopAssets assets, FieldLoopSeed seed)
    : assets_(std::move(assets)), owned_(std::move(seed)),
      updates_(assets_.map, field_map_centers(owned_.actions.scroll)),
      display_(assets_.map, assets_.animation, owned_.third_phase, owned_.palette) {
    require(!owned_.transition.pending, "Field entry still has a pending map load");
    require(owned_.actions.occupancy.width == assets_.map.definition.layouts[0].width &&
                owned_.actions.occupancy.height == assets_.map.definition.layouts[0].height,
            "Field entry occupancy does not match its map");
    (void)owned_.actions.occupancy.at(0, 0);
    require(owned_.state.field.update_count <= owned_.state.field.update_order.size(),
            "Invalid initial field update count");
    (void)selected(owned_.state.field);
    if (owned_.world) {
        // A world save retains the previous field for the next field arrival,
        // but does not re-enter it. Its last scripted coordinates can lie beyond
        // walkable terrain (for example a scrolling exit). WorldSession owns
        // rendering, music and input until an actual field destination loads.
        require(!owned_.startup, "World restoration cannot run a field startup");
        owned_.map_presentation_pending = false;
        owned_.fade_in = false;
        phase_ = Phase::frame_begin;
    } else if (owned_.startup && begin_field_map_startup(owned_.state, owned_.events, *owned_.startup))
        phase_ = Phase::startup_begin;
    else
        finish_startup();
}
bool FieldLoop::event_running() const {
    return owned_.events.continuation() != EventContinuation::script ||
           owned_.events.position() != assets_.player.idle_event;
}
bool FieldLoop::run_events(std::size_t& budget) {
    while (budget != 0) {
        --budget;
        const auto result =
            advance_field_contact_event(owned_.state, owned_.actions, owned_.events,
                                        {.confirm_down = held(owned_.input.held, Button::a),
                                         .backgrounds_ready = updates_.ready(),
                                         .map_load_pending = owned_.transition.pending.has_value(),
                                         .dialogue_open = owned_.dialogue.open(),
                                         .dialogue_text_ready = owned_.dialogue.text_ready(),
                                         .fade_active = owned_.transition.fade.active(),
                                         .music_finished = music_finished_,
                                         .music_position = music_position_},
                                        1);
        if (result == EventYield::budget)
            continue;
        if (result != EventYield::operation)
            return true;
        FieldFlagAccess flags(owned_.state);
        if (const auto* timer =
                std::get_if<EventSetTimer>(&owned_.events.pending_request()->operation)) {
            owned_.timers.at(timer->identity) =
                {timer->frames, timer->event, timer->pause_in_menu_battle,
                 timer->visible_field, timer->visible_menu_battle, timer->terminate_external};
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* timer =
                std::get_if<EventResetTimer>(&owned_.events.pending_request()->operation)) {
            owned_.timers.at(timer->identity) = {};
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* scroll =
                std::get_if<EventBackgroundScroll>(&owned_.events.pending_request()->operation)) {
            auto& layer = owned_.actions.scroll.layers.at(scroll->layer);
            layer[0].background_rate = scroll->x;
            layer[1].background_rate = scroll->y;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* palette =
                std::get_if<EventLoadPalette>(&owned_.events.pending_request()->operation)) {
            const auto& colors = assets_.objects.sprites.palettes.at(palette->source);
            if (palette->destination < 8)
                display_.load_palette(palette->destination, colors);
            else {
                owned_.object_base_colors[palette->destination - 8] = colors;
                owned_.object_colors[palette->destination - 8] = colors;
            }
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* filter = std::get_if<EventPaletteFilter>(&owned_.events.pending_request()->operation)) {
            if (filter->first < 128)
                display_.filter_palette(filter->channels, filter->first, std::min<std::uint8_t>(filter->last, 127));
            for (unsigned i = std::max<unsigned>(filter->first, 128); i <= filter->last; ++i) {
                const unsigned palette = (i - 128) / 16, color = i % 16;
                owned_.object_colors[palette][color] =
                    filter_field_color(owned_.object_base_colors[palette][color], filter->channels);
            }
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* palette = std::get_if<EventBackgroundPalette>(&owned_.events.pending_request()->operation)) {
            display_.modify_palette(palette->control, palette->first, palette->last);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* palette = std::get_if<EventSpritePalette>(&owned_.events.pending_request()->operation)) {
            modify_field_sprite_palettes(owned_.object_colors, owned_.object_base_colors,
                                         palette->control, palette->first, palette->last);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (std::holds_alternative<EventRestoreColorMath>(owned_.events.pending_request()->operation)) {
            display_.restore_color_math();
            owned_.flash = {};
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* color =
                std::get_if<EventFixedColor>(&owned_.events.pending_request()->operation)) {
            owned_.flash.ramp(color->control, color->subtract);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* flash =
                std::get_if<EventScreenFlash>(&owned_.events.pending_request()->operation)) {
            owned_.flash.flash(flash->channels);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* rate = std::get_if<EventBackgroundRate>(&owned_.events.pending_request()->operation)) {
            display_.set_background_rate(rate->group, rate->rate);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* portrait = std::get_if<EventPortrait>(&owned_.events.pending_request()->operation)) {
            owned_.portrait = portrait->identity;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* pyramid = std::get_if<EventPyramid>(&owned_.events.pending_request()->operation)) {
            owned_.pyramid.object = pyramid->object;
            if (!pyramid->object)
                owned_.pyramid.spans = {};
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* light = std::get_if<EventFlashlight>(&owned_.events.pending_request()->operation)) {
            owned_.flashlight.active = true;
            owned_.flashlight.target = light->radius;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* mosaic = std::get_if<EventScreenMosaic>(&owned_.events.pending_request()->operation)) {
            owned_.player.effects.mosaic_speed = mosaic->speed;
            owned_.player.effects.mosaic_counter = FF6_ROM_VALUE_FIELD_EVENT_MOSAIC_START_0;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* shake = std::get_if<EventScreenShake>(&owned_.events.pending_request()->operation)) {
            owned_.shake.control = shake->control;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (std::holds_alternative<EventClearOverlay>(owned_.events.pending_request()->operation)) {
            clear_field_overlay(owned_.player.overlay);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* fade =
                std::get_if<EventScreenFade>(&owned_.events.pending_request()->operation)) {
            // Retain all operand bits when deciding active/direction; only the
            // numerical step is five-bit. Fade-in with rate zero is already idle.
            const unsigned control = fade->rate | (fade->fade_out ? 128U : 0U);
            owned_.transition.fade = {static_cast<std::uint8_t>(fade->fade_out
                                          ? FF6_ROM_VALUE_FIELD_EVENT_FADE_BRIGHTNESS_0
                                          : FF6_ROM_VALUE_FIELD_EVENT_FADE_BRIGHTNESS_1),
                                      static_cast<std::uint8_t>(control & 31U),
                                      control == 0            ? FieldFadeDirection::steady
                                      : (control & 128U) != 0 ? FieldFadeDirection::darker
                                                              : FieldFadeDirection::brighter};
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* adjustment =
                std::get_if<EventMusicAdjustment>(&owned_.events.pending_request()->operation)) {
            if (adjustment->pitch)
                audio_.push_back(MusicPitchRamp{adjustment->duration, adjustment->target});
            else
                audio_.push_back(MusicTempoRamp{adjustment->duration, adjustment->target});
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* stop = std::get_if<EventStopSound>(&owned_.events.pending_request()->operation)) {
            audio_.push_back(StopPlayback{stop->group});
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* volume =
                std::get_if<PlaybackVolumeRamp>(&owned_.events.pending_request()->operation)) {
            audio_.push_back(*volume);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* pan =
                std::get_if<EffectPanRamp>(&owned_.events.pending_request()->operation)) {
            audio_.push_back(*pan);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (std::holds_alternative<EventStopScreenFade>(
                owned_.events.pending_request()->operation)) {
            owned_.transition.fade.step = 0;
            owned_.transition.fade.direction = FieldFadeDirection::steady;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* camera =
                std::get_if<EventCameraLock>(&owned_.events.pending_request()->operation)) {
            // Event 38/39 only changes following eligibility. Retain the current
            // position and rates; the shared action owner governs later movement.
            owned_.actions.camera_locked = camera->locked;
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* song =
                std::get_if<EventPlaySong>(&owned_.events.pending_request()->operation)) {
            require(song->identity < 85, "Event requests a song outside the supported content");
            if (song->suspend_current && song->identity != owned_.map_song)
                owned_.suspended_song = owned_.map_song;
            if (song->update_map_song)
                owned_.map_song = song->identity;
            // F6/10 and F6/11 retain the map's return song. All go through the
            // session's ordinary song selector, which suppresses duplicate loads.
            audio_.push_back(SongLoad{song->identity, song->volume, song->alternate,
                                     song->suspend_current});
            if (song->fade_in)
                audio_.push_back(PlaybackVolumeRamp{PlaybackGroup::music, *song->fade_in, 255});
            // Keep this request and event visit alive across display/audio work.
            // Actor actions, dialogue reveal and event waits cannot advance here.
            audio_resume_ = phase_;
            phase_ = Phase::audio_wait;
            return false;
        }
        if (const auto* resume =
                std::get_if<EventResumeSong>(&owned_.events.pending_request()->operation)) {
            owned_.map_song = owned_.suspended_song;
            audio_.push_back(SongLoad{owned_.map_song, 0});
            audio_.push_back(PlaybackVolumeRamp{PlaybackGroup::music, resume->fade, 255});
            audio_resume_ = phase_;
            phase_ = Phase::audio_wait;
            return false;
        }
        if (std::holds_alternative<EventContinueSong>(
                owned_.events.pending_request()->operation)) {
            audio_.push_back(MusicCue{});
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* fade =
                std::get_if<EventFadeSong>(&owned_.events.pending_request()->operation)) {
            audio_.push_back(PlaybackVolumeRamp{PlaybackGroup::music, fade->duration, 0});
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* removed =
                std::get_if<EventDeleteObject>(&owned_.events.pending_request()->operation)) {
            // Empty party selectors have no visible object or occupancy to
            // remove. Do not manufacture the original unused dummy-object RAM.
            if (const auto identity = find_field_object_reference(owned_.state, removed->object))
                delete_field_object(owned_.state, owned_.actions.occupancy, *identity);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* hide =
                std::get_if<EventHideObject>(&owned_.events.pending_request()->operation)) {
            if (const auto identity = find_field_object_reference(owned_.state, hide->object))
                hide_field_object(owned_.state, owned_.actions.occupancy, *identity);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (const auto* effect =
                std::get_if<EventGameSoundEffect>(&owned_.events.pending_request()->operation)) {
            audio_.push_back(FieldSoundEffect{effect->identity, 128});
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (std::holds_alternative<EventRandomBranch>(owned_.events.pending_request()->operation)) {
            // C0/B271 shares Rand with actor movement and screen shake.
            // Draw only when accepting the request, never on budget retries.
            owned_.actions.random_index = wrap8(unsigned{owned_.actions.random_index} + 1U);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags, {}, {},
                                           {}, {},
                                           assets_.actions.random[owned_.actions.random_index]);
            continue;
        }
        if (const auto* optimum =
                std::get_if<EventOptimizeEquipment>(&owned_.events.pending_request()->operation)) {
            // C0/B08C calls the same C3/96D2 Optimum entry as the equipment
            // menu, then refreshes field equipment effects. No stock is created.
            auto next = owned_.state;
            save_field_membership(next);
            const auto members = current_menu_party(next.roster);
            require(std::ranges::find(members, optimum->character) != members.end(),
                    "Event Optimum requires a character in the active party");
            optimize_menu_equipment(assets_.menu_items, assets_.party, assets_.equipment,
                                    next.inventory, next.roster.characters[optimum->character]);
            refresh_field_equipment(next, assets_.party, assets_.equipment);
            owned_.state = std::move(next);
            owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
            continue;
        }
        if (consume_field_party_control(owned_) ||
            queue_field_battle_request(owned_, assets_.battles, assets_.battle_background) ||
            owned_.dialogue.consume(assets_.dialogue, owned_.events, flags) ||
            consume_field_map_edit(assets_.map, updates_, owned_.events, flags) ||
            consume_field_party_request(owned_.state, owned_.actions.occupancy, owned_.events) ||
            queue_field_map_request(owned_.state, owned_.transition, owned_.events) ||
            consume_field_request(owned_.state, owned_.events, assets_.party, assets_.equipment))
            continue;
        return false;
    }
    return false;
}
void FieldLoop::run_actions(bool startup) {
    FieldActionPass pass(owned_.state.field.event_visits);
    while (!pass.complete()) {
        const auto result =
            pass.advance(owned_.state, owned_.actions, assets_.map, assets_.actions,
                         {event_running(), false, owned_.transition.pending.has_value()});
        if (result.boundary == FieldActionBoundary::event) {
            pass.consume_event(owned_.state, assets_.actions, owned_.events);
        } else if (result.boundary == FieldActionBoundary::player) {
            const auto effect = pass.consume_player(
                owned_.state, owned_.actions, owned_.player, assets_.map, assets_.actions,
                assets_.player, owned_.events,
                {owned_.input.held, !startup && (owned_.transition.fade.brightness >> 4U) == 15,
                 false, owned_.transition.pending.has_value(), owned_.dialogue.open(),
                 updates_.status(), overlay_view(owned_.actions.scroll).origin});
            if (effect.door) {
                updates_.request(0, true);
                audio_.push_back(FieldSoundEffect{effect.door->sound_effect});
            }
            if (effect.treasure) {
                if (effect.treasure->update_background)
                    updates_.request(0, true);
                audio_.push_back(FieldSoundEffect{effect.treasure->sound_effect});
            }
            if (effect.close_map_title())
                close_map_title();
        }
    }
}
void FieldLoop::move_objects() {
    const auto tile = owned_.player.local.cells[7];
    step_field_objects(owned_.state.field, assets_.objects,
                       {owned_.state.field.event_visits, owned_.vblank, (tile.flags & 192U) != 0,
                        tile.directions != 255 && (tile.directions & 64U) != 0});
}
void FieldLoop::retain_drawing() {
    drawn_ = owned_.state.field;
    drawn_terrain_ = owned_.actions.player_terrain;
    drawn_overlay_ = owned_.player.overlay;
    drawn_scroll_ = owned_.actions.scroll;
    drawn_vblank_ = owned_.vblank;
}
void FieldLoop::finish_startup() {
    if (owned_.fade_in)
        owned_.transition.fade = {
            static_cast<std::uint8_t>(FF6_ROM_VALUE_FIELD_STARTUP_FADE_0),
            static_cast<std::uint8_t>(FF6_ROM_VALUE_FIELD_STARTUP_FADE_0 >> 8U),
            FieldFadeDirection::brighter};
    auto& field = owned_.state.field;
    auto& actor = field.objects[*field.update_order[0]];
    owned_.player.local = field_local_tiles(assets_.map, actor.x, actor.y);
    owned_.actions.player_tile = owned_.player.local.cells[7];
    initialize_field_player_terrain(actor, owned_.actions.player_terrain, owned_.player.local,
                                    owned_.travel.saved_level);
    update_field_overlay(owned_.player.overlay, assets_.player.overlay, assets_.player.layouts,
                         assets_.map, actor, owned_.actions.player_terrain, owned_.player.local,
                         overlay_view(owned_.actions.scroll));
    owned_.startup.reset();
    if (owned_.map_presentation_pending) {
        if (const auto song = field_map_song(assets_.map.definition.song,
                                             owned_.state.flags.story[0x1cc], owned_.map_song))
            audio_.push_back(*song);
        owned_.travel.run_startup = owned_.travel.keep_dimensions = false;
        owned_.travel.fade_in = true;
        owned_.fade_in = false;
        if (owned_.travel.placement.show_name && !owned_.state.flags.story[0x1ce])
            owned_.dialogue.show_map_title(assets_.dialogue, assets_.map_title);
        owned_.travel.placement.show_name = owned_.dialogue.title_remaining() != 0;
        owned_.map_presentation_pending = false;
    }
    // LoadMap initializes poses for every active object without moving them.
    for (std::size_t i = 0; i < field.update_count; ++i) {
        require(field.update_order[i] && *field.update_order[i] < field.objects.size(),
                "Invalid initial field object order");
        auto& object = field.objects[*field.update_order[i]];
        FieldPoseInput pose;
        pose.facing = static_cast<FieldFacing>(object.facing);
        pose.ride = object.ride;
        pose.walking = object.walking_animation;
        pose.moving_x = object.velocity_x != 0;
        pose.moving_y = object.velocity_y != 0;
        require(object.x <= 0xffffff && object.y <= 0xffffff, "Invalid initial field position");
        pose.x = wrap16(object.x >> 8U);
        pose.y = wrap16(object.y >> 8U);
        pose.vblank_count = owned_.vblank;
        pose.logic_count = field.event_visits;
        pose.diagonal_tile = (owned_.actions.player_tile.flags & 192U) != 0;
        pose.party_on_ladder = field.update_order[i] == field.update_order[0] &&
                               owned_.actions.player_tile.directions != 255 &&
                               (owned_.actions.player_tile.directions & 64U) != 0;
        pose.special = object.special_pose;
        object.pose = object.pending_pose =
            select_field_pose(assets_.objects.sprites.selection, pose, object.pending_pose);
    }
    for (unsigned phase = 0; phase < 4; ++phase) {
        field.event_visits = static_cast<std::uint8_t>(phase);
        publish_field_objects(field, assets_.objects, publication_);
    }
    retain_drawing();
    phase_ = Phase::frame_begin;
}
void FieldLoop::publish_frame(bool advance_palette, std::uint8_t mosaic_size) {
    const auto pulse_size = advance_field_mosaic(owned_.player.effects);
    if (mosaic_size == 1) mosaic_size = pulse_size;
    if (display_.displaying())
        display_.finish();
    updates_.publish_frame();
    publish_field_objects(owned_.state.field, assets_.objects, publication_);
    owned_.dialogue.publish();
    display_.commit(advance_field_fade(owned_.transition.fade), updates_.visible());
    const auto origins = field_scroll_origins(drawn_scroll_);
    owned_.shake.advance((owned_.vblank & 1U) != 0, assets_.actions.random, owned_.actions.random_index);
    auto shaken_origins = origins;
    for (unsigned i = 0; i < 3; ++i) shaken_origins[i].y += owned_.shake.offsets[i];
    const bool text_only = owned_.dialogue.open() && owned_.dialogue.status().text_only;
    dialogue_backgrounds_ = text_only;
    published_origins_ = shaken_origins;
    published_flash_ = owned_.flash;
    published_mosaic_ = mosaic_size;
    if (owned_.flashlight.active) {
        const auto& object = owned_.state.field.objects[
            owned_.state.field.update_order[0].value_or(FieldObjects::camera)];
        owned_.flashlight.advance(
            static_cast<std::uint8_t>(static_cast<std::int32_t>(object.x >> 8) - origins[0].x + FF6_ROM_VALUE_FIELD_WINDOW_ANCHOR_X_0),
            static_cast<std::uint8_t>(static_cast<std::int32_t>(object.y >> 8) - origins[0].y - FF6_ROM_VALUE_FIELD_FLASHLIGHT_ANCHOR_Y_0));
    }
    published_window_ = {owned_.flashlight.active, owned_.flashlight.spans};
    if (owned_.pyramid.object) {
        const auto& object = owned_.state.field.objects.at(*owned_.pyramid.object);
        owned_.pyramid.advance(
            wrap8(static_cast<std::uint32_t>(static_cast<std::int32_t>(object.x >> 8) - origins[0].x + FF6_ROM_VALUE_FIELD_WINDOW_ANCHOR_X_0)),
            wrap8(static_cast<std::uint32_t>(static_cast<std::int32_t>(object.y >> 8) - origins[0].y)),
            assets_.window_sine);
        // FieldMain writes the pyramid after the flashlight. These producers
        // share an aperture; their masks must not be intersected together.
        published_window_ = {true, owned_.pyramid.spans};
    }
    const bool light = published_window_.active;
    // Text-only bands reveal both underlying backgrounds even when the map
    // normally hides them. Including both guarantees their first two indices.
    auto scene = display_.scene(shaken_origins, {text_only || light, text_only || light, light});
    // LoadMap clears the sprite list. Initial pose/graphics publication does
    // not draw actors; the first ordinary DrawObjSprites produces that list.
    if (drawing_ready_) {
        auto overlay = overlay_view(drawn_scroll_);
        overlay.shake_y = owned_.shake.offsets[3];
        const auto masks = make_field_overlay_sprites(
            assets_.player.overlay,
            project_field_overlay(drawn_overlay_, overlay),
            owned_.object_colors, assets_.map.composition.objects.mix);
        scene.objects = make_field_object_layer(drawn_, assets_.objects, publication_,
                                                {{origins[0], origins[1]},
                                                 owned_.shake.offsets[3],
                                                 drawn_terrain_.low_priority,
                                                 owned_.object_colors,
                                                 assets_.map.composition.objects,
                                                 drawn_vblank_},
                                                &masks);
    }
    if (owned_.timers[0].visible_field)
        draw_field_event_timer(scene, assets_.timer_graphics, owned_.timers[0].remaining,
                               owned_.object_colors[0]);
    owned_.flash.apply(scene);
    published_window_.apply(scene, assets_.map.definition.window_mode);
    if (mosaic_size != 1)
        apply_field_mosaic(scene, mosaic_size);
    dialogue_underlay_ = std::move(scene);
    compose_dialogue();
    ++frames_;
    advance_game_time(owned_.menu.time, owned_.menu.time_frames);
    owned_.vblank = wrap8(unsigned{owned_.vblank} + 1U);
    if (advance_palette)
        display_.advance_palette();
}
void FieldLoop::compose_dialogue() {
    if (!dialogue_underlay_)
        return;
    const bool text_only = owned_.dialogue.open() && owned_.dialogue.status().text_only;
    if (text_only && !dialogue_backgrounds_) {
        // A text-only message may start after field publication. Materialize
        // hidden backgrounds only when this actual caller needs them, retaining
        // that frame's origins, effects and already published object plane.
        auto backgrounds = display_.scene(published_origins_, {true, true, published_window_.active});
        auto flash = published_flash_;
        flash.apply(backgrounds);
        published_window_.apply(backgrounds, assets_.map.definition.window_mode);
        if (published_mosaic_ != 1)
            apply_field_mosaic(backgrounds, published_mosaic_);
        dialogue_underlay_->layers = std::move(backgrounds.layers);
        dialogue_backgrounds_ = true;
    }
    auto scene = *dialogue_underlay_;
    if (owned_.ending_bars)
        compose_field_ending_bars(scene, assets_.dialogue);
    owned_.dialogue.compose(scene, assets_.dialogue, owned_.window_skin, display_.map().colors,
                            text_only ? std::optional{std::array<std::size_t, 2>{0, 1}}
                                      : std::nullopt);
    visible_ = std::move(scene);
}
void FieldLoop::publish_battle_transition(Buttons buttons, std::uint8_t mosaic_size, bool visible) {
    if (phase_ != Phase::stopped || stopped_ != FieldLoopBoundary::battle || !owned_.battle)
        throw std::logic_error("No suspended field battle transition");
    // Dialogue bands change mosaic's vertical restart point. Do not silently
    // approximate that unverified path with the ordinary full-field profile.
    if (owned_.dialogue.title_remaining() != 0 ||
        std::ranges::any_of(owned_.dialogue.bands(), [](auto value) { return value != 0; }))
        throw std::runtime_error("Battle mosaic with visible dialogue is not implemented yet");
    auto next = *this;
    next.publish_frame(false, mosaic_size);
    sample_input(next.owned_.input, next.owned_.settings, InputContext::field, buttons, 0);
    next.owned_.palette = next.display_.palette();
    next.owned_.third_phase = next.display_.third_phase();
    if (!visible) {
        RasterScene blank;
        blank.intensity.resize(blank.height);
        next.visible_ = std::move(blank);
    }
    *this = std::move(next);
}
bool FieldLoop::advance_external_timers() {
    bool expired = false;
    for (auto& timer : owned_.timers)
        if (!timer.pause_in_menu_battle) {
            if (timer.remaining)
                --timer.remaining;
            else if (timer.terminate_external)
                expired = true;
        }
    // C0/BB24 refreshes the shared field/battle timer flags on every external
    // update. In particular, a previous timeout must not leak into the next
    // encounter after its timer has been stopped. Keep unrelated story bits.
    auto& flags = owned_.state.battle_variables[8];
    flags = static_cast<std::uint8_t>((flags & 0x9fU) |
        (owned_.timers[0].visible_menu_battle ? 0x40U : 0U) | (expired ? 0x20U : 0U));
    return expired;
}
void FieldLoop::check_timers() {
    if (event_running())
        return;
    auto& actor = owned_.state.field.objects[*owned_.state.field.update_order[0]];
    // CheckTimer tests the integer pixel's low nibble, not subpixel precision
    // or player-control mode. A busy script postpones dispatch, not countdown.
    if (!aligned(actor, false))
        return;
    for (auto& timer : owned_.timers) {
        if (timer.remaining || !timer.event)
            continue;
        owned_.events.begin_object_event(timer.event);
        timer.event = 0;
        actor.saved_movement = actor.movement;
        actor.movement.mode = 4;
        close_map_title();
        return; // Lowest timer slot wins; other expirations remain pending.
    }
}
void FieldLoop::check_trigger() {
    if (owned_.transition.pending || event_running())
        return;
    auto& actor = owned_.state.field.objects[*owned_.state.field.update_order[0]];
    if (!aligned(actor, true) || actor.movement.mode != 2)
        return;
    for (const auto& trigger : assets_.triggers) {
        if (trigger.position != FieldTilePosition{owned_.actions.player_tile_position[0],
                                                  owned_.actions.player_tile_position[1]})
            continue;
        owned_.events.begin_object_event(trigger.event);
        actor.velocity_x = actor.velocity_y = 0;
        actor.move_direction = 0;
        owned_.player.effects.on_trigger = true;
        actor.saved_movement = actor.movement;
        actor.movement = {4, 0};
        const auto& camera = owned_.state.field.objects[FieldObjects::camera];
        update_field_scroll_rate(owned_.actions.scroll, owned_.actions.scroll_limits,
                                 assets_.map.parallax.multiplier,
                                 {owned_.actions.camera_locked,
                                  false,
                                  owned_.actions.player_tile_position,
                                  {0, 0},
                                  {camera.velocity_x, camera.velocity_y}});
        close_map_title();
        return;
    }
}
std::optional<FieldLoopBoundary> FieldLoop::check_handoffs() {
    if (owned_.transition.ready_to_load())
        return FieldLoopBoundary::map_load;
    if (owned_.battle)
        return FieldLoopBoundary::battle;
    if (event_running() || owned_.transition.pending)
        return {};
    const auto& actor = selected(owned_.state.field);
    const auto entrance = check_field_entrances(
        owned_.state.field, owned_.transition, owned_.travel, assets_.entrances,
        {{owned_.actions.player_tile_position[0], owned_.actions.player_tile_position[1]},
         owned_.actions.player_tile.flags,
         owned_.actions.player_terrain.level,
         false,
         owned_.player.effects.entrances_disabled,
         false,
         false,
         assets_.player.idle_event});
    if (entrance.matched())
        owned_.player.effects.on_trigger = true;
    // New ordinary maps rebuild special-NPC graphics counts and scroll bounds.
    // Their scratch writes do not affect this continuing pre-load scene.
    if (owned_.transition.pending)
        return {};
    // Encounter eligibility consumes the successful step once, even when its
    // probability draw does not select a battle.
    if (assets_.random_encounters && owned_.player.effects.encounters_enabled &&
        !owned_.player.effects.on_trigger && !owned_.state.flags.story[0x1cd] &&
        aligned(actor, true))
        (void)check_field_random_battle(owned_, assets_.battles, assets_.battle_background);
    return {};
}
FieldLoopResult FieldLoop::advance(Buttons first, Buttons second, std::size_t budget,
                                   bool audio_ready, bool music_finished,
                                   std::optional<std::uint8_t> music_position) {
    // Public boundaries are transactional, including a malformed event reached
    // after earlier owners have run. Share immutable assets later if profiling
    // establishes that this correctness-first copy is significant.
    auto next = *this;
    next.music_finished_ = music_finished;
    next.music_position_ = music_position;
    const auto result = next.advance_impl(first, second, budget, audio_ready);
    next.owned_.palette = next.display_.palette();
    next.owned_.third_phase = next.display_.third_phase();
    *this = std::move(next);
    return result;
}
bool FieldLoop::check_party_request() {
    if (!owned_.state.flags.story[0x1ce] || owned_.transition.pending ||
        owned_.actions.collision.stage != FieldCollision::Stage::none || event_running() ||
        !aligned(selected(owned_.state.field), true))
        return false;
    // This latch updates only after the eligibility gates. A press edge sampled
    // while walking cannot substitute for the original held-button behavior.
    if (!held(owned_.input.held, Button::y)) {
        owned_.party_switch_ready = true;
        return false;
    }
    return owned_.party_switch_ready;
}
bool FieldLoop::menu_requested() const {
    const auto& actor = selected(owned_.state.field);
    return held(owned_.input.held, Button::x) && !owned_.transition.pending &&
           !owned_.transition.fade.active() &&
           owned_.actions.collision.stage == FieldCollision::Stage::none && !event_running() &&
           actor.move_direction == 0 && aligned(actor, true) && !owned_.state.flags.story[0x1c2];
}
FieldLoopResult FieldLoop::advance_impl(Buttons first, Buttons second, std::size_t budget,
                                        bool audio_ready) {
    require(budget != 0, "Field loop needs a nonzero work budget");
    if (phase_ == Phase::stopped)
        return {stopped_};
    if (phase_ == Phase::audio_wait) {
        require(audio_resume_.has_value(), "Song wait lost its event continuation");
        if (*audio_resume_ == Phase::frame_events) {
            publish_frame(false);
            sample_input(owned_.input, owned_.settings, InputContext::field, first, second);
        }
        // Startup runs with field interrupts disabled. It waits without field
        // publication; the session still services audio using a blank display.
        if (!audio_ready)
            return {FieldLoopBoundary::audio_wait};
        FieldFlagAccess flags(owned_.state);
        owned_.events.complete_request(owned_.events.pending_request()->identity, flags);
        phase_ = *audio_resume_;
        audio_resume_.reset();
    }
    if (phase_ == Phase::frame_begin) {
        if (owned_.transition.ready_to_load()) {
            phase_ = Phase::stopped;
            stopped_ = FieldLoopBoundary::map_load;
            return {stopped_};
        }
        publish_frame();
        sample_input(owned_.input, owned_.settings, InputContext::field, first, second);
        const auto& actor = selected(owned_.state.field);
        owned_.actions.player_tile_position = {wrap8(actor.x >> 12U), wrap8(actor.y >> 12U)};
        check_timers();
        check_trigger();
        for (auto& timer : owned_.timers)
            if (timer.remaining)
                --timer.remaining;
        begin_field_event_visit(owned_.state, owned_.transition.pending.has_value());
        phase_ = Phase::frame_events;
    } else if (phase_ == Phase::startup_begin) {
        begin_field_event_visit(owned_.state, owned_.transition.pending.has_value());
        phase_ = Phase::startup_events;
    }
    if (phase_ == Phase::startup_events || phase_ == Phase::startup_extra ||
        phase_ == Phase::frame_events) {
        if (!run_events(budget)) {
            if (phase_ == Phase::audio_wait)
                return {FieldLoopBoundary::audio_wait};
            return {owned_.events.pending_request() ? FieldLoopBoundary::event_request
                                                    : FieldLoopBoundary::budget};
        }
        if (phase_ == Phase::startup_extra) {
            finish_startup();
            return {FieldLoopBoundary::startup_visit};
        }
        if (phase_ == Phase::startup_events) {
            if (owned_.state.flags.story[0x1cf]) {
                run_actions(true);
                move_objects();
                updates_.prepare(assets_.map, field_map_centers(owned_.actions.scroll));
                updates_.publish();
                phase_ = Phase::startup_begin;
            } else if (owned_.events.returning_to(assets_.player.idle_event)) {
                begin_field_event_visit(owned_.state, owned_.transition.pending.has_value());
                phase_ = Phase::startup_extra;
                return budget == 0 ? FieldLoopResult{FieldLoopBoundary::budget}
                                   : advance_impl(first, second, budget, audio_ready);
            } else {
                finish_startup();
            }
            return {FieldLoopBoundary::startup_visit};
        }
        phase_ = Phase::after_events;
    }
    if (const auto handoff = check_handoffs()) {
        phase_ = Phase::stopped;
        stopped_ = *handoff;
        return {*handoff};
    }
    display_.refresh_text_color(owned_.font_color);
    FieldDialogueValues substitutions;
    for (std::size_t i = 0; i < substitutions.names.size(); ++i)
        substitutions.names[i] = owned_.state.roster.characters[i].name;
    substitutions.gil = owned_.player.treasure.message_gil;
    substitutions.item = owned_.player.treasure.message_item;
    substitutions.spell = owned_.message_spell;
    owned_.dialogue.advance(assets_.dialogue, substitutions, owned_.input.held);
    // UpdateDlgWindow changes live window bands after ExecEvent. Freezing them
    // with the preceding sprite/glyph publication delayed opening by a frame.
    compose_dialogue();
    if (owned_.dialogue.title_remaining() == 0)
        owned_.travel.placement.show_name = false;
    run_actions(false);
    updates_.notify_scroll(advance_field_scroll(owned_.actions.scroll));
    move_objects();
    updates_.prepare(assets_.map, field_map_centers(owned_.actions.scroll));
    updates_.prepare_scroll(assets_.map, owned_.actions.scroll);
    latch_field_object_poses(owned_.state.field);
    retain_drawing();
    drawing_ready_ = true;
    if (check_party_request()) {
        phase_ = Phase::stopped;
        stopped_ = FieldLoopBoundary::menu_or_party;
        return {stopped_};
    }
    if (activate_field_contact(owned_.state, owned_.actions, owned_.events, assets_.actions.poses,
                               assets_.player.idle_event))
        close_map_title();
    if (menu_requested()) {
        phase_ = Phase::stopped;
        stopped_ = FieldLoopBoundary::menu_or_party;
        return {stopped_};
    }
    phase_ = Phase::frame_begin;
    return {FieldLoopBoundary::frame};
}
std::vector<FieldAudioCommand> FieldLoop::take_audio() {
    auto result = std::move(audio_);
    audio_.clear();
    return result;
}
void FieldLoop::complete_cutscene(std::uint64_t identity, EventCutscene expected, bool skipped) {
    const auto& request = owned_.events.pending_request();
    require(request && request->identity == identity &&
                (phase_ == Phase::frame_events || phase_ == Phase::startup_events ||
                 phase_ == Phase::startup_extra),
            "No retained cutscene event to resume");
    const auto* operation = std::get_if<EventRunCutscene>(&request->operation);
    require(operation && operation->scene == expected,
            "Cutscene completion does not own this event request");
    // PushDP/PopDP preserve the field visit and its input/timing/RNG owners.
    // Only the cutscene result and event continuation change here.
    FieldFlagAccess flags(owned_.state);
    owned_.events.complete_request(identity, flags, skipped);
}
void FieldLoop::close_map_title() {
    close_title_ = true;
    owned_.dialogue.close_map_title();
    owned_.travel.placement.show_name = false;
}
bool FieldLoop::take_close_map_title() {
    const bool result = close_title_;
    close_title_ = false;
    return result;
}
} // namespace ff6
