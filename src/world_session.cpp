#include "ff6/field_session.hpp"
#include "rom-materials/world_veldt_song.macros.inc"
#include "rom-materials/world_session_effects.macros.inc"
#include "rom-materials/world_airship_initial_frame.macros.inc"
#include "rom-materials/world_pigeon_frame_period.macros.inc"
#include "rom-materials/world_vehicle_battle_duration.macros.inc"
#include "rom-materials/world_vehicle_battle_split.macros.inc"
#include "rom-materials/world_doom_gaze_selection.macros.inc"
#include "rom-materials/world_dismount_sound_deadline.macros.inc"
#include "rom-materials/world_dismount_fade_deadline.macros.inc"
#include "rom-materials/world_poison_mosaic_parameters.macros.inc"
#include "rom-materials/world_battle_zoom_span.macros.inc"
#include "rom-materials/approach_duration.macros.inc"
#include "rom-materials/approach_fade_threshold.macros.inc"
#include "rom-materials/audio_loading_choices.macros.inc"
#include "ff6/game_time.hpp"
#include <algorithm>

namespace ff6 {
void FieldSession::enter_world(const ContentPack& pack, const FieldMapLoadPlan& plan) {
    const auto arrival = std::get<WorldMapArrival>(plan.arrival);
    if (plan.map != 2 && arrival.vehicle == 3)
        throw std::runtime_error("This world vehicle requires its own traversal owner");
    const auto script = field_event_script(pack);
    if (arrival.event_resume >= script.size())
        throw std::runtime_error("Invalid world entry event");
    world_script_ = script[arrival.event_resume] == 0xff
                        ? std::nullopt
                        : std::optional{WorldScriptState{arrival.event_resume, 0, {}}};
    WorldState world;
    world.minimap = scene_.owned_.world_minimap;
    world.map = static_cast<std::uint8_t>(plan.map);
    world.x = static_cast<std::uint16_t>(arrival.position.x * 16U);
    world.y = static_cast<std::uint16_t>(arrival.position.y * 16U);
    const auto facing = plan.return_facing.value_or(scene_.owned_.travel.return_facing);
    world.facing = (facing & 128U) ? facing & 3U : static_cast<std::uint8_t>(arrival.header_facing);
    if (arrival.vehicle == 2)
        enter_world_chocobo(world);
    if (plan.map == 2 || arrival.vehicle == 1) {
        world.vehicle.emplace();
        world.minimap = plan.map != 2 && scene_.owned_.world_minimap;
        if (plan.map != 2) {
            scene_.owned_.state.flags.story.reset(0x1bb); // InitAirship leaves the Veldt.
            if (scene_.owned_.airship_resume) {
                world.vehicle->heading = unsigned{scene_.owned_.airship_resume->heading} * 256U;
                world.vehicle->altitude = scene_.owned_.airship_resume->altitude;
                scene_.owned_.airship_resume.reset();
            }
            world.vehicle->sprite_frame = FF6_ROM_VALUE_WORLD_AIRSHIP_INITIAL_FRAME_0;
        }
    }
    scene_.owned_.world = world;
    scene_.owned_.state.field.parent_map = plan.parent;
    scene_.owned_.transition.pending.reset();
    if (!world_script_)
        scene_.owned_.events = FieldEventRunner::parked(script, scene_.assets_.player.idle_event);
    reload_world(pack);
}
void FieldSession::begin_world_event(const ContentPack& pack, FieldEventRunner& events,
                                    std::uint32_t entry) {
    // Independent world interactions need a fresh field return owner. An
    // authored world-to-field continuation then resumes this invocation,
    // never an old transport script or the parked field's empty call stack.
    events = FieldEventRunner::parked(field_event_script(pack), scene_.assets_.player.idle_event);
    events.begin_object_event(entry);
    world_script_ = WorldScriptState{entry, 0, {}};
}
void FieldSession::reload_world(const ContentPack& pack) {
    auto& seed = scene_.owned_;
    auto& world = *seed.world;
    world_assets_ = load_world_assets(pack, world.map, seed.state.flags);
    if (world.vehicle)
        doom_gaze_locations_ = initialize_doom_gaze_locations(
            seed.actions.random_index, world_assets_->battles.random);
    world.encounter_origin = {wrap8(world.x / 16), wrap8(world.y / 16)};
    world.forest = world.map < 2 && (world_tile_properties(*world_assets_, world) & 32U) != 0;
    world_brightness_ = 0;
    world.clock = 0;
    world.remaining = world.walk_phase = world.walk_clock = world.poison_flash = 0;
    world.dx = world.dy = 0;
    boundary_ = FieldLoopBoundary::frame;
    stopped_ = pending_tick_ = loading_ = false;
    notice_.clear();
}
RasterScene FieldSession::world_scene() const {
    const auto& seed = scene_.state();
    const auto leader = seed.state.field.update_order[0];
    if (!leader || *leader >= 16)
        throw std::logic_error("World has no party leader");
    auto presentation = world_script_ ? world_script_->presentation : WorldPresentation{};
    if (seed.state.flags.story[0x1b9]) presentation.parked_airship = seed.parked_airship;
    return compose_world_scene(
        *world_assets_, *seed.world, seed.state.roster.characters[*leader].graphics,
        seed.state.field.objects[*leader].upper.palette, world_brightness_, FF6_ROM_VALUE_WORLD_ONFOOT_CAMERA_SCALE_0,
        presentation);
}
FieldSessionTick FieldSession::advance_world(const ContentPack& pack, Buttons buttons) {
    if (!world_assets_)
        reload_world(pack);
    auto& seed = scene_.owned_;
    auto& world = *seed.world;
    FieldSessionTick tick;
    sample_input(seed.input, seed.settings, InputContext::field, buttons, 0);
    tick.complete = tick.input_sampled = true;
    ++ticks_;
    advance_game_time(seed.menu.time, seed.menu.time_frames);
    if (!world.clock && !world_exit_ && !world_battle_zoom_) {
        const auto song =
            seed.state.flags.story[0x1cc] ? seed.map_song
            : world.map == 2
                ? world_assets_->trench_songs[unsigned{seed.state.flags.story[0x1b8]}]
            : seed.state.flags.story[0x1bb]
                ? std::uint8_t{FF6_ROM_VALUE_WORLD_VELDT_SONG_0}
                : (world.vehicle ? world_assets_->airship_songs
                   : world.chocobo ? world_assets_->chocobo_songs
                                   : world_assets_->songs)[world.map * 2 +
                                                         unsigned{seed.state.flags.story[0x1b8]}];
        seed.map_song = song;
        tick.audio.push_back(GameSong{song, 255});
    }
    if (world_exit_) {
        if (world_brightness_)
            --world_brightness_;
        visible_ = render_scene(world_scene());
        if (!world_brightness_) {
            const auto plan = *world_exit_;
            if (plan.dispatch_world) {
                world_exit_.reset();
                enter_world(pack, plan);
                ++maps_loaded_;
                return tick;
            }
            seed.transition.pending = *world_exit_;
            seed.transition.fade = {};
            seed.world.reset();
            // Ordinary travel already owns an idle field continuation;
            // authored arrivals and Game Over retain their explicit event.
            auto assets = load_field_map_entry_assets(pack, world_exit_->map, scene_.map());
            auto entry = prepare_field_map_entry(std::move(assets), std::move(seed));
            scene_ = FieldLoop(std::move(entry.assets), std::move(entry.seed));
            world_exit_.reset();
            world_script_.reset();
            world_assets_.reset();
            observed_frames_ = 0;
            ++maps_loaded_;
        }
        return tick;
    }
    // A scripted vehicle encounter suspends its continuation and its momentum.
    // VehicleBattleEffect fades for forty pictures without advancing travel.
    if (world_battle_zoom_ && world.vehicle) {
        if (world_brightness_) --world_brightness_;
        visible_ = render_scene(world_scene());
        if (!--world_battle_zoom_) enter_battle(pack);
        return tick;
    }
    if (world.airship_transition) {
        const bool was_flying = world.vehicle.has_value();
        const bool finished = advance_airship_transition(*world_assets_, world);
        world_brightness_ = world.airship_transition->brightness;
        if (was_flying != world.vehicle.has_value()) {
            if (world.vehicle) {
                seed.state.flags.story.reset(0x1bb);
                doom_gaze_locations_ = initialize_doom_gaze_locations(
                    seed.actions.random_index, world_assets_->battles.random);
            }
            if (!seed.state.flags.story[0x1cc]) {
                const auto song = !world.vehicle && seed.state.flags.story[0x1bb] ? std::uint8_t{FF6_ROM_VALUE_WORLD_VELDT_SONG_0}
                    : (world.vehicle ? world_assets_->airship_songs : world_assets_->songs)
                        [world.map * 2 + unsigned{seed.state.flags.story[0x1b8]}];
                seed.map_song = song;
                tick.audio.push_back(GameSong{song, 255});
            }
        }
        if (finished) world.airship_transition.reset();
        visible_ = render_scene(world_scene());
        return tick;
    }
    if (world_script_) {
        // Publish the camera origin and sprites before the next authored
        // movement/perspective update, independently of host refresh rate.
        if (world.vehicle && !world_script_->presentation.crash && !world_script_->presentation.ruin &&
            !world_script_->presentation.falcon && !world_script_->presentation.judgment)
            prepare_world_vehicle_picture(world);
        const auto exit = advance_world_script(field_event_script(pack), *world_script_, world,
                                               seed.state.flags);
        if (world_script_->parked_airship) {
            seed.parked_airship = world_script_->parked_airship;
            world_script_->parked_airship.reset();
        }
        if (world.vehicle) {
            if (world_script_->presentation.falcon) {
                advance_falcon_rise(*world_assets_, *world_script_->presentation.falcon);
                advance_world_vehicle(*world_assets_, world, 0, seed.state.flags);
            } else if (world_script_->presentation.judgment) {
                auto& shot = *world_script_->presentation.judgment;
                if (advance_world_judgment(*world_assets_, shot))
                    tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_WORLD_SESSION_EFFECTS_0, 128});
                seed.actions.random_index = shot.random;
                advance_world_vehicle(*world_assets_, world, 0, seed.state.flags);
            } else if (world_script_->presentation.ruin) {
                if (advance_world_ruin_explosion(*world_script_->presentation.ruin))
                    tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_WORLD_SESSION_EFFECTS_1, 128});
            } else if (world_script_->presentation.crash) {
                advance_airship_crash(*world_assets_, *world_script_->presentation.crash, world);
            } else if (world_script_->presentation.approach) {
                advance_world_approach(*world_assets_, *world_script_->presentation.approach, world);
                // The cinematic advances the normal airship propellers, but
                // owns its camera position independently of steering input.
                advance_world_vehicle(*world_assets_, world, 0, seed.state.flags);
                const unsigned remaining = unsigned{FF6_ROM_VALUE_APPROACH_DURATION_0} - world_script_->presentation.approach->frame;
                if (remaining < FF6_ROM_VALUE_APPROACH_FADE_THRESHOLD_0)
                    world_script_->presentation.target_brightness = static_cast<std::uint8_t>(remaining / 16);
            } else advance_world_vehicle(*world_assets_, world, seed.input.held, seed.state.flags);
            if (world_script_->presentation.smoke)
                advance_airship_smoke(*world_assets_, *world_script_->presentation.smoke);
            auto& presentation = world_script_->presentation;
            if (presentation.pigeon && !--presentation.pigeon_wait) {
                presentation.pigeon_phase = static_cast<std::uint8_t>((presentation.pigeon_phase + 1U) & 3U);
                presentation.pigeon_wait = FF6_ROM_VALUE_WORLD_PIGEON_FRAME_PERIOD_0;
            }
        }
        // WorldCmd_d6..d9 select a target. WorldMain approaches it by one
        // intensity level per visit, including while the authored script waits.
        const auto target = world_script_->presentation.target_brightness;
        if (world_script_->presentation.falcon)
            world_brightness_ = world_script_->presentation.falcon->brightness;
        else if (world_script_->presentation.judgment)
            world_brightness_ = world_script_->presentation.judgment->brightness;
        else if (world_script_->presentation.ruin)
            world_brightness_ = world_script_->presentation.ruin->brightness;
        else if (world_script_->presentation.crash)
            world_brightness_ = static_cast<std::uint8_t>(world_script_->presentation.crash->brightness / 256);
        else if (world_brightness_ < target)
            ++world_brightness_;
        else if (world_brightness_ > target)
            --world_brightness_;
        ++world.clock;
        if (exit) {
            const auto& entrance = exit->entrance;
            if (entrance.map >= 415)
                throw std::runtime_error("Unsupported authored world destination");
            FieldMapLoadPlan plan;
            plan.map = entrance.map;
            plan.dispatch_world = entrance.map < 3;
            if (plan.dispatch_world)
                plan.arrival = WorldMapArrival{entrance.destination,
                    static_cast<FieldFacing>(entrance.facing), exit->vehicle, exit->resume};
            else
                plan.arrival =
                    FieldMapArrival{entrance.destination, static_cast<FieldFacing>(entrance.facing),
                                    entrance.show_name};
            plan.characters = save_field_character_locations(seed.state.field);
            plan.parent = entrance.set_parent ? FieldDestination{world.map, wrap8(world.x / 16),
                                                                 wrap8(world.y / 16),
                                                                 // VehicleCmd_d2/d3 retain parent facing;
                                                                 // only WorldCmd_d2/d3 replace it (EE/7345,7DF4).
                                                                 world.vehicle ? seed.state.field.parent_map.facing
                                                                               : world.facing}
                                              : seed.state.field.parent_map;
            plan.run_startup = (exit->flags & 128U) != 0;
            plan.fade_in = (exit->flags & 64U) == 0;
            plan.keep_dimensions = (exit->flags & 32U) != 0;
            seed.events.resume_from_world(exit->resume);
            world_exit_ = plan;
        } else if (world_script_->battle) {
            const auto [group, background] = *world_script_->battle;
            FieldBattleRequest request;
            request.formation = world_assets_->battles.event_groups[group][seed.menu.time_frames < FF6_ROM_VALUE_WORLD_VEHICLE_BATTLE_SPLIT_0 ? 0 : 1];
            request.background = background;
            request.continue_music = seed.state.flags.story[0x2bc];
            request.field_return.fade_in = true;
            seed.battle = request;
            world_script_->battle.reset();
            world_battle_zoom_ = FF6_ROM_VALUE_WORLD_VEHICLE_BATTLE_DURATION_0;
            tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_10, 128});
        } else if (!world_script_->wait && !world_script_->movement_visits &&
                   !world_script_->walk_distance &&
                   (!world_script_->presentation.castle || world_script_->presentation.castle->finished) &&
                   (!world_script_->presentation.ruin || world_script_->presentation.ruin->finished) &&
                   (!world_script_->presentation.judgment || world_script_->presentation.judgment->finished) &&
                   (!world_script_->presentation.crash || world_script_->presentation.crash->finished) &&
                   (!world_script_->presentation.approach || world_script_->presentation.approach->frame >= FF6_ROM_VALUE_APPROACH_DURATION_0) &&
                   slice(field_event_script(pack), world_script_->position, 1)[0] == 0xff) {
            seed.world_minimap = world.minimap;
            world_script_.reset();
            seed.events = FieldEventRunner::parked(field_event_script(pack),
                                                   scene_.assets_.player.idle_event);
        }
        visible_ = render_scene(world_scene());
        return tick;
    }
    if (world_battle_zoom_) {
        const auto at = (FF6_ROM_VALUE_WORLD_BATTLE_ZOOM_SPAN_0 / 2U - world_battle_zoom_) * 2U;
        const auto leader = *seed.state.field.update_order[0];
        auto image = compose_world_scene(
            *world_assets_, world, seed.state.roster.characters[leader].graphics,
            seed.state.field.objects[leader].upper.palette, world_assets_->battle_zoom[at + 1],
            static_cast<std::uint16_t>(unsigned{world_assets_->battle_zoom[at]} * 256));
        image.objects.reset(); // BattleZoom disables the object plane.
        visible_ = render_scene(image);
        if (!--world_battle_zoom_)
            enter_battle(pack);
        return tick;
    }
    if (world.vehicle) {
        world_brightness_ = std::min<std::uint8_t>(15, world_brightness_ + 1);
        auto& v = *world.vehicle;
        std::optional<std::uint32_t> event;
        // LandAirship tests the tile under the craft before this frame's motion.
        if (held(seed.input.held, Button::b) && !held(seed.input.held, Button::y)) {
            const auto properties = world_tile_properties(*world_assets_, world);
            if (!(properties & 2U)) {
                if (world.map == 1 && (properties & 0xc000U))
                    event = properties & 0x8000U ? world_assets_->kefka_tower_event
                                                : world_assets_->phoenix_cave_event;
                else {
                    if (properties & 0x2000U) seed.state.flags.story.set(0x1bb);
                    seed.parked_airship = FieldTilePosition{wrap8(world.x / 16), wrap8(world.y / 16)};
                    seed.state.flags.story.set(0x1b9);
                    seed.airship_resume.reset();
                    begin_airship_landing(world);
                }
            }
        }
        if (held(seed.input.pressed, Button::start)) {
            world.minimap = !world.minimap;
            seed.world_minimap = world.minimap;
        }
        if (!event && !world.airship_transition && held(seed.input.pressed, Button::x)) {
            seed.airship_resume = WorldFlightResume{static_cast<std::uint16_t>(v.heading / 256), v.altitude};
            event = world_assets_->airship_deck_event;
        }
        if (event) {
            begin_world_event(pack, seed.events, *event);
        } else if (!world.airship_transition) {
            if (encounters_doom_gaze(world, doom_gaze_locations_, seed.input.held,
                                     (seed.state.battle_variables[9] & 1U) != 0)) {
                // EE/6F3A uses the first event-group entry without a random
                // formation draw. Suspend free flight, preserving heading/height.
                v.speed = 0; v.turn = v.climb = 0;
                v.fraction_x = v.fraction_y = 0;
                FieldBattleRequest request;
                request.formation = world_assets_->battles.event_groups[FF6_ROM_VALUE_WORLD_DOOM_GAZE_SELECTION_0 / 4][0];
                request.background = FF6_ROM_VALUE_WORLD_DOOM_GAZE_SELECTION_1;
                request.field_return.fade_in = true;
                seed.battle = request;
                doom_gaze_encounter_ = true;
                world_battle_zoom_ = FF6_ROM_VALUE_WORLD_VEHICLE_BATTLE_DURATION_0;
                tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_10, 128});
            } else {
                prepare_world_vehicle_picture(world);
                advance_world_vehicle(*world_assets_, world, seed.input.held, seed.state.flags, true);
                ++world.clock;
            }
        }
        visible_ = render_scene(world_scene());
        return tick;
    }
    if (world.chocobo) {
        if (held(seed.input.pressed, Button::start)) {
            world.minimap = !world.minimap;
            seed.world_minimap = world.minimap;
        }
        if (advance_world_chocobo(*world_assets_, world, seed.input.held)) {
            if (world_tile_properties(*world_assets_, world) & 0x2000U)
                seed.state.flags.story.set(0x1bb);
            world.chocobo.reset();
            world.x &= 4080;
            world.y &= 4080;
            reload_world(pack);
        } else if (world.chocobo->dismount) {
            const auto age = *world.chocobo->dismount;
            if (age == FF6_ROM_VALUE_WORLD_DISMOUNT_SOUND_DEADLINE_0)
                tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_WORLD_SESSION_EFFECTS_2, 128});
            world_brightness_ =
                age > FF6_ROM_VALUE_WORLD_DISMOUNT_FADE_DEADLINE_0
                    ? static_cast<std::uint8_t>(std::max(0, FF6_ROM_VALUE_WORLD_DISMOUNT_FADE_DEADLINE_0 + 15 - int(age))) : 15;
        } else
            world_brightness_ = std::min<std::uint8_t>(15, world_brightness_ + 1);
        visible_ = render_scene(world_scene());
        return tick;
    }
    if (world.remaining && world.poison_flash)
        world.poison_flash -= FF6_ROM_VALUE_WORLD_POISON_MOSAIC_PARAMETERS_0;
    world_brightness_ = std::min<std::uint8_t>(15, world_brightness_ + 1);
    const bool completed_step = !world.remaining && (world.dx || world.dy);
    if (completed_step) {
        const FieldTilePosition tile{wrap8(world.x / 16), wrap8(world.y / 16)};
        std::optional<WorldEventExit> destination;
        bool on_trigger = false;
        for (const auto& trigger : world_assets_->triggers) {
            if (trigger.position != tile)
                continue;
            on_trigger = true;
            destination =
                resolve_world_event(field_event_script(pack), trigger.event, seed.state.flags);
            break;
        }
        for (const auto& entrance : world_assets_->entrances.short_regions) {
            if (entrance.position != tile)
                continue;
            destination = WorldEventExit{entrance, 0x80};
            break;
        }
        if (destination) {
            const auto& entrance = destination->entrance;
            FieldMapLoadPlan plan;
            plan.map = entrance.map;
            if (plan.map < 3 || plan.map >= 415)
                throw std::runtime_error("Unsupported world entrance destination");
            plan.arrival =
                FieldMapArrival{entrance.destination, static_cast<FieldFacing>(entrance.facing),
                                entrance.show_name};
            plan.characters = save_field_character_locations(seed.state.field);
            plan.parent = entrance.set_parent
                              ? FieldDestination{world.map, tile.x, tile.y, world.facing}
                              : seed.state.field.parent_map;
            plan.run_startup = (destination->flags & 128U) != 0;
            plan.fade_in = (destination->flags & 64U) == 0;
            plan.keep_dimensions = (destination->flags & 32U) != 0;
            world_exit_ = plan;
            world.dx = world.dy = 0;
        }
        if (!world_exit_ && !on_trigger && !seed.state.flags.story[0x1cd]) {
            const auto battle = select_world_encounter(
                *world_assets_, world, seed.battle_random, seed.encounter_counter,
                seed.state.field.equipment_effects, seed.state.flags.story[0x2bc],
                seed.state.encountered_formations);
            if (battle) {
                seed.battle = battle;
                seed.state.field.objects[*seed.state.field.update_order[0]].facing = world.facing;
                world_battle_zoom_ = FF6_ROM_VALUE_WORLD_BATTLE_ZOOM_SPAN_0 / 2;
                world.dx = world.dy = 0;
                tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_10, 128});
            }
        }
    }
    if (!world_exit_ && !world_battle_zoom_ && !world.remaining && seed.parked_airship &&
        seed.state.flags.story[0x1b9] && held(seed.input.pressed, Button::a) &&
        world.x / 16 == seed.parked_airship->x && world.y / 16 == seed.parked_airship->y) {
        if (!seed.state.flags.story[0x1ba]) {
            begin_airship_liftoff(world);
            visible_ = render_scene(world_scene());
            return tick;
        }
        // World interaction starts a new event invocation, just like Tent.
        // The later field return must not inherit the parked idle loop or the
        // event that originally delivered the party to the world map.
        begin_world_event(pack, seed.events, world_assets_->airship_ground_event);
        world.dx = world.dy = 0;
        visible_ = render_scene(world_scene());
        return tick;
    }
    if (!world_exit_ && !world_battle_zoom_) {
        if (!world.remaining && held(seed.input.pressed, Button::start)) {
            world.minimap = !world.minimap;
            seed.world_minimap = world.minimap;
        }
        if (!world.remaining && held(seed.input.pressed, Button::x) &&
            !(seed.input.held & (button(Button::up) | button(Button::down) | button(Button::left) |
                                 button(Button::right)))) {
            menu_fade_ = 15;
            world.dx = world.dy = 0;
        } else if (advance_world_walk(*world_assets_, world, seed.input.held)) {
            if (apply_world_step_effects(seed.state.roster, seed.player.effects)) {
                world.poison_flash = FF6_ROM_VALUE_WORLD_POISON_MOSAIC_PARAMETERS_1;
                tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_WORLD_SESSION_EFFECTS_3, 128});
            }
        }
    }
    visible_ = render_scene(world_scene());
    return tick;
}
} // namespace ff6
