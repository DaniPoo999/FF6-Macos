#include "ff6/saved_game.hpp"
#include "ff6/field_map_entry.hpp"
namespace ff6 {
SavedGame retain_game_over_progress(SavedGame saved, const Roster& defeated,
                                    const PartyData& growth) {
    for (unsigned i = 0; i < saved.roster.characters.size(); ++i) {
        auto& restored = saved.roster.characters[i];
        const auto& previous = defeated.characters[i];
        if (!restored.properties || restored.properties != previous.properties)
            continue;
        if (previous.level < restored.level || previous.level > 99 || restored.level == 0)
            throw std::invalid_argument("Game-over save has incompatible character levels");
        // C0/RestartGame adds ordinary growth to the saved base, preserving
        // neither intervening esper bonuses nor intervening current HP/MP.
        // UpdateEquip subsequently reapplies the saved equipment's boosts.
        for (unsigned level = restored.level; level < previous.level; ++level) {
            restored.hp.base = static_cast<std::uint16_t>(
                std::min(unsigned{restored.hp.base} + growth.hp_growth.at(level - 1), 9999U));
            restored.mp.base = static_cast<std::uint16_t>(
                std::min(unsigned{restored.mp.base} + growth.mp_growth.at(level - 1), 999U));
        }
        restored.level = previous.level;
        restored.experience = previous.experience;
        learn_character_level_abilities(saved.roster.abilities, growth, restored);
    }
    return saved;
}
SavedGame capture_saved_game(const FieldLoopSeed& seed) {
    SavedGame out;
    out.terra_magic_pending = seed.state.terra_magic_pending;
    out.world = seed.world;
    out.parked_airship = seed.parked_airship;
    out.airship_resume = seed.airship_resume;
    out.world_minimap = seed.world_minimap;
    out.roster = seed.state.roster;
    out.inventory = seed.state.inventory;
    out.flags = seed.state.flags;
    out.event_variables = seed.state.event_variables;
    // C3/1586 PushTimers and C3/1595 PopTimers preserve all four countdowns,
    // callbacks and flags. They are gameplay state, including Cid's health loop.
    out.timers = seed.timers;
    out.battle_variables = seed.state.battle_variables;
    out.encountered_formations = seed.state.encountered_formations;
    out.menu = seed.menu;
    out.input = seed.settings;
    out.colors = seed.window_colors.value();
    out.skin = seed.window_skin;
    out.font_color = seed.font_color;
    out.steps = seed.player.effects.steps;
    const auto& field = seed.state.field;
    const auto locations = save_field_character_locations(field);
    for (unsigned i = 0; i < 16; ++i) {
        const auto& actor = field.objects[i];
        out.characters[i] = {locations[i], actor.upper.palette, actor.facing, actor.ride,
                             actor.rider_visible};
    }
    out.leader = field.update_order[0].value();
    out.destination = {field.current_map, seed.actions.player_tile_position[0],
                       seed.actions.player_tile_position[1], field.objects[out.leader].facing};
    out.parent = field.parent_map;
    out.scroll = {seed.actions.scroll.layers[0][0].center, seed.actions.scroll.layers[0][1].center};
    out.terrain_level = seed.actions.player_terrain.level;
    out.map_song = seed.map_song;
    out.field_random = seed.actions.random_index;
    out.travel = seed.travel;
    out.battle_random = seed.battle_random;
    out.encounter_counter = seed.encounter_counter;
    out.doom_gaze_hp = seed.saved_doom_gaze_hp;
    out.battle_conditions = seed.battle_conditions;
    out.battle_rules = seed.battle_command_rules;
    out.party_levels = seed.party_levels;
    validate_saved_game(out);
    return out;
}
FieldLoop restore_saved_game(const ContentPack& pack, const SavedGame& game) {
    validate_saved_game(game);
    auto assets = load_field_map_entry_assets(pack, game.destination.map, FieldMap{});
    FieldLoopSeed seed(
        FieldEventRunner::parked(field_event_script(pack), assets.scene.player.idle_event));
    seed.state.terra_magic_pending = game.terra_magic_pending;
    seed.world = game.world;
    seed.parked_airship = game.parked_airship;
    seed.airship_resume = game.airship_resume;
    seed.world_minimap = game.world_minimap;
    seed.state.roster = game.roster;
    seed.state.inventory = game.inventory;
    seed.state.flags = game.flags;
    seed.state.event_variables = game.event_variables;
    seed.timers = game.timers;
    seed.state.battle_variables = game.battle_variables;
    seed.state.encountered_formations = game.encountered_formations;
    seed.menu = game.menu;
    seed.settings = game.input;
    seed.window_colors = game.colors;
    seed.window_skin = game.skin;
    seed.font_color = game.font_color;
    seed.player.effects.steps = game.steps;
    seed.map_song = game.map_song;
    seed.actions.random_index = game.field_random;
    seed.actions.player_tile_position = {game.destination.x, game.destination.y};
    seed.actions.player_terrain.level = game.terrain_level;
    seed.travel = game.travel;
    seed.battle_random = game.battle_random;
    seed.encounter_counter = game.encounter_counter;
    seed.saved_doom_gaze_hp = game.doom_gaze_hp;
    seed.battle_conditions = game.battle_conditions;
    seed.battle_command_rules = game.battle_rules;
    seed.party_levels = game.party_levels;
    auto& field = seed.state.field;
    field.current_map = game.destination.map;
    FieldMapLoadPlan plan;
    plan.map = game.destination.map;
    plan.parent = game.parent;
    plan.arrival = FieldMapArrival{{game.destination.x, game.destination.y},
                                   static_cast<FieldFacing>(game.destination.facing),
                                   false};
    plan.run_startup = !game.world;
    plan.fade_in = true;
    plan.saved_level = game.terrain_level;
    for (unsigned i = 0; i < 16; ++i) {
        const auto& saved = game.characters[i];
        auto& actor = field.objects[i];
        plan.characters[i] = saved.location;
        actor.map = saved.location.map;
        actor.membership = game.roster.membership[i];
        // InitSavedGame rebuilds object graphics from the persistent character
        // record. Palette restoration alone leaves every object using sprite 0.
        actor.graphics = game.roster.characters[i].graphics;
        // InitSavedGame clears every character's current/saved movement, then
        // enables input only on the saved leader. Giving hidden companions
        // player control makes them run movement against the leader's state.
        actor.movement.mode = i == game.leader ? 2 : 0;
        actor.walking_animation = true;
        actor.upper.palette = actor.lower.palette = saved.palette;
        actor.upper.priority = actor.lower.priority = 2;
        actor.facing = saved.facing;
        actor.ride = saved.ride;
        actor.rider_visible = saved.rider_visible;
    }
    field.party_selectors[game.roster.active_party - 1] = game.leader;
    sort_field_objects(seed.state);
    seed.transition.pending = plan;
    auto entry = prepare_field_map_entry(std::move(assets), std::move(seed));
    // Original load rebuilds the map and its startup rather than serializing
    // transient event stacks, NPC tasks or renderer state.
    return FieldLoop(std::move(entry.assets), std::move(entry.seed));
}
} // namespace ff6
