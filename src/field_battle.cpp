#include "ff6/field_battle.hpp"
#include "ff6/field_loop.hpp"
#include <algorithm>
#include "rom-materials/field_battle_mosaic_phase.macros.inc"
#include "rom-materials/field_battle_mosaic_small.macros.inc"
#include "rom-materials/field_battle_mosaic_large.macros.inc"
#include "rom-materials/encounter_counter_cap.macros.inc"
#include "rom-materials/encounter_index_step.macros.inc"
#include "rom-materials/formation_index_step.macros.inc"
#include "rom-materials/event_battle_choice.macros.inc"
#include "rom-materials/random_battle_cutoffs.macros.inc"
#include "rom-materials/colosseum_battle_formations.macros.inc"
#include "rom-materials/colosseum_battle_background.macros.inc"

namespace ff6 {
FieldBattleData decode_field_battle_data(ByteView random, ByteView groups) {
    if (random.size() != 256 || groups.size() != 1024)
        throw std::invalid_argument("Invalid field battle table extent");
    FieldBattleData result;
    std::ranges::copy(random, result.random.begin());
    for (std::size_t group = 0; group < result.event_groups.size(); ++group)
        for (std::size_t choice = 0; choice < 2; ++choice)
            result.event_groups[group][choice] = read16(groups, group * 4 + choice * 2);
    return result;
}
FieldBattleData load_field_battle_data(const ContentPack& pack) {
    auto out = decode_field_battle_data(pack.get("encounter_rng_tbl_bin"),
                                        pack.get("encounter_event_battle_group_bin"));
    const auto groups = pack.get("encounter_rand_battle_group_bin"),
               maps = pack.get("encounter_sub_battle_group_bin"),
               rates = pack.get("field_encounter_rates"),
               map_rates = pack.get("data_cf5800_battle_groups");
    if (groups.size() != 2048 || maps.size() != 512 || rates.size() != 32 ||
        map_rates.size() != 256)
        throw std::invalid_argument("Invalid field encounter tables");
    for (unsigned i = 0; i < 256; ++i)
        for (unsigned j = 0; j < 4; ++j)
            out.random_groups[i][j] = read16(groups, i * 8 + j * 2);
    for (unsigned i = 0; i < 512; ++i) {
        out.map_groups[i] = maps[i];
        out.map_rates[i] =
            static_cast<std::uint8_t>((map_rates[128 + i / 4] >> ((i % 4) * 2)) & 3U);
    }
    for (unsigned i = 0; i < 16; ++i)
        out.rates[i] = read16(rates, i * 2);
    return out;
}
FieldBattleRequest select_field_battle(const FieldBattleData& data, const EventStartBattle& command,
                                       const FieldBattleContext& context,
                                       FieldBattleRandom& random) {
    if (command.background > 63 ||
        (command.kind != EventBattleKind::scripted && command.kind != EventBattleKind::contact &&
         command.kind != EventBattleKind::treasure))
        throw std::invalid_argument("Invalid field battle command");
    const bool chest = command.kind == EventBattleKind::treasure;
    const auto group = chest ? context.treasure_group : command.group;
    const auto index = wrap8(unsigned{random.formation_index} + 1U);
    // Formation and encounter streams have independently selected increments
    // (UpdateBattleGrpRng/UpdateBattleRng). Widen before explicit byte wrapping.
    const auto offset = wrap8(unsigned{random.formation_offset} +
                             (index == 0 ? unsigned{FF6_ROM_VALUE_FORMATION_INDEX_STEP_0} : 0U));
    const auto choice = wrap8(unsigned{data.random[index]} + offset) >=
                        FF6_ROM_VALUE_EVENT_BATTLE_CHOICE_0 ? 1U : 0U;
    const auto background = chest ? 63U : command.background;
    FieldBattleRequest result;
    result.formation = data.event_groups[group][choice];
    result.background =
        static_cast<std::uint8_t>(background == 63 ? context.map_background & 127U : background);
    result.mosaic = chest || command.mosaic;
    result.sound = chest || command.sound;
    result.continue_music = context.continue_music;
    result.field_return.scroll =
        command.kind == EventBattleKind::contact ? context.contact : context.scroll;
    result.field_return.party =
        command.kind == EventBattleKind::contact ? context.contact : context.party;
    result.field_return.keep_dimensions = command.kind == EventBattleKind::scripted;
    random.formation_index = index;
    random.formation_offset = offset;
    return result;
}
bool queue_field_battle_request(FieldLoopSeed& seed, const FieldBattleData& data,
                                std::uint8_t map_background) {
    if (!seed.events.pending_request())
        return false;
    const auto* command = std::get_if<EventStartBattle>(&seed.events.pending_request()->operation);
    if (!command)
        return false;
    if (seed.battle)
        throw std::logic_error("A field battle is already pending");
    auto next = seed;
    if (command->kind == EventBattleKind::colosseum) {
        if (!next.colosseum || next.colosseum->actor == 255 || next.colosseum->wager == 255)
            throw std::logic_error("Coliseum battle requires a completed wager menu");
        FieldBattleRequest request;
        request.formation = next.colosseum->shadow ? FF6_ROM_VALUE_COLOSSEUM_BATTLE_FORMATIONS_0
                                                   : FF6_ROM_VALUE_COLOSSEUM_BATTLE_FORMATIONS_1;
        request.background = FF6_ROM_VALUE_COLOSSEUM_BATTLE_BACKGROUND_0;
        request.mosaic = request.sound = false;
        request.continue_music = next.state.flags.story[0x2bc];
        request.field_return.scroll = {next.actions.scroll.layers[0][0].center, next.actions.scroll.layers[0][1].center};
        request.field_return.party = {next.actions.player_tile_position[0], next.actions.player_tile_position[1]};
        next.state.flags.story[0x2bb] = next.colosseum->shadow;
        next.battle = request;
    } else if (command->kind == EventBattleKind::random) {
        if (!next.selected_encounter)
            throw std::logic_error("Random battle command has no selected encounter");
        next.battle = next.selected_encounter;
        next.selected_encounter.reset();
    } else {
        if (next.selected_encounter)
            throw std::logic_error("Another battle cannot replace a regrouping encounter");
        const FieldBattleContext context{
            {seed.actions.scroll.layers[0][0].center, seed.actions.scroll.layers[0][1].center},
            {seed.actions.player_tile_position[0], seed.actions.player_tile_position[1]},
            seed.actions.contact.scroll_target,
            map_background,
            seed.player.treasure.battle_formation,
            seed.state.flags.story[0x2bc]};
        next.battle = select_field_battle(data, *command, context, next.battle_random);
    }
    FieldFlagAccess flags(next.state);
    next.events.complete_request(next.events.pending_request()->identity, flags);
    seed = std::move(next);
    return true;
}
bool check_field_random_battle(FieldLoopSeed& seed, const FieldBattleData& data,
                               std::uint8_t background) {
    const auto map = seed.state.field.current_map;
    if (map >= data.map_groups.size() || seed.battle || seed.selected_encounter)
        throw std::invalid_argument("Invalid random battle context");
    seed.player.effects.encounters_enabled = false;
    const auto rate =
        data.rates[(seed.state.field.equipment_effects & 3U) * 4U + data.map_rates[map]];
    if (!rate)
        return false;
    const unsigned counter = unsigned{seed.encounter_counter} + rate;
    seed.encounter_counter = counter > 65535 ? FF6_ROM_VALUE_ENCOUNTER_COUNTER_CAP_0
                                           : static_cast<std::uint16_t>(counter);
    auto& random = seed.battle_random;
    random.encounter_index = wrap8(unsigned{random.encounter_index} + 1);
    if (!random.encounter_index)
        random.encounter_offset = wrap8(unsigned{random.encounter_offset} + FF6_ROM_VALUE_ENCOUNTER_INDEX_STEP_0);
    if (wrap8(unsigned{data.random[random.encounter_index]} + random.encounter_offset) >=
        (seed.encounter_counter >> 8U))
        return false;
    seed.encounter_counter = 0;
    random.formation_index = wrap8(unsigned{random.formation_index} + 1);
    if (!random.formation_index)
        random.formation_offset = wrap8(unsigned{random.formation_offset} + FF6_ROM_VALUE_FORMATION_INDEX_STEP_0);
    const auto value =
        wrap8(unsigned{data.random[random.formation_index]} + random.formation_offset);
    FieldBattleRequest request;
    request.formation = data.random_groups[data.map_groups[map]][
        value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_0 ? 0
        : value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_1 ? 1
        : value < FF6_ROM_VALUE_RANDOM_BATTLE_CUTOFFS_2 ? 2 : 3];
    request.background = background & 127U;
    request.continue_music = seed.state.flags.story[0x2bc];
    request.field_return.scroll = {seed.actions.scroll.layers[0][0].center,
                                   seed.actions.scroll.layers[0][1].center};
    request.field_return.party = {seed.actions.player_tile_position[0],
                                  seed.actions.player_tile_position[1]};
    // CheckBattleSub sets only RUN_STARTUP ($80), unlike the scripted callers
    // which also suppress automatic fade-in. The RandBattle event does not
    // issue a fade of its own, so reusing those callers leaves a black field.
    request.field_return.fade_in = true;
    seed.events.begin_object_event(data.random_event);
    const auto leader = seed.state.field.update_order[0];
    if (!leader || *leader >= seed.state.field.objects.size())
        throw std::invalid_argument("Random encounter needs a selected field object");
    auto& actor = seed.state.field.objects[*leader];
    actor.velocity_x = actor.velocity_y = 0;
    actor.saved_movement = actor.movement;
    actor.movement = {4, 0};
    for (auto& layer : seed.actions.scroll.layers)
        for (auto& axis : layer)
            axis.follow_rate = 0;
    seed.selected_encounter = request;
    return true;
}
std::uint8_t FieldBattleTransition::advance() {
    if (complete())
        throw std::logic_error("Battle transition has no remaining wait");
    ++waits_;
    // BattleMosaic's selected width masks become periods; the returned sample
    // width adds the format's implicit first pixel after the encoded size.
    const auto size = waits_ < FF6_ROM_VALUE_FIELD_BATTLE_MOSAIC_PHASE_0
        ? waits_ % (unsigned{FF6_ROM_VALUE_FIELD_BATTLE_MOSAIC_SMALL_0} + 1U)
        : waits_ % (unsigned{FF6_ROM_VALUE_FIELD_BATTLE_MOSAIC_LARGE_0} + 1U);
    return static_cast<std::uint8_t>(size + 1U);
}
FieldBattleEntry prepare_field_battle_entry(FieldLoopSeed& seed) {
    if (!seed.battle)
        throw std::logic_error("No field battle to enter");
    const auto leader = seed.state.field.update_order[0];
    if (!leader || *leader >= seed.state.field.objects.size())
        throw std::invalid_argument("Battle entry needs a selected field object");
    FieldBattleEntry result{*seed.battle, *leader, seed.state.field.objects[*leader].facing,
                            seed.actions.player_terrain.level};
    save_field_membership(seed.state);
    // InitBattle copies persistent switches into AI bytes 4..23. Native
    // storage omits those four local bytes: command flags are bytes 7/8,
    // corresponding to C2/534F, C2/540B and UpdateMorph C2/0B01.
    const auto flags = seed.state.battle_variables[7];
    seed.battle_command_rules.magic_only = (flags & 1U) != 0;
    seed.battle_command_rules.double_morph_duration = (flags & 2U) != 0;
    seed.battle_command_rules.permanent_morph = (flags & 4U) != 0;
    seed.battle_command_rules.morph_available = (seed.state.battle_variables[8] & 4U) != 0;
    seed.travel.saved_level = result.terrain_level;
    return result;
}
} // namespace ff6
