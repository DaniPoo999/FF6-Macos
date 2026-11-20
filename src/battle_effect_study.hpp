#pragma once
#include "ff6/battle_action_animation.hpp"
#include "ff6/field_session.hpp"

// Development study: fixed target anchors and a deliberately composed party,
// with native actor publication. This is not combat simulation or a game party.
struct BattleEffectStudy {
    std::shared_ptr<const ff6::BattleEffectLibrary> library;
    ff6::BattleBackground background;
    ff6::BattleActorSceneAssets actor_assets;
    ff6::BattleActorScene actors;
    ff6::BattleActorSceneRules actor_rules;
    std::optional<ff6::BattleAttackAnimation> animation;
    std::optional<ff6::BattleActionAnimation> action;
    enum class Playback { body, sequence, automatic };
    Playback playback = Playback::body;
    ff6::FieldPacer clock;
    ff6::RasterScene visible;
    std::uint16_t identity = 131;
    std::string error;
    std::string sequence_error;
    bool paused = false;
    enum class Target { monster, character, party };
    Target target = Target::monster;
    unsigned cues = 0;
    unsigned appearance_tail = 0, ambient_frames = 0;
    // All requests in the last update, before the shared final-cue dispatch.
    std::vector<ff6::GameSoundEffect> sound_requests;
    explicit BattleEffectStudy(const ff6::ContentPack& pack, Playback mode = Playback::body)
        : library(std::make_shared<ff6::BattleEffectLibrary>(ff6::load_battle_effects(pack))),
          background(ff6::load_battle_background(pack, 8)), playback(mode) {
        // The study's Narshe backdrop inherits terrain from its entering map,
        // just as the live field-to-battle caller does. Waves can expose it.
        const auto field = ff6::load_field_map(pack, 19);
        background =
            ff6::load_battle_background(pack, 8, std::span{field.characters}.subspan(256, 256));
        actor_assets.view = ff6::load_battle_character_view_data(pack);
        actor_assets.graphics = ff6::load_battle_character_graphics_data(pack);
        actor_assets.palettes = ff6::load_battle_character_palette_data(pack);
        ff6::BattleMonsterGraphicsRequest monster_request;
        monster_request.identities[0] = 0; // Development target, independent of spell identity.
        actor_assets.monster_graphics_selection = monster_request;
        actor_assets.monster_graphics_data = std::make_shared<ff6::BattleMonsterGraphicsData>(
            ff6::load_battle_monster_graphics_data(pack));
        actor_assets.monsters = ff6::initialize_battle_monster_graphics(
            *actor_assets.monster_graphics_data, monster_request);
        actor_assets.monster_pieces = ff6::make_monster_sprite_variants(
            ff6::load_battle_monster_view_data(pack), monster_request.layout,
            actor_assets.monsters.dimensions);
        const auto status = pack.get("battle_character_status_palette");
        if (status.size() != 32)
            throw std::invalid_argument("Invalid battle status palette extent");
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned color = 0; color < 4; ++color)
                actor_assets.status_colors[i][color] =
                    ff6::read16(status, i * 8 + color * 2) & 32767U;
        restart();
    }
    ff6::RasterScene base_scene() const {
        ff6::RasterScene scene;
        scene.intensity.assign(224, 15);
        auto layer = ff6::make_battle_background_plane(background, {.width = 256, .height = 151});
        layer.lines.resize(224);
        for (unsigned y = 0; y < 224; ++y) {
            layer.lines[y].enabled = y >= 4 && y < 151;
            layer.lines[y].left = 8;
            layer.lines[y].right = 248;
        }
        scene.layers.push_back(std::move(layer));
        return scene;
    }
    void restart() {
        animation.reset();
        action.reset();
        error.clear();
        sequence_error.clear();
        cues = 0;
        appearance_tail = ambient_frames = 0;
        sound_requests.clear();
        clock = {};
        visible = base_scene();
        try {
            ff6::BattleEffectContext context;
            context.battlefield_colors.assign(background.colors.begin(), background.colors.end());
            context.attacker = 0;
            // Explicit study targets are independent of the selected attack.
            // Precasts, weapon/esper setup and varied target layouts have their
            // own owners and must never be implied by this preview.
            if (target == Target::party)
                context.targets = context.hits = 7;
            else
                context.targets[target == Target::character ? 0 : 4] =
                    context.hits[target == Target::character ? 0 : 4] = true;
            context.magitek = identity >= 131 && identity <= 138;
            actors = {};
            actor_rules = {};
            actor_rules.magitek = context.magitek;
            actor_rules.shown_characters = {true, true, true, false};
            actors.animation.visible = {true, true, true, false};
            ff6::BattleCharacterLayoutRequest request;
            request.graphics = request.characters = {0, 1, 2, 255};
            request.magitek = context.magitek;
            std::uint8_t phase = 0;
            ff6::initialize_battle_character_presentation(actor_assets.view, actor_assets.graphics,
                                                          actor_assets.palettes, request, {}, phase,
                                                          actors.characters);
            actors.clock = phase;
            for (unsigned slot = 0; slot < 3; ++slot) {
                auto& pose = actors.characters.layout.poses[slot];
                pose.x = 192;
                pose.y = static_cast<std::uint16_t>(72 + slot * 24);
                pose.appearance.priority = 3;
                actors.order.published[slot] = {pose.y, static_cast<std::uint8_t>(slot)};
            }
            actors.order.staged = actors.order.published;
            actor_rules.shown_monsters[0] = true;
            actors.animation.monster_entry_visible[0] = true;
            actors.monsters.positions[0] = {40, 44};
            actors.monster_styles[0].palette = actor_assets.monsters.palette_slots[0];
            actors.monster_styles[0].priority = 3;
            actors.order.published[3] = {84, 4};
            actors.order.staged = actors.order.published;
            // Prime separately scheduled images, including all armor selections.
            for (unsigned i = 0; i < 4; ++i) {
                ff6::prepare_battle_actor_scene(actor_assets, actor_rules, actors);
                ff6::publish_battle_actor_scene(actors, actor_assets, false);
            }
            context.characters = actors.characters.layout.poses;
            context.character_colors = ff6::battle_character_colors(actors, actor_assets);
            context.monster_colors = ff6::battle_monster_colors(actors, actor_assets);
            context.monster_styles = actors.monster_styles;
            context.monster_images = std::make_shared<ff6::BattleMonsterBackgroundSource>(
                ff6::make_battle_monster_background_source(actor_assets));
            context.published_character_frames = actors.publication.character_frames;
            context.published_status_tiles = actors.publication.status_tiles;
            context.shown_monsters[0] = context.visible_monsters[0] = true;
            context.control = actors.animation;
            context.draw_order = actors.order.staged;
            for (unsigned slot = 0; slot < 3; ++slot) {
                const auto y = static_cast<std::uint16_t>(80 + slot * 24);
                context.anchors[slot] = {
                    200, y, static_cast<std::uint16_t>(y + 8), static_cast<std::uint16_t>(y + 8),
                    0,   0};
            }
            context.anchors[4] = {56, 68, 84, 84, 0, 0};
            context.characters[0].x = 192;
            context.characters[0].y = 72;
            context.monsters[0].x = 40;
            context.monsters[0].y = 44;
            context.monster_widths[0] = actor_assets.monsters.dimensions[0].width;
            context.monster_heights[0] = actor_assets.monsters.dimensions[0].height;
            const auto retained = ff6::initial_battle_effect_images();
            if (playback != Playback::body) {
                try {
                    const auto kind = context.magitek ? ff6::BattleActionAnimationKind::magitek
                                                      : ff6::BattleActionAnimationKind::magic;
                    ff6::BattleActionAnimation candidate(library, {kind, identity}, context,
                                                         retained);
                    auto validation = candidate;
                    auto state = actors;
                    auto rules = actor_rules;
                    for (unsigned frame = 0; !validation.complete(); ++frame) {
                        if (frame >= 4096)
                            throw std::invalid_argument("Action exceeds study frame limit");
                        validation.advance(actor_assets, state, rules);
                        auto scene = base_scene();
                        validation.compose(scene, ff6::make_battle_actor_sprite_groups(
                                                      actor_assets, state.publication,
                                                      validation.actor_palettes()));
                        (void)ff6::render_scene(scene);
                    }
                    action = std::move(candidate);
                    return;
                } catch (const std::invalid_argument& e) {
                    if (playback == Playback::sequence)
                        throw;
                    // The inspector's catalog includes isolated bodies whose
                    // pre-cast/return owner is unfinished or is a different command.
                    // Auto may show that body, explicitly labeled as such. Strict
                    // sequence mode and the gameplay caller still reject it.
                    sequence_error = e.what();
                }
            }
            ff6::BattleAttackAnimation candidate(library, identity, context, retained);
            auto validation = candidate;
            auto validation_actors = actors;
            auto validation_rules = actor_rules;
            for (unsigned frame = 0; !validation.complete(); ++frame) {
                if (frame >= 2048)
                    throw std::invalid_argument("Animation exceeds study frame limit");
                step_actors(validation, validation_actors, validation_rules);
                auto scene = base_scene();
                validation.compose(scene, ff6::make_battle_actor_sprite_groups(
                                              actor_assets, validation_actors.publication,
                                              validation.actor_palettes()));
                // A later wave can reveal an unowned retained source cell.
                // Validate actual samples before any public image or sound.
                (void)ff6::render_scene(scene);
            }
            animation = std::move(candidate);
        } catch (const std::exception& e) {
            error = e.what();
        }
    }
    bool complete() const {
        return (action ? action->complete() : !animation || animation->complete()) &&
               appearance_tail == 0;
    }
    bool available() const {
        return action.has_value() || animation.has_value();
    }
    void select(int direction, bool all_definitions) {
        if (direction != -1 && direction != 1)
            throw std::invalid_argument("Invalid animation browsing direction");
        const auto count = static_cast<unsigned>(library->definitions.size());
        // Evaluate the actual selected mode and target; a hard-coded support
        // list would become stale and hide newly implemented shared handlers.
        for (unsigned visited = 0; visited < count; ++visited) {
            identity =
                static_cast<std::uint16_t>((identity + (direction > 0 ? 1U : count - 1U)) % count);
            restart();
            if (all_definitions || available())
                return;
        }
    }
    void cycle_playback() {
        playback = playback == Playback::automatic ? Playback::body
                   : playback == Playback::body    ? Playback::sequence
                                                   : Playback::automatic;
        restart();
    }
    const char* mode_label() const {
        if (playback == Playback::automatic)
            return action      ? "Auto: sequence / S: mode"
                   : animation ? "Auto: body only / S: mode"
                               : "Auto / S: mode";
        return playback == Playback::body ? "Body only / S: mode" : "Sequence / S: mode";
    }
    std::uint32_t frames() const {
        return (action ? action->frames() : animation ? animation->frames() : 0) + ambient_frames;
    }
    const ff6::BattleEffectContext& context() const {
        return action ? action->context() : animation->context();
    }
    const char* phase_name() const {
        if (appearance_tail && (action ? action->complete() : animation && animation->complete()))
            return "Finishing actor appearance";
        if (action && action->resource_wait())
            return "Loading animation images";
        return action       ? ff6::battle_action_animation_phase_name(action->phase())
               : complete() ? "Body finished"
                            : "Playing body / P: pause";
    }
    std::optional<ff6::GameSoundEffect> advance() {
        sound_requests.clear();
        if (appearance_tail && (action ? action->complete() : animation && animation->complete())) {
            // The battle continues drawing after a graphics action returns.
            // Let persistent appearance transitions finish without adding waits
            // to the gameplay action itself, or freezing a preview mid-wipe.
            ff6::publish_battle_actor_scene(actors, actor_assets, actor_rules.hide_characters);
            ff6::prepare_battle_actor_scene(actor_assets, actor_rules, actors);
            ff6::update_battle_character_status(actor_assets.graphics, actor_rules.appearance,
                                                actors.characters);
            advance_appearance_tail();
            ++ambient_frames;
            visible = base_scene();
            const auto groups = ff6::make_battle_actor_sprite_groups(actor_assets, actors.publication);
            if (action) action->compose(visible, groups);
            else animation->compose(visible, groups);
            return {};
        }
        if (action) {
            if (action->complete())
                return {};
            const auto frame = action->advance(actor_assets, actors, actor_rules);
            advance_appearance_tail();
            sound_requests = frame.sound_requests;
            visible = base_scene();
            action->compose(visible,
                            ff6::make_battle_actor_sprite_groups(actor_assets, actors.publication,
                                                                 action->actor_palettes()));
            if (frame.sound)
                ++cues;
            return frame.sound;
        }
        if (!animation || animation->complete())
            return {};
        const auto frame = step_actors(*animation, actors, actor_rules);
        advance_appearance_tail();
        sound_requests = frame.sound_requests;
        visible = base_scene();
        animation->compose(visible,
                           ff6::make_battle_actor_sprite_groups(actor_assets, actors.publication,
                                                                animation->actor_palettes()));
        if (frame.sound)
            ++cues;
        return frame.sound;
    }

  private:
    void advance_appearance_tail() {
        // An unchanged pose may refresh only on its next eight-step image
        // visit, then needs up to four round-robin publications. Draining just
        // four leaves the final two wipe rows displaying the previous sheet.
        if (actors.characters.transition_busy)
            appearance_tail = 12;
        else if (appearance_tail)
            --appearance_tail;
    }
    ff6::BattleEffectFrame step_actors(ff6::BattleAttackAnimation& effect,
                                       ff6::BattleActorScene& state,
                                       ff6::BattleActorSceneRules& rules) const {
        ff6::publish_battle_actor_scene(state, actor_assets, rules.hide_characters);
        effect.update_character_frames(state.publication.character_frames);
        effect.update_character_colors(ff6::battle_character_colors(state, actor_assets));
        effect.update_monster_colors(ff6::battle_monster_colors(state, actor_assets));
        effect.update_draw_order(state.order.staged);
        effect.update_anchors(state.anchors);
        effect.update_presentation_tick(state.clock);
        const auto frame = effect.advance();
        effect.apply_actor_state(state, rules);
        ff6::apply_battle_actor_appearance_changes(actor_assets, rules, frame.appearance_changes,
                                                  state, false);
        ff6::prepare_battle_actor_scene(actor_assets, rules, state);
        ff6::update_battle_character_status(actor_assets.graphics, rules.appearance,
                                            state.characters);
        return frame;
    }
};
