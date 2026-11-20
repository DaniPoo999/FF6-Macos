#include "ff6/field_session.hpp"
#include "rom-materials/battle_initial_refresh_level.macros.inc"
#include "ff6/battle_entry.hpp"
#include "ff6/battle_display.hpp"
#include "ff6/battle_scene.hpp"
#include "ff6/battle_schedule.hpp"
#include "ff6/battle_rewards.hpp"
#include "rom-materials/final_battle_formations.macros.inc"
#include "rom-materials/final_battle_background_origin.macros.inc"

namespace ff6 {
void FieldSession::advance_final_tier(const ContentPack& pack) {
    const auto identity = next_final_battle(battle_formation_->selection.formation);
    if (!identity) throw std::logic_error("Final battle has no following tier");
    auto roster = scene_.owned_.state.roster;
    auto inventory = scene_.owned_.state.inventory;
    retain_battle_party(*battle_state_, *battle_party_, roster, inventory);
    const auto retained = final_battle_party_context(*battle_state_, *battle_party_);
    const auto data = load_battle_preparation_data(pack);
    const auto formations = load_battle_formation_data(pack);
    auto random = BattleRandom::from_game_clock(scene_.owned_.menu.time_frames);
    const auto selection = select_battle_formation(formations, *identity,
        scene_.owned_.battle_conditions, {}, random);
    BattleFormationEntry formation{selection, random, formations.formations.at(selection.formation)};
    auto party = initialize_battle_party(data.party, roster, formation, retained);
    auto preparation = prepare_battle(data, formation, party, scene_.assets_.party, scene_.assets_.equipment,
        roster, inventory, {scene_.owned_.menu, scene_.owned_.battle_command_rules,
                            scene_.owned_.saved_doom_gaze_hp, {}});
    auto battle = initialize_battle_state(preparation, party, initialize_battle_status(preparation, party));
    retain_final_battle_state(battle, *battle_state_, retained);
    battle.morph.saved_gauge = roster.morph_gauge;
    battle.morph.permanent = scene_.owned_.battle_command_rules.permanent_morph;
    battle.morph.double_duration = scene_.owned_.battle_command_rules.double_morph_duration;
    resolve_battle_status_consequences(battle);
    refresh_battle_actions(battle, data.commands, data.random, FF6_ROM_VALUE_BATTLE_INITIAL_REFRESH_LEVEL_0);
    initialize_battle_entry(battle, formation.formation, preparation.characters.party_relic_effects,
                           load_battle_approach_weights(pack), data.random);
    resume_final_battle_jumps(battle);
    prepare_battle_presentation(battle, formation.formation);
    BattleSceneAppearance appearance;
    // The final-battle event chooses the first background; each transition
    // advances it once. Keep that scene choice in the user-provided ROM.
    appearance.background = static_cast<std::uint8_t>(
        FF6_ROM_VALUE_FINAL_BATTLE_BACKGROUND_ORIGIN_0 + 1U
        + *identity - FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_1);
    appearance.window.skin = scene_.owned_.window_skin;
    if (scene_.owned_.window_colors) appearance.window.colors = scene_.owned_.window_colors->at(appearance.window.skin);
    appearance.font_color = scene_.owned_.font_color;
    auto entrance = make_battle_entrance(pack, formation, party, preparation, battle, appearance, true);
    entrance.settle_final_tier(battle_menus_->clock());
    auto transition = BattleFinalTransition(battle_menus_->final_tier_picture(),
        ff6::final_tier_picture(entrance.scene_assets(), entrance.scene_seed()),
        *identity == FF6_ROM_VALUE_FINAL_BATTLE_FORMATIONS_3);
    scene_.owned_.state.battle_variables = battle_menus_->global_script_variables();
    scene_.owned_.state.roster = std::move(roster);
    scene_.owned_.state.inventory = std::move(inventory);
    battle_state_ = std::move(battle); battle_preparation_ = std::move(preparation);
    battle_party_ = std::move(party); battle_formation_ = std::move(formation);
    battle_entrance_ = std::move(entrance); battle_menus_.reset();
    final_transition_ = std::move(transition);
    rewards_applied_ = false;
}
void FieldSession::change_battle(const ContentPack& pack) {
    const auto request = *battle_menus_->battle_change();
    // The only ordinary-game change is immediate. Final-battle scrolling has
    // its own cinematic owner; never silently treat it as an instant change.
    if (request.transition != 0)
        throw std::runtime_error("Scrolling battle change is not implemented yet");
    const auto data = load_battle_preparation_data(pack);
    const auto formations = load_battle_formation_data(pack);
    auto battle = *battle_state_;
    auto preparation = *battle_preparation_;
    const auto selection = select_battle_formation(formations, request.formation,
        scene_.owned_.battle_conditions, battle_formation_->selection.battle_identity, battle.random);
    BattleFormationEntry formation{selection, battle.random, formations.formations.at(selection.formation)};
    BattleMonsterRetention retained;
    retained.retain_vitals = !request.restore;
    for (unsigned i = 0; i < 6; ++i) {
        retained.stats[i] = battle.stats[i + 4];
        retained.stats[i].retaliation_flags = 0;
        retained.ai[i] = battle.ai[i + 4];
    }
    preparation.monsters = reload_battle_monsters(data, formation, scene_.owned_.saved_doom_gaze_hp,
                                                  retained, battle.random);
    preparation.random = battle.random;
    const auto status = initialize_battle_status(preparation, *battle_party_);
    const auto fresh = initialize_battle_state(preparation, *battle_party_, status);
    for (unsigned i = 4; i < 10; ++i) {
        release_battle_target_links(battle, static_cast<std::uint8_t>(i));
        battle.stats[i] = fresh.stats[i];
        auto& target = battle.status.targets[i];
        target.present = preparation.monsters.present[i - 4];
        target.monster_alive = true;
        target.died = target.monster_dead_or_escaped = false;
        target.overcast = target.phantasm = false;
        target.updates = 0xbc;
        target.pending.apply = battle.stats[i].pending_status;
        battle.steal_stock[i] = fresh.steal_stock[i];
        if (const auto& monster = preparation.monsters.slots[i - 4]) battle.ai[i] = monster->ai;
        battle.monster_information[i - 4] = fresh.monster_information[i - 4];
        battle.departed_monsters.reset(i - 4);
        battle.untargetable.reset(i);
        battle.away_from_battle.reset(i);
        battle.deferred_death.reset(i);
        battle.arrival_override.reset(i);
        battle.turns[i].atb_stopped = false;
        battle.turns[i].actions_blocked = false;
        battle.turns[i].status_actions_blocked = false;
    }
    for (auto id : preparation.monsters.first_strike_queue) {
        const auto slot = static_cast<std::uint8_t>(id + 4);
        battle.turns[slot].atb_stopped = true;
        battle.turns[slot].atb &= 255U;
        battle.turns[slot].advance_wait = 0xff00;
        auto& queue = battle.advance_schedule;
        queue.entries[queue.write] = slot;
        queue.write = wrap8(unsigned{queue.write} + 1U);
    }
    std::array<BattleStatusContext, 10> contexts;
    for (unsigned i = 0; i < 10; ++i)
        contexts[i] = {battle.stats[i].hp, battle.stats[i].blocked_status};
    update_battle_status(battle.status, contexts);
    resolve_battle_status_consequences(battle);
    refresh_battle_actions(battle, data.commands, data.random, 0);
    update_battle_participation(battle);
    change_battle_approach(battle, formation.formation, preparation.characters.party_relic_effects,
                           load_battle_approach_weights(pack), data.random);
    update_battle_display(battle);
    BattleSceneAppearance appearance;
    appearance.background = battle_entry_->request.background;
    auto entrance = make_battle_entrance(pack, formation, *battle_party_, preparation, battle,
                                         appearance, true);
    battle_menus_->replace_monsters(entrance, formation, preparation, battle);
    battle_formation_ = std::move(formation);
    battle_preparation_ = std::move(preparation);
    battle_state_ = std::move(battle);
}
} // namespace ff6
