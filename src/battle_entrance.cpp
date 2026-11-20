#include "ff6/battle_entrance.hpp"
#include "rom-materials/battle_sprite_reset_priority.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
void update_anchors(BattleActorScene& state, const BattleActorSceneAssets& assets) {
    std::array<BattleMonsterAnchorInput, 6> monsters;
    std::array<BattleCharacterAnchorInput, 4> characters;
    for (unsigned i = 0; i < 6; ++i) {
        const auto& p = state.monsters.positions[i];
        const auto& size = assets.monsters.dimensions[i];
        monsters[i] = {p.x, p.y, size.overlap_y, p.cursor_front, size.width, size.height};
    }
    for (unsigned i = 0; i < 4; ++i) {
        const auto& p = state.characters.layout.poses[i];
        characters[i] = {p.x,        p.y,           p.offset_x,
                         p.offset_y, p.animation_x, state.monsters.character_cursor_front[i]};
    }
    state.anchors = calculate_battle_anchors(monsters, characters);
}
} // namespace
BattleEntranceData load_battle_entrance_data(const ContentPack& pack) {
    const auto order = pack.get("battle_entry_order"), songs = pack.get("battle_song_selection"),
               transitions = pack.get("battle_monster_transition_programs");
    require(order.size() == 17 && songs.size() == 8 && transitions.size() == 232,
            "Invalid battle entrance directory extent");
    BattleEntranceData out;
    for (unsigned i = 0; i < 17; ++i) {
        require(order[i] <= 1, "Invalid character entrance order");
        out.characters_first[i] = order[i] != 0;
    }
    for (unsigned i = 0; i < 8; ++i)
        if (songs[i] != 255)
            out.songs[i] = songs[i];
    for (unsigned i = 0; i < 18; ++i) {
        const auto pointer = read16(transitions, 36 + i * 2);
        require(pointer >= 0xe4ebU + 72U && pointer < 0xe4ebU + transitions.size(),
                "Monster entry pointer leaves its data resource");
        const auto at = std::size_t{pointer - 0xe4ebU};
        if (transitions[at] == 255)
            continue;
        require(at + 6 <= transitions.size() && transitions[at] == 0 && transitions[at + 1] == 4 &&
                    transitions[at + 4] == 1 && transitions[at + 5] == 255,
                "Unsupported monster transition program grammar");
        const auto offset = read16(transitions, at + 2);
        require(offset % 14 == 0, "Unaligned monster animation property");
        out.monster_properties[i] = static_cast<std::uint16_t>(offset / 14U);
    }
    return out;
}
std::optional<std::uint8_t> select_battle_song(const BattleEntranceData& data,
                                               const BattleFormation& formation,
                                               bool continue_field_music,
                                               std::optional<std::uint8_t> scripted_song) {
    if (formation.continue_music || continue_field_music)
        return {};
    require(formation.song_slot < data.songs.size(), "Invalid battle song selection");
    return scripted_song ? scripted_song : data.songs[formation.song_slot];
}
BattleEntrance::BattleEntrance(BattleEntranceAssets assets, BattleEntranceSeed seed)
    : assets_(std::move(assets)), seed_(std::move(seed)) {
    require(seed_.rules.battle_time_stopped, "Initial battle entrance must stop battle time");
    require(!assets_.monster_effect ||
                (assets_.effects && *assets_.monster_effect < assets_.effects->definitions.size()),
            "Monster entrance requires its complete native effect resources");
    require(!assets_.flashing_monsters || assets_.effects, "Flashing entrance requires its durations");
    update_anchors(seed_.actors, assets_.actors);
    stage_battle_draw_order(seed_.actors.order, seed_.actors.anchors);
    seed_.actors.animation.visible.fill(false);
    seed_.actors.animation.monster_entry_visible.fill(false);
    for (unsigned i = 0; i < 4; ++i)
        if (seed_.readout.characters[i])
            counters_[i] = seed_.readout.characters[i]->counters;
    gauges_.prepared = gauges_.visible = counters_;
}
void BattleEntrance::settle_final_tier(std::uint16_t clock) {
    // The scrolling owner has already introduced the terrain; do not replay a
    // fresh battle entrance or its music. Publish the replacement actor set.
    seed_.actors.clock = clock;
    seed_.actors.animation.visible.fill(true);
    seed_.actors.animation.monster_entry_visible.fill(true);
    seed_.rules.battle_time_stopped = false;
    reset_priorities();
    for (unsigned pass = 0; pass < 2; ++pass) {
        prepare_battle_actor_scene(assets_.actors, seed_.rules, seed_.actors);
        publish_battle_actor_scene(seed_.actors, assets_.actors, seed_.rules.hide_characters);
    }
    phase_ = BattleEntrancePhase::complete;
    fade_.brightness = 15; fade_.remaining = 0; fade_.subtraction.fill(0);
}
void BattleEntrance::update_gauges(const BattleParty& party, const BattleState& battle) {
    const auto view = make_battle_readout_view(party, battle, seed_.hide_names, seed_.readout.show_gauges);
    for (unsigned i = 0; i < 4; ++i)
        if (view.characters[i])
            counters_[i] = view.characters[i]->counters;
}
void BattleEntrance::publish_frame(BattleEntranceTick& tick) {
    auto& actor = seed_.actors;
    publish_battle_actor_scene(actor, assets_.actors, seed_.rules.hide_characters);
    // WaitFrame publishes retained text, prepares one counter row, then calls
    // battle time. The host supplies the preceding logical counter buffer.
    advance_battle_gauge_publication(gauges_, counters_);
    for (unsigned i = 0; i < 4; ++i)
        if (seed_.readout.characters[i])
            seed_.readout.characters[i]->counters = gauges_.visible[i];
    visible_ = compose_battle_scene(assets_.background, assets_.readouts, seed_.readout, {},
                                    {0, 1, 256, (FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_0 + FF6_ROM_VALUE_FINAL_BATTLE_VIEWPORT_1)}, actor.clock);
    const auto groups = make_battle_actor_sprite_groups(assets_.actors, actor.publication,
                                                        monsters_ ? monsters_->actor_palettes()
                                                                  : BattleActorPaletteOverrides{});
    if (monsters_)
        monsters_->compose(*visible_, groups);
    else
        for (const auto& group : groups)
            for (const auto& piece : group.sprites)
                visible_->objects->sprites.push_back(piece.image);
    apply_battle_entry_fade(fade_, *visible_);
    prepare_battle_actor_scene(assets_.actors, seed_.rules, actor);
    advance_battle_entry_fade(fade_);
    ++frames_;
    tick.interval = tick.image_changed = tick.scene_updated = true;
    // Timing follows scene/text preparation. The semantic battle owner services
    // this request; animation budget continuations never create an extra tick.
    tick.battle_time_requested = !seed_.rules.battle_time_stopped;
    if (tick.battle_time_requested)
        ++battle_time_requests_;
}
void BattleEntrance::reset_priorities() {
    for (auto& pose : seed_.actors.characters.layout.poses)
        pose.appearance.priority = FF6_ROM_VALUE_BATTLE_SPRITE_RESET_PRIORITY_0;
    for (auto& style : seed_.actors.monster_styles)
        style.priority = FF6_ROM_VALUE_BATTLE_SPRITE_RESET_PRIORITY_0;
}
void BattleEntrance::start_stage() {
    auto& actor = seed_.actors;
    const bool character_stage = seed_.characters_first != second_stage_;
    stage_battle_draw_order(actor.order, actor.anchors);
    if (character_stage) {
        seed_.rules.battle_time_stopped = true;
        characters_ = make_battle_character_entry_animation(
            assets_.character_programs, seed_.characters, actor.characters.layout.poses,
            actor.anchors, seed_.rules.magitek, false, actor.animation);
        phase_ = BattleEntrancePhase::character_commands;
    } else if (!assets_.monster_effect && !assets_.flashing_monsters) {
        actor.animation.monster_entry_visible.fill(true);
        phase_ = BattleEntrancePhase::next_stage;
    } else {
        BattleEffectContext context;
        context.battle_type = static_cast<std::uint8_t>(seed_.approach);
        context.anchors = actor.anchors;
        context.characters = actor.characters.layout.poses;
        for (unsigned i = 0; i < 4; ++i)
            context.shown_character_status[i] = actor.characters.control[i].shown_status;
        context.monsters = actor.monsters.positions;
        context.monster_facing = seed_.rules.retained_monster_facing;
        context.control = actor.animation;
        context.draw_order = actor.order.staged;
        context.monster_styles = actor.monster_styles;
        context.magitek = seed_.rules.magitek;
        context.character_colors = battle_character_colors(actor, assets_.actors);
        context.monster_colors = battle_monster_colors(actor, assets_.actors);
        context.battlefield_colors.assign(assets_.background.colors.begin(),
                                          assets_.background.colors.end());
        context.freeze_character_palettes = seed_.rules.freeze_palettes;
        context.hide_characters = seed_.rules.hide_characters;
        context.presentation_tick = actor.clock;
        context.published_character_frames = actor.publication.character_frames;
        context.monster_images = std::make_shared<BattleMonsterBackgroundSource>(
            make_battle_monster_background_source(assets_.actors, actor.publication));
        for (unsigned i = 0; i < 6; ++i) {
            context.targets[i + 4] = context.shown_monsters[i] = seed_.rules.shown_monsters[i];
            context.visible_monsters[i] = !seed_.rules.invisible_monsters[i];
            context.monster_widths[i] = assets_.actors.monsters.dimensions[i].width;
            context.monster_heights[i] = assets_.actors.monsters.dimensions[i].height;
        }
        if (context.targets.none()) {
            actor.animation.monster_entry_visible.fill(true);
            phase_ = BattleEntrancePhase::next_stage;
            return;
        }
        context.hits = context.targets;
        context.attacker = 4;
        while (!context.targets[context.attacker])
            ++context.attacker;
        // Monster entry scripts select ordinary attack definitions (C2/E564
        // for water). Use the same task/image/palette/sound owners as attacks.
        if (assets_.flashing_monsters) {
            special_context_ = std::move(context);
            special_monsters_.emplace(*assets_.effects, 13, true, special_context_);
        } else {
            monsters_.emplace(assets_.effects, *assets_.monster_effect, context,
                              initial_battle_effect_images());
        }
        phase_ = BattleEntrancePhase::monster_commands;
    }
}
void BattleEntrance::finish_commands() {
    seed_.actors.animation.fixed_draw_order = false;
    stage_battle_draw_order(seed_.actors.order, seed_.actors.anchors);
    reset_priorities();
    phase_ = BattleEntrancePhase::cleanup_publish;
}
BattleEntranceTick BattleEntrance::advance(bool audio_ready, std::size_t command_budget) {
    BattleEntranceTick tick;
    if (complete() || command_budget == 0)
        return tick;
    if (phase_ == BattleEntrancePhase::start) {
        if (seed_.song)
            tick.audio.push_back(GameSong{*seed_.song, 255, false});
        RasterScene blank;
        blank.intensity.resize(blank.height);
        visible_ = std::move(blank);
        tick.interval = tick.image_changed = true;
        phase_ = BattleEntrancePhase::music_ready;
        return tick;
    }
    if (phase_ == BattleEntrancePhase::music_ready) {
        if (seed_.song && !audio_ready) {
            tick.interval = true;
            return tick;
        }
        // Publish/prep before any actor can become visible. Resource uploads
        // are native resident content; there is no emulated video transfer wait.
        publish_frame(tick);
        phase_ = BattleEntrancePhase::first_stage;
        return tick;
    }
    // Only finite caller transitions can share a visit. Animation work itself
    // has its own bounded continuation and never fabricates a display interval.
    for (unsigned transition = 0; transition < 8; ++transition) {
        if (phase_ == BattleEntrancePhase::first_stage)
            start_stage();
        if (phase_ == BattleEntrancePhase::character_commands) {
            characters_->control = seed_.actors.animation;
            const auto boundary = advance_battle_character_animation(
                assets_.character_programs.programs, *characters_,
                seed_.actors.characters.layout.poses, command_budget);
            seed_.actors.animation = characters_->control;
            if (boundary == BattleAnimationBoundary::work_limit)
                return tick;
            for (const auto& t : characters_->tasks)
                if (t && t->effect_frame && *t->effect_frame != 31)
                    throw std::runtime_error(
                        "Character entry effect geometry is not implemented yet");
            publish_frame(tick);
            ++character_passes_;
            resume_battle_character_animation(*characters_);
            seed_.actors.animation.fixed_draw_order = characters_->control.fixed_draw_order;
            if (!seed_.actors.animation.fixed_draw_order)
                stage_battle_draw_order(seed_.actors.order, seed_.actors.anchors);
            if (characters_->boundary == BattleAnimationBoundary::complete)
                finish_commands();
            return tick;
        }
        if (phase_ == BattleEntrancePhase::monster_commands && special_monsters_) {
            special_monsters_->advance(special_context_);
            apply_battle_effect_actor_state(special_context_, seed_.actors, seed_.rules);
            for (unsigned i = 0; i < 6; ++i)
                seed_.rules.shown_monsters[i] = special_context_.shown_monsters[i];
            publish_frame(tick);
            ++monster_passes_;
            if (special_monsters_->complete()) finish_commands();
            return tick;
        }
        if (phase_ == BattleEntrancePhase::monster_commands) {
            monsters_->update_anchors(seed_.actors.anchors);
            monsters_->update_draw_order(seed_.actors.order.staged);
            monsters_->update_presentation_tick(seed_.actors.clock);
            // Effect scripts have their own bounded per-picture work limit;
            // the caller's resumable command budget applies to character tasks.
            const auto frame = monsters_->advance_presentation();
            monsters_->apply_actor_state(seed_.actors, seed_.rules);
            if (frame.sound)
                tick.audio.push_back(*frame.sound);
            if (frame.system_sound)
                tick.audio.push_back(*frame.system_sound);
            publish_frame(tick);
            ++monster_passes_;
            if (!seed_.actors.animation.fixed_draw_order)
                stage_battle_draw_order(seed_.actors.order, seed_.actors.anchors);
            if (frame.complete)
                finish_commands();
            return tick;
        }
        if (phase_ == BattleEntrancePhase::cleanup_publish) {
            publish_frame(tick);
            phase_ = BattleEntrancePhase::cleanup_finish;
            return tick;
        }
        if (phase_ == BattleEntrancePhase::cleanup_finish) {
            // DeinitAnimType has two explicit frame waits around cleanup.
            publish_frame(tick);
            if (characters_) {
                characters_.reset();
                seed_.rules.battle_time_stopped = false;
            }
            if (monsters_ || special_monsters_) {
                special_monsters_.reset();
                monsters_.reset();
                seed_.actors.animation.monster_entry_visible.fill(true);
            }
            phase_ = BattleEntrancePhase::next_stage;
            return tick;
        }
        if (phase_ == BattleEntrancePhase::next_stage) {
            if (!second_stage_) {
                second_stage_ = true;
                phase_ = BattleEntrancePhase::first_stage;
                continue;
            }
            phase_ = BattleEntrancePhase::final_fade;
        }
        if (phase_ == BattleEntrancePhase::final_fade) {
            // The caller waits at least once, even if the reveal already ended.
            const bool already_revealed = fade_.complete();
            publish_frame(tick);
            if (already_revealed)
                phase_ = BattleEntrancePhase::complete;
            return tick;
        }
    }
    throw std::logic_error("Unbounded battle entrance caller transitions");
}
BattleEntrance make_battle_entrance(const ContentPack& pack, const BattleFormationEntry& formation,
                                    const BattleParty& party, const BattlePreparation& preparation,
                                    const BattleState& battle,
                                    const BattleSceneAppearance& appearance,
                                    bool continue_field_music,
                                    std::span<const std::optional<Tile>> retained_terrain) {
    const auto script = battle_scripted_presentation(load_battle_party_data(pack), party);
    const auto background = script.background.value_or(appearance.background);
    const auto data = load_battle_entrance_data(pack);
    const auto type = formation.formation.entrance & 15U;
    BattleEntranceAssets assets;
    BattleEntranceSeed seed;
    seed.approach = battle.entry.approach;
    seed.characters_first = data.characters_first[type];
    seed.song = select_battle_song(data, formation.formation, continue_field_music, script.song);
    seed.hide_names = script.hide_names;
    seed.enemy_character_proxy = script.enemy_character_proxy;
    assets.character_programs = load_battle_character_animation_content(pack);
    assets.monster_effect = data.monster_properties[type];
    assets.flashing_monsters = type == 13;
    if (assets.monster_effect || assets.flashing_monsters)
        assets.effects = std::make_shared<BattleEffectLibrary>(load_battle_effects(pack));
    for (const auto& character : battle.character_display)
        seed.rules.magitek |= (character.status & status_mask(BattleStatus::magitek)) != 0;
    seed.rules.battle_time_stopped = true;
    seed.rules.forced_running_background = background == 33;
    seed.rules.appearance.green_soldier_disguise = appearance.green_soldier_disguise;
    assets.background = load_battle_background(pack, background, retained_terrain);
    initialize_battle_background_motion(pack, assets.background);
    const auto monster_data = load_battle_monster_graphics_data(pack);
    const auto monster_view = load_battle_monster_view_data(pack);
    BattleMonsterGraphicsRequest monster_request;
    monster_request.layout = formation.formation.graphics_layout;
    for (unsigned i = 0; i < 6; ++i) {
        monster_request.identities[i] = battle.colosseum && preparation.monsters.slots[i]
            ? std::optional{preparation.monsters.slots[i]->identity} : formation.formation.monsters[i].monster;
        monster_request.imp[i] =
            (battle.status.targets[i + 4].active & status_mask(BattleStatus::imp)) != 0;
        seed.rules.shown_monsters[i] =
            formation.formation.monsters[i].present && battle.participation.active[i + 4];
        seed.rules.invisible_monsters[i] =
            (battle.status.targets[i + 4].active & status_mask(BattleStatus::vanish)) != 0;
    }
    assets.actors.monsters = initialize_battle_monster_graphics(monster_data, monster_request);
    assets.actors.monster_graphics_data =
        std::make_shared<BattleMonsterGraphicsData>(monster_data);
    assets.actors.monster_graphics_selection = monster_request;
    assets.actors.monster_pieces = make_monster_sprite_variants(
        monster_view, monster_request.layout, assets.actors.monsters.dimensions);
    BattleMonsterPositionRequest positions;
    positions.approach = battle.entry.approach;
    if (battle.colosseum && preparation.monsters.slots[0])
        positions.colosseum_monster = wrap8(preparation.monsters.slots[0]->identity);
    positions.sizes = assets.actors.monsters.dimensions;
    for (unsigned i = 0; i < 6; ++i) {
        positions.positions[i].x =
            static_cast<std::uint16_t>(formation.formation.monsters[i].x_tiles * 8U);
        positions.positions[i].y =
            static_cast<std::uint16_t>(formation.formation.monsters[i].y_tiles * 8U);
        seed.actors.monster_styles[i].palette = assets.actors.monsters.palette_slots[i];
    }
    seed.actors.monsters = initialize_monster_positions(monster_view, positions);
    assets.actors.graphics = load_battle_character_graphics_data(pack);
    assets.actors.view = load_battle_character_view_data(pack);
    assets.actors.palettes = load_battle_character_palette_data(pack);
    BattleCharacterLayoutRequest request;
    request.approach = battle.entry.approach;
    // InitCharGfxMain selects ordinary or armor layout from the party's actual
    // display status. InitCharGfx (C1/3D27) then visits the same status owner
    // used during combat, including Vanish retained from the preceding battle.
    request.colosseum = battle.colosseum;
    request.magitek = seed.rules.magitek;
    request.magitek_train = background == 44;
    request.green_soldier_disguise = appearance.green_soldier_disguise;
    for (unsigned i = 0; i < 4; ++i) {
        if (preparation.characters.slots[i])
            request.graphics[i] = preparation.characters.slots[i]->combat.graphics;
        request.characters[i] = party.slots[i].actor.value_or(255);
        request.back_row[i] = battle.entry.displayed_back_rows[i];
        request.enemy[i] = seed.rules.enemy_characters[i] = party.slots[i].enemy;
        seed.rules.shown_characters[i] = party.slots[i].graphics.has_value();
        if (script.positions[i])
            request.overrides[i] = BattleCharacterPlacementOverride{(*script.positions[i])[0],
                                                                    (*script.positions[i])[1]};
        seed.characters[i] = {request.graphics[i], party.slots[i].present, script.positions[i].has_value(),
                              battle.character_display[i].status};
        auto& c = seed.actors.characters.control[i];
        c.active_status = c.shown_status = battle.character_display[i].status;
    }
    std::uint8_t clock = 0;
    initialize_battle_character_presentation(
        assets.actors.view, assets.actors.graphics, assets.actors.palettes, request,
        {false, appearance.green_soldier_disguise}, clock, seed.actors.characters);
    seed.actors.clock = clock;
    const auto status = pack.get("battle_character_status_palette");
    assets.actors.status_sprites = load_battle_status_sprite_assets(pack);
    require(status.size() == 32, "Invalid battle status palette extent");
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned c = 0; c < 4; ++c)
            assets.actors.status_colors[i][c] = read16(status, i * 8 + c * 2) & 32767U;
    assets.readouts = load_battle_readout_assets(pack);
    seed.readout =
        make_battle_readout_view(party, battle, seed.hide_names, preparation.configuration.show_gauge);
    seed.readout.window = appearance.window;
    seed.readout.font_color = appearance.font_color;
    return BattleEntrance(std::move(assets), std::move(seed));
}
} // namespace ff6
