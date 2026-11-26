#include "ff6/field_session.hpp"
#include "rom-materials/battle_initial_refresh_level.macros.inc"
#include "rom-materials/audio_loading_choices.macros.inc"
#include "rom-materials/menu_save_success_sound.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include "rom-materials/field_transition_brightness.macros.inc"
#include "ff6/battle_display.hpp"
#include "ff6/battle_entry.hpp"
#include "ff6/battle_scene.hpp"
#include "ff6/battle_schedule.hpp"
#include "ff6/battle_submission.hpp"
#include "ff6/field_boot.hpp"
#include "ff6/field_party_switch.hpp"
#include "ff6/game_time.hpp"

namespace ff6 {
FieldSessionTick FieldSession::advance(const ContentPack& pack, Buttons buttons, std::size_t budget,
                                       bool audio_ready, Buttons second, bool music_finished,
                                       std::optional<std::uint8_t> music_position) {
    FieldSessionTick tick;
    if (stopped_)
        return tick;
    try {
        if (save_menu_)
            return advance_save_menu(pack, buttons);
        if (menu_fade_) {
            scene_.advance_external_timers();
            // Preserve the frozen port's two-level decrement. FadeOut and
            // UpdateBrightness use one level per field NMI; this native timing
            // difference is recorded, not presented as an extracted ROM rate.
            menu_fade_ = static_cast<std::uint8_t>(menu_fade_ > 2 ? menu_fade_ - 2 : 0);
            auto image = scene_.owned_.world ? world_scene() : *scene_.visible();
            image.intensity.assign(image.height, menu_fade_);
            visible_ = render_scene(image);
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            ++ticks_;
            tick.complete = tick.input_sampled = true;
            if (!menu_fade_)
                enter_field_menu(pack, buttons);
            return tick;
        }
        if (field_menu_) {
            const auto timer = scene_.owned_.timers[0];
            scene_.advance_external_timers();
            field_menu_->event_timer(
                timer.visible_menu_battle ? std::optional{scene_.owned_.timers[0].remaining}
                                          : std::nullopt,
                timer.terminate_external && !timer.remaining);
            const auto mono = field_menu_->values().preferences.mono;
            const auto update = field_menu_->advance(buttons);
            if (mono != field_menu_->values().preferences.mono)
                tick.audio.push_back(MonoPlayback{field_menu_->values().preferences.mono});
            for (auto sound : update.sounds)
                tick.audio.push_back(SystemSoundEffect{sound});
            for (auto sound : update.game_sounds)
                tick.audio.push_back(GameSoundEffect{sound, 128});
            visible_ = render_scene(field_menu_->scene());
            ++ticks_;
            tick.complete = tick.input_sampled = true;
            if (field_menu_->page() == FieldMenuPage::save) {
                const auto& v = field_menu_->values();
                saved_preferences_.reset();
                save_menu_.emplace(
                    pack, SaveMenuMode::save,
                    save_store_ ? save_store_->read() : std::array<SaveSlot, 3>{},
                    MenuWindowAppearance{v.window_skin, v.window_colors[v.window_skin]}, v.input,
                    buttons);
            } else if (field_menu_->complete())
                return_from_field_menu(pack);
            return tick;
        }
        if (final_order_menu_) {
            const auto update = final_order_menu_->advance(buttons);
            for (auto sound : update.sounds) tick.audio.push_back(SystemSoundEffect{sound});
            visible_ = render_scene(final_order_menu_->scene());
            tick.complete = tick.input_sampled = true; ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (final_order_menu_->complete()) {
                final_order_ = final_order_menu_->selection().order();
                return_from_event_menu(pack, false);
                final_order_menu_.reset();
            }
            return tick;
        }
        if (final_transition_) {
            sample_input(scene_.owned_.input, scene_.owned_.settings, InputContext::battle,
                         buttons, second, {scene_.owned_.menu.multiplayer,
                                           scene_.owned_.menu.second_player_characters, 0});
            tick.input_sampled = true;
            tick.audio = final_transition_->advance();
            visible_ = render_scene(final_transition_->scene());
            tick.complete = true; ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (final_transition_->complete()) {
                battle_menus_.emplace(pack, *battle_entrance_, scene_.owned_.menu, scene_.owned_.input);
                battle_menus_->connect_combat(*battle_formation_, *battle_preparation_, scene_.owned_.state.battle_variables,
                                             true, (scene_.owned_.state.roster.recruited & (1U << 11U)) != 0);
                battle_state_->time.previous_clock = battle_menus_->clock();
                final_transition_.reset();
            }
            return tick;
        }
        if (party_menu_) {
            const auto update = party_menu_->advance(pack, buttons);
            for (auto sound : update.sounds) tick.audio.push_back(SystemSoundEffect{sound});
            visible_ = render_scene(party_menu_->scene());
            tick.complete = true;
            tick.input_sampled = true;
            ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (party_menu_->complete()) {
                auto& state = scene_.owned_.state;
                state.roster = party_menu_->roster();
                for (unsigned id = 0; id < 16; ++id)
                    state.field.objects[id].membership = state.roster.membership[id];
                sort_field_objects(state);
                return_from_event_menu(pack, false);
                party_menu_.reset();
            }
            return tick;
        }
        if (colosseum_menu_) {
            scene_.advance_external_timers();
            const auto update = colosseum_menu_->advance(buttons);
            for (auto sound : update.sounds) tick.audio.push_back(SystemSoundEffect{sound});
            visible_ = render_scene(colosseum_menu_->scene());
            tick.complete = tick.input_sampled = true;
            ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (colosseum_menu_->complete()) {
                scene_.owned_.colosseum = colosseum_menu_->selection();
                scene_.owned_.state.flags.story[0x1ee] = scene_.owned_.colosseum.has_value();
                return_from_event_menu(pack, false);
                colosseum_menu_.reset();
            }
            return tick;
        }
        if (shop_menu_) {
            scene_.advance_external_timers();
            const auto update = shop_menu_->advance(buttons);
            for (auto sound : update.sounds)
                tick.audio.push_back(SystemSoundEffect{sound});
            for (auto sound : update.game_sounds)
                tick.audio.push_back(GameSoundEffect{sound, 128});
            visible_ = render_scene(shop_menu_->scene());
            tick.complete = tick.input_sampled = true;
            ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (shop_menu_->complete()) {
                scene_.owned_.state.inventory = shop_menu_->values().inventory;
                return_from_event_menu(pack, true);
                shop_menu_.reset();
            }
            return tick;
        }
        if (name_menu_) {
            scene_.advance_external_timers();
            sample_input(name_input_, scene_.owned_.settings, InputContext::menu, buttons, 0);
            for (auto sound : name_menu_->advance(*name_assets_, name_input_.pressed, name_input_.repeating))
                tick.audio.push_back(SystemSoundEffect{sound});
            MenuWindowAppearance appearance{scene_.owned_.window_skin, {}};
            if (scene_.owned_.window_colors) appearance.colors = (*scene_.owned_.window_colors)[appearance.skin];
            visible_ = render_scene(name_menu_->scene(*name_assets_, appearance, scene_.owned_.font_color));
            tick.complete = tick.input_sampled = true;
            ++ticks_;
            advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (name_menu_->phase() == NameMenuPhase::complete) return_from_name_menu(pack);
            return tick;
        }
        if (title_) {
            const auto result = title_->advance(held(buttons, Button::a), audio_ready);
            if (result.image_changed && title_->visible())
                visible_ = render_scene(*title_->visible());
            // Event effects/fades preceding the cutscene in the suspended
            // field visit must reach audio before its initial song command.
            if (result.interval)
                append_field_audio(tick.audio);
            tick.audio.insert(tick.audio.end(), result.audio.begin(), result.audio.end());
            tick.input_sampled = result.input_sampled;
            tick.complete = result.interval;
            if (result.interval)
                ++ticks_;
            if (title_->complete()) {
                last_title_skipped_ = title_->skipped();
                scene_.complete_cutscene(cutscene_request_, EventCutscene::title,
                                         *last_title_skipped_);
                title_.reset();
            }
            return tick;
        }
        if (credits_) {
            const auto result = credits_->advance(held(buttons, Button::a), audio_ready);
            if (result.image_changed && credits_->visible())
                visible_ = render_scene(*credits_->visible());
            if (result.interval)
                append_field_audio(tick.audio);
            tick.audio.insert(tick.audio.end(), result.audio.begin(), result.audio.end());
            tick.input_sampled = result.input_sampled;
            tick.complete = result.interval;
            if (result.interval)
                ++ticks_;
            if (credits_->complete()) {
                last_credits_skipped_ = credits_->skipped();
                scene_.complete_cutscene(cutscene_request_, EventCutscene::opening_credits,
                                         *last_credits_skipped_);
                credits_.reset();
            }
            return tick;
        }
        if (continent_rise_) {
            append_field_audio(tick.audio);
            if (continent_rise_->advance()) {
                visible_ = render_scene(continent_rise_->scene());
                ++ticks_;
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
                tick.complete = true;
            }
            if (continent_rise_->complete()) {
                visible_ = render_scene(continent_rise_->scene());
                scene_.complete_cutscene(cutscene_request_, EventCutscene::floating_continent, false);
                continent_rise_.reset();
            }
            return tick;
        }
        if (world_ruin_) {
            append_field_audio(tick.audio);
            if (world_ruin_->advance()) {
                visible_ = render_scene(world_ruin_->scene());
                ++ticks_;
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
                tick.complete = true;
            }
            if (world_ruin_->complete()) {
                visible_ = render_scene(world_ruin_->scene());
                scene_.complete_cutscene(cutscene_request_, EventCutscene::world_ruin, false);
                world_ruin_.reset();
            }
            return tick;
        }
        if (the_end_) {
            append_field_audio(tick.audio);
            const auto sounds = the_end_->advance(music_position, music_finished);
            tick.audio.insert(tick.audio.end(), sounds.begin(), sounds.end());
            visible_ = render_scene(the_end_->scene());
            ++ticks_;
            tick.complete = true;
            // BB jumps permanently into TheEnd; there is no field continuation
            // or game-time interrupt in this owner. Host close/reset still work.
            return tick;
        }
        if (ending_airship_) {
            append_field_audio(tick.audio);
            if (ending_airship_->advance()) {
                visible_ = render_scene(ending_airship_->scene());
                ++ticks_;
                tick.complete = true;
            }
            if (ending_airship_->complete()) {
                scene_.complete_cutscene(cutscene_request_, EventCutscene::ending_airship, false);
                ending_airship_.reset();
            }
            return tick;
        }
        if (ending_flight_) {
            append_field_audio(tick.audio);
            if (ending_flight_->advance()) {
                scene_.advance_external_timers();
                visible_ = render_scene(ending_flight_->scene());
                ++ticks_;
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
                tick.complete = true;
            }
            if (ending_flight_->complete()) {
                scene_.complete_cutscene(cutscene_request_, EventCutscene::ending, false);
                ending_flight_.reset();
            }
            return tick;
        }
        if (ending_characters_) {
            append_field_audio(tick.audio);
            if (ending_characters_->advance()) {
                scene_.advance_external_timers();
                visible_ = render_scene(ending_characters_->scene());
                ++ticks_;
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
                tick.complete = true;
            }
            if (ending_characters_->complete()) {
                scene_.complete_cutscene(cutscene_request_, EventCutscene::ending, false);
                ending_characters_.reset();
            }
            return tick;
        }
        if (ending_book_) {
            append_field_audio(tick.audio);
            if (ending_book_->advance()) {
                // MenuNMI keeps the shared menu/battle timer policy running
                // while BA owns display. Ordinary field actors stay suspended.
                scene_.advance_external_timers();
                visible_ = render_scene(ending_book_->scene());
                ++ticks_;
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
                tick.complete = true;
            }
            if (ending_book_->complete()) {
                scene_.complete_cutscene(cutscene_request_, EventCutscene::ending, false);
                ending_book_.reset();
            }
            return tick;
        }
        if (battle_menus_) {
            // Decide before any external timer/ATB/play-time mutation. The
            // preview consumes the same preceding input sample below.
            const bool active = !battle_menus_->pause_decision(audio_ready).skip_active_updates;
            if (active) {
                if (scene_.owned_.timers[0].visible_menu_battle)
                    battle_timer_display_.advance(scene_.owned_.timers[0].remaining);
                battle_state_->event_timer_expired = scene_.advance_external_timers();
            }
            tick.audio = battle_menus_->advance(*battle_state_, *battle_party_, buttons, second, audio_ready);
            if (active) {
                if (battle_menus_->battle_change()) change_battle(pack);
                if (battle_menus_->scripted_event_active()) battle_state_->time.previous_clock = battle_menus_->clock();
                else if (!battle_menus_->ending()) service_battle_time(
                    *battle_state_,
                    {battle_menus_->clock(), battle_menus_->menus().wait_for_menu(),
                     battle_menus_->escape_held() && !battle_state_->menus_disabled},
                    battle_submission_data_);
            }
            auto battle_picture = battle_menus_->visible();
            if (scene_.owned_.timers[0].visible_menu_battle)
                draw_battle_event_timer(battle_picture, scene_.assets_.timer_graphics,
                                        battle_timer_display_.frames, scene_.owned_.font_color);
            visible_ = render_scene(battle_picture);
            tick.complete = tick.input_sampled = true;
            ++ticks_;
            if (active) advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (!active) return tick;
            notice_ = battle_menus_->menus().notice();
            if (single_action_study_ && battle_menus_->action_complete()) {
                stopped_ = true;
                notice_ = battle_menus_->animation_notice();
            }
            if (battle_menus_->victory_ready() && next_final_battle(battle_formation_->selection.formation)) {
                advance_final_tier(pack);
                return tick;
            }
            if (battle_menus_->victory_ready() && !rewards_applied_) {
                const auto preparation = load_battle_preparation_data(pack);
                auto rewards = battle_state_->colosseum
                    ? award_colosseum_victory(*battle_state_, *battle_party_, scene_.owned_.state.roster,
                                               scene_.owned_.state.inventory, scene_.owned_.colosseum->prize)
                    : award_battle_victory(*battle_state_, *battle_party_, *battle_preparation_,
                    preparation.monsters, scene_.assets_.party, preparation.random,
                    scene_.owned_.state.roster, scene_.owned_.state.inventory,
                    battle_entry_->request.on_veldt, load_battle_reward_data(pack),
                    battle_formation_->selection.formation,
                    (scene_.owned_.state.battle_variables[8] & 8U) != 0,
                    battle_menus_->cursed_shield_battles());
                battle_state_->menus_disabled = true;
                battle_menus_->begin_victory(std::move(rewards));
                rewards_applied_ = true;
            }
            if (battle_menus_->victory_complete() || battle_menus_->escape_complete() ||
                battle_menus_->defeat_complete() ||
                battle_menus_->scripted_return_ready() ||
                battle_menus_->leap_complete())
                return_from_battle(pack);
            return tick;
        }
        if (battle_entrance_) {
            battle_entrance_->update_gauges(*battle_party_, *battle_state_);
            const auto result = battle_entrance_->advance(audio_ready, budget);
            if (result.battle_time_requested)
                service_battle_time(*battle_state_, {wrap8(battle_entrance_->actors().clock)},
                                    battle_submission_data_);
            if (result.image_changed && battle_entrance_->visible())
                visible_ = render_scene(*battle_entrance_->visible());
            tick.audio = result.audio;
            tick.complete = result.interval;
            if (result.interval) {
                sample_input(scene_.owned_.input, scene_.owned_.settings, InputContext::battle,
                             buttons, second, {scene_.owned_.menu.multiplayer,
                                               scene_.owned_.menu.second_player_characters, 0});
                tick.input_sampled = true;
                scene_.advance_external_timers();
                ++ticks_;
            }
            if (result.scene_updated)
                advance_game_time(scene_.owned_.menu.time, scene_.owned_.menu.time_frames);
            if (battle_entrance_->complete()) {
                battle_menus_.emplace(pack, *battle_entrance_, scene_.owned_.menu,
                                      scene_.owned_.input);
                if (!single_action_study_)
                    battle_menus_->connect_combat(*battle_formation_, *battle_preparation_,
                                                   scene_.owned_.state.battle_variables,
                                                   battle_entry_->request.continue_music,
                                                   (scene_.owned_.state.roster.recruited & (1U << 11U)) != 0);
            }
            return tick;
        }
        if (mine_cart_) return advance_mine_cart(pack);
        if (scene_.owned_.world) return advance_world(pack, buttons);
        if (loading_) {
            if (scene_.transition().pending->dispatch_world) {
                enter_world(pack, *scene_.transition().pending);
                loading_ = pending_tick_ = false;
                ++maps_loaded_;
                return advance_world(pack, buttons);
            }
            // Construct first: a rejected destination retains the pending old
            // scene and the last visible image for an actionable diagnostic.
            auto destination = scene_.load_destination(pack);
            scene_ = std::move(destination);
            observed_frames_ = 0;
            loading_ = false;
            ++maps_loaded_;
            return tick;
        }
        if (battle_transition_) {
            auto next = *battle_transition_;
            const auto size = next.advance();
            scene_.publish_battle_transition(buttons, size, !next.complete());
            battle_transition_ = next;
            observed_frames_ = scene_.frames();
            visible_ = render_scene(*scene_.visible());
            ++ticks_;
            tick.complete = tick.input_sampled = true;
            if (next.complete())
                enter_battle(pack);
            return tick;
        }
        if (!pending_tick_ && !scene_.state().startup) {
            sampled_ = buttons;
            sampled_second_ = second;
            pending_tick_ = true;
            tick.input_sampled = true;
        }
        boundary_ =
            scene_.advance(sampled_, sampled_second_, budget, audio_ready, music_finished,
                           music_position).boundary;
        if (boundary_ == FieldLoopBoundary::budget || boundary_ == FieldLoopBoundary::startup_visit)
            return tick;
        if (boundary_ == FieldLoopBoundary::map_load) {
            loading_ = true;
            if (scene_.frames() == observed_frames_)
                pending_tick_ = false;
        } else if (boundary_ != FieldLoopBoundary::frame &&
                   boundary_ != FieldLoopBoundary::audio_wait) {
            stopped_ = true;
            switch (boundary_) {
            case FieldLoopBoundary::encounter:
                notice_ = "Encounter handling is not implemented yet";
                break;
            case FieldLoopBoundary::battle:
                battle_transition_.emplace(scene_.state().battle->mosaic);
                stopped_ = false;
                break;
            case FieldLoopBoundary::menu_or_party:
                if (scene_.menu_requested()) {
                    menu_fade_ = FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0 >> 4;
                    stopped_ = false;
                    append_field_audio(tick.audio);
                    notice_.clear();
                } else {
                    switch_field_party(scene_.owned_);
                    scene_.phase_ = FieldLoop::Phase::frame_begin;
                    stopped_ = false;
                    notice_.clear();
                }
                break;
            case FieldLoopBoundary::event_request:
                if (const auto* cutscene = std::get_if<EventRunCutscene>(
                        &scene_.state().events.pending_request()->operation);
                    cutscene && cutscene->scene == EventCutscene::title) {
                    // The field's cold profile retains a zero cutscene RNG seed.
                    // The unused field outline routine is its only other writer.
                    title_.emplace(load_title_animation_assets(pack), 0);
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::opening_credits) {
                    credits_.emplace(load_opening_assets(pack), 0);
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::floating_continent) {
                    continent_rise_.emplace(load_floating_continent_assets(pack));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::world_ruin) {
                    world_ruin_.emplace(load_world_ruin_assets(pack));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::ending_airship) {
                    ending_airship_.emplace(load_ending_airship_assets(pack));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::the_end) {
                    the_end_.emplace(load_the_end_assets(pack));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::ending) {
                    if (is_ending_book_segment(cutscene->segment))
                        ending_book_.emplace(load_ending_book_assets(pack), ending_book_segment(cutscene->segment));
                    else if (is_ending_flight_segment(cutscene->segment))
                        ending_flight_.emplace(load_ending_flight_assets(pack,
                            ending_flight_segment(cutscene->segment)));
                    else if (is_ending_character_segment(cutscene->segment))
                        ending_characters_.emplace(load_ending_character_assets(pack,
                            ending_character_segment(cutscene->segment), scene_.owned_.state.roster));
                    else
                        throw std::invalid_argument("Unimplemented ending cinematic segment " +
                                                    std::to_string(cutscene->segment));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                } else if (cutscene && cutscene->scene == EventCutscene::mine_cart) {
                    mine_cart_.emplace(load_mine_cart_assets(pack));
                    cutscene_request_ = scene_.state().events.pending_request()->identity;
                    append_field_audio(tick.audio);
                    tick.audio.push_back(GameSong{mine_cart_->song(), 255, false});
                    stopped_ = false;
                } else if (std::holds_alternative<EventFinalOrder>(scene_.state().events.pending_request()->operation)) {
                    save_field_membership(scene_.owned_.state);
                    FieldMenuValues values;
                    values.roster = scene_.owned_.state.roster;
                    values.input = scene_.owned_.settings;
                    values.window_colors = scene_.owned_.window_colors.value_or(scene_.assets_.dialogue.colors);
                    values.window_skin = scene_.owned_.window_skin;
                    values.font_color = scene_.owned_.font_color;
                    final_order_menu_.emplace(pack, std::move(values), buttons);
                    append_field_audio(tick.audio);
                    stopped_ = false; notice_.clear();
                } else if (const auto* selection = std::get_if<EventPartyMenu>(
                               &scene_.state().events.pending_request()->operation)) {
                    save_field_membership(scene_.owned_.state);
                    FieldMenuValues values;
                    values.roster = scene_.owned_.state.roster;
                    values.initialized_characters = scene_.owned_.state.flags.initialized_characters();
                    values.inventory = scene_.owned_.state.inventory;
                    values.preferences = scene_.owned_.menu;
                    values.input = scene_.owned_.settings;
                    values.window_colors =
                        scene_.owned_.window_colors.value_or(scene_.assets_.dialogue.colors);
                    values.window_skin = scene_.owned_.window_skin;
                    values.font_color = scene_.owned_.font_color;
                    party_menu_.emplace(pack, std::move(values), selection->parties,
                                        selection->clear, selection->forced, buttons);
                    append_field_audio(tick.audio);
                    stopped_ = false;
                    notice_.clear();
                } else if (std::holds_alternative<EventColosseumMenu>(scene_.state().events.pending_request()->operation)) {
                    save_field_membership(scene_.owned_.state);
                    FieldMenuValues values;
                    values.roster = scene_.owned_.state.roster;
                    values.initialized_characters = scene_.owned_.state.flags.initialized_characters();
                    values.inventory = scene_.owned_.state.inventory;
                    values.input = scene_.owned_.settings;
                    values.window_colors = scene_.owned_.window_colors.value_or(scene_.assets_.dialogue.colors);
                    values.window_skin = scene_.owned_.window_skin;
                    values.font_color = scene_.owned_.font_color;
                    colosseum_menu_.emplace(pack, std::move(values), scene_.owned_.state.flags.story[0x1ef], buttons);
                    append_field_audio(tick.audio);
                    stopped_ = false;
                    notice_.clear();
                } else if (const auto* shop = std::get_if<EventShop>(
                               &scene_.state().events.pending_request()->operation)) {
                    save_field_membership(scene_.owned_.state);
                    FieldMenuValues values;
                    values.roster = scene_.owned_.state.roster;
                    values.initialized_characters = scene_.owned_.state.flags.initialized_characters();
                    values.inventory = scene_.owned_.state.inventory;
                    values.input = scene_.owned_.settings;
                    values.window_colors =
                        scene_.owned_.window_colors.value_or(scene_.assets_.dialogue.colors);
                    values.window_skin = scene_.owned_.window_skin;
                    values.font_color = scene_.owned_.font_color;
                    const auto leader = *scene_.owned_.state.field.update_order[0];
                    shop_menu_.emplace(pack, shop->identity,
                                       scene_.owned_.state.field.objects[leader].graphics,
                                       std::move(values), buttons);
                    append_field_audio(tick.audio);
                    stopped_ = false;
                    notice_.clear();
                } else if (const auto* naming = std::get_if<EventNameMenu>(
                               &scene_.state().events.pending_request()->operation)) {
                    named_character_ = resolve_field_object_reference(scene_.owned_.state, naming->character);
                    if (named_character_ >= scene_.owned_.state.roster.characters.size())
                        throw std::invalid_argument("Naming requires a persistent character");
                    const auto& character = scene_.owned_.state.roster.characters[named_character_];
                    name_assets_ = load_name_menu(pack);
                    name_menu_.emplace(character.name, character.graphics);
                    name_input_ = {};
                    name_input_.previous = buttons;
                    append_field_audio(tick.audio);
                    stopped_ = false;
                    notice_.clear();
                } else if (std::holds_alternative<EventRestartGame>(
                               scene_.state().events.pending_request()->operation)) {
                    return restart_after_game_over(pack);
                } else if (std::holds_alternative<EventLoadMenu>(
                               scene_.state().events.pending_request()->operation)) {
                    const auto slots =
                        save_store_ ? save_store_->read() : std::array<SaveSlot, 3>{};
                    if (std::ranges::any_of(slots, [](const auto& slot) {
                            return slot.game || !slot.error.empty();
                        })) {
                        save_menu_.emplace(pack, SaveMenuMode::load, slots, MenuWindowAppearance{},
                                           InputSettings{}, buttons);
                        stopped_ = false;
                        notice_.clear();
                    } else if (scene_.complete_load_without_save(storage_)) {
                        ++new_game_entries_;
                        stopped_ = false;
                    } else
                        notice_ = "Saved-game selection is not implemented yet";
                } else
                    notice_ = "This event operation is not implemented yet";
                break;
            default:
                throw std::logic_error("Unexpected field session boundary");
            }
        }
        if (scene_.frames() != observed_frames_ || boundary_ == FieldLoopBoundary::audio_wait) {
            if (scene_.frames() != observed_frames_) {
                if (scene_.frames() != observed_frames_ + 1 || !scene_.visible())
                    throw std::logic_error("Field session skipped a visible publication");
                visible_ = render_scene(*scene_.visible());
            } else if (scene_.state().startup) {
                // LoadMap's startup has a blank display and no field interrupt
                // work. Continue audio without fabricating a gameplay visit.
                RasterScene blank;
                blank.intensity.resize(blank.height);
                visible_ = render_scene(blank);
            }
            append_field_audio(tick.audio);
            observed_frames_ = scene_.frames();
            ++ticks_;
            pending_tick_ = false;
            tick.complete = true;
        }
        if (battle_transition_) {
            // Drain the preceding event intentions before the transition effect.
            // This branch runs once at handoff, never on a budget continuation.
            append_field_audio(tick.audio);
            if (scene_.state().battle->sound)
                tick.audio.push_back(GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_10, 128});
            if (battle_transition_->complete())
                enter_battle(pack);
        }
    } catch (const std::exception& error) {
        stopped_ = true;
        notice_ = error.what();
    }
    return tick;
}
SavedGame FieldSession::menu_save_snapshot() const {
    auto seed = scene_.state();
    const auto& v = field_menu_->values();
    seed.state.roster = v.roster;
    seed.state.inventory = v.inventory;
    seed.menu = v.preferences;
    seed.menu.save_count = wrap16(unsigned{seed.menu.save_count} + 1);
    seed.menu.saved_time = seed.menu.time;
    seed.settings = v.input;
    seed.window_colors = v.window_colors;
    seed.window_skin = v.window_skin;
    seed.font_color = v.font_color;
    return capture_saved_game(seed);
}
FieldSessionTick FieldSession::advance_save_menu(const ContentPack& pack, Buttons buttons) {
    FieldSessionTick tick;
    scene_.advance_external_timers();
    if (field_menu_)
        field_menu_->advance(0);
    for (auto sound : save_menu_->advance(buttons))
        tick.audio.push_back(SystemSoundEffect{sound});
    if (save_menu_->phase() == SaveMenuPhase::request) {
        try {
            if (save_menu_->mode() == SaveMenuMode::save) {
                if (!save_store_)
                    throw std::runtime_error("No save folder has been configured");
                const auto game = menu_save_snapshot();
                save_store_->write(save_menu_->choice()->slot, game);
                restart_slot_ = save_menu_->choice()->slot;
                saved_preferences_ = game.menu;
                tick.audio.push_back(SystemSoundEffect{FF6_ROM_VALUE_MENU_SAVE_SUCCESS_SOUND_0});
            } else if (!save_menu_->choice()->new_game) {
                loaded_scene_.reset(
                    new FieldLoop(restore_saved_game(pack, save_menu_->selected_game())));
                restart_slot_ = save_menu_->choice()->slot;
            }
            save_menu_->finish_request();
        } catch (const std::exception& error) {
            save_menu_->finish_request(error.what());
            tick.audio.push_back(SystemSoundEffect{FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0});
        }
    }
    notice_ = save_menu_->notice();
    visible_ = render_scene(save_menu_->scene());
    ++ticks_;
    tick.complete = tick.input_sampled = true;
    if (save_menu_->phase() == SaveMenuPhase::complete) {
        if (save_menu_->mode() == SaveMenuMode::save)
            field_menu_->finish_save(saved_preferences_);
        else if (save_menu_->choice()->new_game) {
            restart_slot_.reset();
            OriginalSram empty;
            if (!scene_.complete_load_without_save(empty))
                throw std::logic_error("New game failed");
            ++new_game_entries_;
        } else {
            scene_ = *loaded_scene_;
            world_assets_.reset();
            world_exit_.reset();
            world_script_.reset();
            world_battle_zoom_ = 0;
            loaded_scene_.reset();
            observed_frames_ = 0;
            pending_tick_ = loading_ = false;
            boundary_ = FieldLoopBoundary::frame;
            // Loading a slot starts its field music rather than retaining the
            // title/credits song or the previous session's mono setting.
            tick.audio.push_back(MonoPlayback{scene_.state().menu.mono});
            tick.audio.push_back(GameSong{scene_.state().map_song, 255});
            ++maps_loaded_;
        }
        save_menu_.reset();
        notice_.clear();
        stopped_ = false;
    }
    return tick;
}
FieldSessionTick FieldSession::restart_after_game_over(const ContentPack& pack) {
    // C0/RestartGame uses the last saved/loaded slot without a selection menu.
    // An absent or invalid slot goes through Reset. Experience survives only a
    // valid reload; no dead actors or in-flight battle/event work are retained.
    std::optional<SavedGame> saved;
    if (restart_slot_ && save_store_)
        saved = save_store_->read().at(*restart_slot_).game;
    if (saved) {
        auto restored = retain_game_over_progress(std::move(*saved), scene_.owned_.state.roster,
                                                  scene_.assets_.party);
        scene_ = restore_saved_game(pack, restored);
    } else {
        scene_ = std::move(make_initial_field_session(pack).scene_);
        restart_slot_.reset();
        storage_ = {};
    }
    world_assets_.reset();
    world_exit_.reset();
    world_script_.reset();
    world_battle_zoom_ = 0;
    observed_frames_ = 0;
    pending_tick_ = loading_ = stopped_ = false;
    boundary_ = FieldLoopBoundary::frame;
    notice_.clear();
    ++game_over_restarts_;
    ++maps_loaded_;
    ++ticks_;
    FieldSessionTick tick;
    tick.complete = tick.input_sampled = true;
    tick.audio.push_back(MonoPlayback{scene_.state().menu.mono});
    tick.audio.push_back(GameSong{scene_.state().map_song, 255});
    return tick;
}
void FieldSession::enter_field_menu(const ContentPack& pack, Buttons buttons) {
    auto& retained = scene_.owned_;
    save_field_membership(retained.state);
    FieldMenuValues values;
    values.roster = retained.state.roster;
    values.initialized_characters = retained.state.flags.initialized_characters();
    values.inventory = retained.state.inventory;
    values.preferences = retained.menu;
    values.input = retained.settings;
    values.window_colors = retained.window_colors.value_or(scene_.assets_.dialogue.colors);
    values.window_skin = retained.window_skin;
    values.font_color = retained.font_color;
    values.steps = retained.player.effects.steps;
    for (unsigned i = 0; i < values.rare_items.size(); ++i)
        values.rare_items[i] = retained.state.flags.story[0x1d0 + i];
    // WorldMain replaces $0201 with $80: rest/save are allowed, Warp is not.
    // The dormant field map's permissions must not leak into the world menu.
    values.permissions = {retained.world.has_value() || retained.state.flags.story[0x1bf],
                          !retained.world && (scene_.assets_.menu_permissions & 2U) != 0};
    field_menu_.emplace(pack, std::move(values), buttons);
}
void FieldSession::return_from_field_menu(const ContentPack& pack) {
    auto retained = scene_.owned_;
    const auto& values = field_menu_->values();
    retained.state.roster = values.roster;
    retained.state.inventory = values.inventory;
    retained.menu = values.preferences;
    retained.settings = values.input;
    retained.window_colors = values.window_colors;
    retained.window_skin = values.window_skin;
    retained.font_color = values.font_color;
    for (unsigned i = 0; i < 16; ++i)
        retained.state.field.objects[i].membership = retained.state.roster.membership[i];
    if (retained.world) {
        const auto handoff = field_menu_->item_handoff();
        if (handoff && *handoff != FieldItemResult::tent)
            throw std::logic_error("World menu returned a prohibited item handoff");
        if (handoff) {
            // WorldMain's Tent menu return selects WorldTent_ext. Its authored
            // world commands enter the ordinary field rest map, whose tile
            // event owns healing, tent poses, song wait/resume and parent return.
            const auto world_tent_event =
                read24(pack.get("ptr_table_eeb260_world_and_vehicle_events"), 12);
            begin_world_event(pack, retained.events, world_tent_event);
        }
        scene_.owned_ = std::move(retained);
        refresh_field_equipment(scene_.owned_.state, scene_.assets_.party, scene_.assets_.equipment);
        select_field_party_leader(scene_.owned_.state);
        field_menu_.reset();
        reload_world(pack);
        return;
    }
    const auto leader = retained.state.field.update_order[0];
    if (!leader)
        throw std::logic_error("Menu return has no field leader");
    FieldMapLoadPlan plan;
    plan.map = retained.state.field.current_map;
    plan.arrival = FieldMapArrival{
        {retained.actions.player_tile_position[0], retained.actions.player_tile_position[1]},
        static_cast<FieldFacing>(retained.state.field.objects[*leader].facing),
        false,
        true};
    plan.characters = save_field_character_locations(retained.state.field);
    plan.parent = retained.state.field.parent_map;
    plan.run_startup = plan.keep_dimensions = plan.fade_in = true;
    plan.saved_level = retained.actions.player_terrain.level;
    const auto scroll = retained.actions.scroll;
    retained.transition.pending = plan;
    retained.transition.fade = {};
    auto assets = load_field_map_entry_assets(pack, plan.map, scene_.display_.map());
    std::copy_n(scene_.assets_.objects.special_tiles.begin(), 32,
                assets.scene.objects.special_tiles.begin());
    auto entry = prepare_field_map_entry(std::move(assets), std::move(retained), true);
    entry.seed.actions.scroll = scroll;
    if (const auto handoff = field_menu_->item_handoff()) {
        // Named entries in the extracted event-language ABI. The original
        // script owns recovery, tent poses, song suspension and map travel.
        if (*handoff != FieldItemResult::tent && *handoff != FieldItemResult::warp)
            throw std::logic_error("Unexpected field item continuation");
        entry.seed.events.begin_object_event(*handoff == FieldItemResult::tent ? 0x34 : 0x39);
        auto& actor = entry.seed.state.field.objects[*leader];
        actor.saved_movement = actor.movement;
        actor.movement = {4, 0};
    }
    scene_ = FieldLoop(std::move(entry.assets), std::move(entry.seed));
    observed_frames_ = 0;
    pending_tick_ = loading_ = false;
    field_menu_.reset();
    boundary_ = FieldLoopBoundary::frame;
    notice_.clear();
}
void FieldSession::return_from_name_menu(const ContentPack& pack) {
    scene_.owned_.state.roster.characters[named_character_].name = name_menu_->name();
    return_from_event_menu(pack, false);
    name_menu_.reset();
    name_assets_.reset();
}
void FieldSession::return_from_event_menu(const ContentPack& pack, bool fade_in) {
    auto retained = scene_.owned_;
    const auto request = retained.events.pending_request();
    if (!request || (!std::holds_alternative<EventNameMenu>(request->operation) &&
                     !std::holds_alternative<EventShop>(request->operation) &&
                     !std::holds_alternative<EventColosseumMenu>(request->operation) &&
                     !std::holds_alternative<EventPartyMenu>(request->operation) &&
                     !std::holds_alternative<EventFinalOrder>(request->operation)))
        throw std::logic_error("Menu lost its field event continuation");
    save_field_membership(retained.state);
    FieldFlagAccess flags(retained.state);
    retained.events.complete_request(request->identity, flags);
    if (retained.events.advance(flags, {}) != EventYield::frame_wait)
        throw std::logic_error("Event menu return lost its event-visit boundary");
    const auto leader = retained.state.field.update_order[0];
    if (!leader)
        throw std::logic_error("Event menu return has no field leader");
    const auto& actor = retained.state.field.objects[*leader];
    FieldMapLoadPlan plan;
    plan.map = retained.state.field.current_map;
    plan.arrival = FieldMapArrival{{retained.actions.player_tile_position[0], retained.actions.player_tile_position[1]},
                                  static_cast<FieldFacing>(actor.facing), false, true};
    plan.characters = save_field_character_locations(retained.state.field);
    plan.parent = retained.state.field.parent_map;
    // Event 98 overrides OpenMenu's defaults with E0: same-map startup,
    // retained dimensions and no automatic fade. The event owns the reveal.
    plan.run_startup = true;
    plan.keep_dimensions = !fade_in && !std::holds_alternative<EventPartyMenu>(request->operation) &&
                           !std::holds_alternative<EventColosseumMenu>(request->operation);
    plan.fade_in = fade_in;
    plan.saved_level = retained.actions.player_terrain.level;
    const auto scroll = retained.actions.scroll;
    retained.transition.pending = plan;
    retained.transition.fade = {};
    auto assets = load_field_map_entry_assets(pack, plan.map, scene_.display_.map());
    std::copy_n(scene_.assets_.objects.special_tiles.begin(), 32, assets.scene.objects.special_tiles.begin());
    auto entry = prepare_field_map_entry(std::move(assets), std::move(retained), true);
    entry.seed.actions.scroll = scroll;
    scene_ = FieldLoop(std::move(entry.assets), std::move(entry.seed));
    observed_frames_ = 0;
    pending_tick_ = loading_ = false;
    boundary_ = FieldLoopBoundary::frame;
    notice_.clear();
}
void FieldSession::return_from_battle(const ContentPack& pack) {
    auto retained = scene_.owned_;
    retained.state.terra_magic_pending = battle_state_->terra_magic_pending;
    retained.state.battle_variables = battle_menus_->global_script_variables();
    if (const auto target = battle_preparation_->monsters.doom_gaze_target) {
        // C2/49C4 retains HP on every ending, including Doom Gaze's own Escape.
        // Keep the existing save field and its original global-variable alias
        // consistent. The death script, not HP alone, decides Bahamut eligibility.
        const auto hp = battle_state_->stats[*target + 4U].hp.current;
        retained.saved_doom_gaze_hp = hp;
        retained.state.battle_variables[10] = wrap8(hp);
        retained.state.battle_variables[11] = wrap8(hp >> 8U);
    }
    remember_veldt_formation(retained.state.encountered_formations, *battle_formation_);
    if (battle_state_->end_event == 4 || battle_state_->end_event == 8 || battle_state_->end_event == 10 ||
        battle_state_->end_event == 2 ||
        battle_menus_->defeat_complete())
        retain_battle_party(*battle_state_, *battle_party_, retained.state.roster, retained.state.inventory);
    if (battle_state_->colosseum) {
        if (!retained.colosseum) throw std::logic_error("Coliseum return lost its wager");
        // Sneeze/escape clears the wager in C2/48E0; other endings pay it,
        // after the victory prize has entered the shared inventory.
        if (battle_state_->end_event != 2)
            consume_colosseum_wager(retained.state.inventory, retained.colosseum->wager);
        retained.colosseum.reset();
    }
    if (battle_state_->end_event == 4 || battle_state_->end_event == 10)
        learn_battle_rages(retained.state.roster.abilities, battle_formation_->formation);
    // ExecBattle (C0/C13E) calls PopCharFlags before returning to field events.
    // Publish battle recruitment/removal for every return path, including a
    // retained-map reload, or the next party update restores stale membership.
    for (unsigned i = 0; i < retained.state.roster.membership.size(); ++i)
        retained.state.field.objects[i].membership = retained.state.roster.membership[i];
    if (mine_cart_) {
        retained.state.last_battle_lost = battle_menus_->defeat_complete();
        retained.battle.reset();
        if (!retained.state.last_battle_lost) {
            scene_.owned_ = std::move(retained);
            mine_cart_->resume_after_battle();
            // Re-init requests the ride song, including after the boss's song.
            scene_.audio_.push_back(SongLoad{mine_cart_->song(), 255, false});
        } else {
            // World PartyDefeated abandons the suspended cinematic and enters
            // the common field defeat event. It must not resume Cid's escape.
            FieldMapLoadPlan plan;
            plan.map = 3;
            plan.arrival = FieldMapArrival{{0, 0}};
            plan.characters = save_field_character_locations(retained.state.field);
            plan.parent = retained.state.field.parent_map;
            plan.fade_in = false;
            retained.events = FieldEventRunner(field_event_script(pack), 0x2e);
            retained.transition.pending = plan;
            auto assets = load_field_map_entry_assets(pack, plan.map, scene_.display_.map());
            auto entry = prepare_field_map_entry(std::move(assets), std::move(retained));
            scene_ = FieldLoop(std::move(entry.assets), std::move(entry.seed));
            mine_cart_.reset();
            observed_frames_ = 0;
            pending_tick_ = loading_ = false;
            ++maps_loaded_;
        }
        clear_battle();
        notice_.clear();
        return;
    }
    if (retained.world) {
        if (battle_menus_->defeat_complete()) {
            doom_gaze_encounter_ = false;
            // World PartyDefeated enters the blank field map with the common
            // defeat event. That event owns the music, message and restart;
            // loading the saved slot directly would skip that presentation.
            constexpr std::uint32_t party_defeated_event = 0x2e;
            FieldMapLoadPlan plan;
            plan.map = 3;
            plan.arrival = FieldMapArrival{{0, 0}};
            plan.characters = save_field_character_locations(retained.state.field);
            plan.parent = retained.state.field.parent_map;
            plan.fade_in = false;
            retained.events = FieldEventRunner(field_event_script(pack), party_defeated_event);
            retained.state.last_battle_lost = true;
            retained.battle.reset();
            scene_.owned_ = std::move(retained);
            world_exit_ = plan;
            world_script_.reset();
            world_battle_zoom_ = 0;
            clear_battle();
            return;
        }
        refresh_field_equipment(retained.state, scene_.assets_.party, scene_.assets_.equipment);
        select_field_party_leader(retained.state);
        retained.battle.reset();
        scene_.owned_ = std::move(retained);
        clear_battle();
        // EE/042B reloads the world with VehicleEvent_05 after complete Engulf.
        // Let the authored world destination set the cave's parent/entry flags.
        // Persistent field variables start at AI variable four: result 12
        // is field byte eight (the authored battle switch $47).
        if (scene_.owned_.state.battle_variables[8] & 0x80U)
            begin_world_event(pack, scene_.owned_.events, world_assets_->gogo_cave_event);
        else if (doom_gaze_encounter_ && (scene_.owned_.state.battle_variables[9] & 1U))
            begin_world_event(pack, scene_.owned_.events, world_assets_->doom_gaze_event);
        doom_gaze_encounter_ = false;
        reload_world(pack);
        return;
    }
    const auto& back = battle_entry_->request.field_return;
    FieldMapLoadPlan plan;
    plan.map = retained.state.field.current_map;
    plan.arrival = FieldMapArrival{back.party, static_cast<FieldFacing>(battle_entry_->facing), false, true};
    plan.characters = save_field_character_locations(retained.state.field);
    plan.parent = retained.state.field.parent_map;
    plan.run_startup = back.run_startup;
    plan.fade_in = back.fade_in;
    plan.keep_dimensions = back.keep_dimensions;
    plan.saved_level = battle_entry_->terrain_level;
    retained.battle.reset();
    retained.state.last_battle_lost = battle_menus_->defeat_complete();
    retained.transition.pending = plan;
    retained.transition.fade = {};
    auto assets = load_field_map_entry_assets(pack, plan.map, scene_.display_.map());
    std::copy_n(scene_.assets_.objects.special_tiles.begin(), 32,
                assets.scene.objects.special_tiles.begin());
    auto entry = prepare_field_map_entry(std::move(assets), std::move(retained), true);
    entry.seed.actions.scroll = initialize_field_scroll(entry.assets.map.definition,
        entry.assets.map.parallax, back.scroll.x, back.scroll.y);
    // Keep the field song identity: the story override can request that exact
    // song after return. GameSong duplicate suppression belongs to the audio
    // owner, whose current song already reflects the intervening battle.
    scene_ = FieldLoop(std::move(entry.assets), std::move(entry.seed));
    observed_frames_ = 0;
    pending_tick_ = loading_ = false;
    boundary_ = FieldLoopBoundary::frame;
    notice_.clear();
    clear_battle();
    ++maps_loaded_;
}
void FieldSession::clear_battle() {
    final_transition_.reset();
    final_order_.reset();
    battle_menus_.reset();
    battle_entrance_.reset();
    battle_transition_.reset();
    battle_state_.reset();
    battle_party_.reset();
    battle_preparation_.reset();
    battle_status_.reset();
    battle_formation_.reset();
    battle_entry_.reset();
}
void FieldSession::enter_battle(const ContentPack& pack) {
    const auto data = load_battle_formation_data(pack);
    auto random = BattleRandom::from_game_clock(scene_.owned_.menu.time_frames);
    const auto selection = select_battle_formation(data, scene_.owned_.battle->formation,
                                                   scene_.owned_.battle_conditions, {}, random);
    // A fresh field battle clears the retained combat identity. Reinforcements
    // will use the same formation owner with their retained battle identity.
    const BattleFormationEntry formation{selection, random, data.formations[selection.formation]};
    // Those callers need their real selection UI/state; do not fabricate a
    // final lineup or colosseum participant for the generic field entry.
    if (selection.battle_identity == 0x1d7 && !final_order_)
        throw std::runtime_error("Final battle requires the event-selected lineup");
    const bool colosseum = selection.battle_identity >= 574;
    if (colosseum && !scene_.owned_.colosseum)
        throw std::logic_error("Coliseum entry lost its wager selection");
    const auto preparation_data = load_battle_preparation_data(pack);
    auto next = scene_.owned_;
    const auto entry = prepare_field_battle_entry(next);
    const auto party =
        initialize_battle_party(preparation_data.party, next.state.roster, formation,
                                {.final_lineup = selection.battle_identity == 0x1d7,
                                 .gau_may_return = entry.request.on_veldt,
                                 .retained = {},
                                 .final_order = final_order_.value_or(std::array<std::optional<std::uint8_t>, 12>{}),
                                 .colosseum_actor = colosseum ? std::optional{BattleActorSelection{next.colosseum->actor}} : std::nullopt});
    auto command_rules = next.battle_command_rules;
    command_rules.on_veldt = entry.request.on_veldt;
    auto preparation =
        prepare_battle(preparation_data, formation, party, scene_.assets_.party,
                       scene_.assets_.equipment, next.state.roster, next.state.inventory,
                       {next.menu, command_rules, next.saved_doom_gaze_hp,
                        colosseum ? std::optional{next.colosseum->monster} : std::nullopt});
    auto status = initialize_battle_status(preparation, party);
    auto battle = initialize_battle_state(preparation, party, status);
    battle.terra_magic_pending = next.state.terra_magic_pending;
    battle.field_steps = next.player.effects.steps;
    battle.morph.saved_gauge = next.state.roster.morph_gauge;
    battle.morph.permanent = next.battle_command_rules.permanent_morph;
    battle.morph.double_duration = next.battle_command_rules.double_morph_duration;
    resolve_battle_status_consequences(battle);
    // InitBattle supplies the ROM-defined level for the initial condemned refresh.
    refresh_battle_actions(battle, preparation_data.commands, preparation_data.random, FF6_ROM_VALUE_BATTLE_INITIAL_REFRESH_LEVEL_0);
    next.state.roster.morph_gauge = battle.morph.saved_gauge;
    initialize_battle_entry(battle, formation.formation, preparation.characters.party_relic_effects,
                            load_battle_approach_weights(pack), preparation_data.random);
    prepare_battle_presentation(battle, formation.formation);
    scene_.owned_ = std::move(next);
    battle_entry_ = entry;
    battle_formation_ = formation;
    battle_party_ = party;
    battle_preparation_ = std::move(preparation);
    battle_status_ = std::move(status);
    battle_state_ = std::move(battle);
    battle_timer_display_ = {};
    battle_submission_data_ = load_battle_submission_data(pack);
    RasterScene blank;
    blank.intensity.resize(blank.height);
    visible_ = render_scene(blank);
    stopped_ = false;
    rewards_applied_ = false;
    notice_.clear();
    BattleSceneAppearance appearance;
    appearance.background = battle_entry_->request.background;
    appearance.window.skin = scene_.owned_.window_skin;
    if (scene_.owned_.window_colors)
        appearance.window.colors = scene_.owned_.window_colors->at(appearance.window.skin);
    appearance.font_color = scene_.owned_.font_color;
    appearance.green_soldier_disguise = scene_.owned_.state.flags.story[0x103];
    battle_entrance_ =
        make_battle_entrance(pack, *battle_formation_, *battle_party_, *battle_preparation_,
                             *battle_state_, appearance, battle_entry_->request.continue_music,
                             std::span{scene_.map().characters}.subspan(256, 256));
    // The formation layout owns side membership. Targeting and escape checks
    // must use those same groups; BattleState's ungrouped defaults place every
    // monster on both sides and would erase all retarget candidates in a pincer.
    const auto& groups = battle_entrance_->actors().monsters.groups;
    battle_state_->monsters_on_left = groups.monsters_on_left;
    battle_state_->monsters_on_right = groups.monsters_on_right;
    update_battle_display(*battle_state_);
}
void FieldSession::append_field_audio(std::vector<MusicPlayerCommand>& target) {
    for (const auto& command : scene_.take_audio()) {
        std::visit(
            [&](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, SongLoad>)
                    target.push_back(GameSong{value.identity, value.volume, value.suspend_current,
                                              value.alternate});
                else if constexpr (std::is_same_v<T, FieldSoundEffect>)
                    target.push_back(GameSoundEffect{value.effect, value.pan});
                else {
                    static_assert(std::is_same_v<T, PlaybackVolumeRamp> ||
                                  std::is_same_v<T, EffectPanRamp> ||
                                  std::is_same_v<T, MusicTempoRamp> ||
                                  std::is_same_v<T, MusicPitchRamp> ||
                                  std::is_same_v<T, MusicCue> || std::is_same_v<T, StopPlayback>);
                    target.push_back(value);
                }
            },
            command);
    }
}
FieldSession make_development_field_session(const ContentPack& pack, std::uint16_t map) {
    if (map != 19 && map != 20)
        throw std::invalid_argument("Development entry supports Narshe maps 19 and 20");
    auto assets = load_field_map_entry_assets(pack, map, FieldMap{});
    FieldLoopSeed seed(
        FieldEventRunner::parked(field_event_script(pack), assets.scene.player.idle_event));
    seed.state.roster = new_roster(assets.scene.party);
    initialize_field_flags(seed.state, pack);
    initialize_field_treasure_flags(seed.state.flags);
    assign_character_name(seed.state.roster, assets.scene.party, 0, 0);
    assign_character_properties(seed.state.roster, assets.scene.party, 0, 0);
    seed.state.roster.active_party = 1;
    seed.state.roster.membership[0] = {.enabled = true, .party = 1, .visible = true};
    auto& field = seed.state.field;
    auto& actor = field.objects[0];
    actor.membership = seed.state.roster.membership[0];
    actor.movement.mode = 2;
    actor.walking_animation = true;
    actor.upper.palette = actor.lower.palette = 2;
    actor.upper.priority = actor.lower.priority = 2;
    // Different authored entry profiles, never a map-specific gameplay rule.
    actor.ride = map == 19 ? FieldRide::magitek : FieldRide::none;
    actor.rider_visible = map == 19;
    field.current_map = map;
    field.party_selectors[0] = 0;
    sort_field_objects(seed.state);
    FieldMapLoadPlan plan;
    plan.map = map;
    plan.arrival = FieldMapArrival{{38, 49}, FieldFacing::down, true};
    plan.characters = save_field_character_locations(field);
    plan.run_startup = true;
    seed.transition.pending = plan;
    auto entry = prepare_field_map_entry(std::move(assets), std::move(seed));
    return FieldSession(FieldLoop(std::move(entry.assets), std::move(entry.seed)));
}
void FieldPacer::update(std::uint64_t now, bool active) {
    if (!active || !previous_ || now < *previous_ || now - *previous_ >= 250000000U) {
        fraction_ = 0;
    } else {
        const auto elapsed = now - *previous_;
        if (fraction_ > UINT64_MAX - elapsed * field_session_rate)
            throw std::overflow_error("Field host clock overflow");
        fraction_ += elapsed * field_session_rate;
    }
    previous_ = active ? std::optional{now} : std::nullopt;
}
void FieldPacer::consume() {
    if (!due())
        throw std::logic_error("Field host consumed a tick before it was due");
    fraction_ -= 1000000000U;
}
} // namespace ff6
