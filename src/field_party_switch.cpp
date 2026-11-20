#include "ff6/field_party_switch.hpp"
#include "rom-materials/field_transition_fade.macros.inc"
#include "rom-materials/field_transition_brightness.macros.inc"
#include "rom-materials/field_transition_step.macros.inc"

namespace ff6 {
// FadeOut's selected fields have separate scalar and packed native consumers.
static_assert(FF6_ROM_VALUE_FIELD_TRANSITION_FADE_0 ==
              (FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0 |
               (FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0 << 8U)));
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
FieldMapLoadPlan reload_plan(const FieldLoopSeed& seed, std::uint8_t leader,
                            FieldTilePosition position, bool startup) {
    const auto& field = seed.state.field;
    const auto& actor = field.objects[leader];
    require(actor.map >= 3 && actor.map < 415, "Party switch requires a field destination");
    FieldMapLoadPlan plan;
    plan.map = actor.map;
    plan.arrival = FieldMapArrival{position, static_cast<FieldFacing>(actor.facing), false, true};
    plan.characters = save_field_character_locations(field);
    plan.parent = field.parent_map;
    plan.reload_same_map = plan.map == field.current_map;
    plan.run_startup = startup;
    return plan;
}
} // namespace
bool switch_field_party(FieldLoopSeed& seed, std::optional<std::uint8_t> target) {
    // Event $7B restores a playable group through ChangeParty even when the
    // script selected another group. Only manual Y input has the 1..3 source
    // restriction (CheckChangeParty).
    require(seed.state.roster.active_party >= 1 && seed.state.roster.active_party < 8 &&
                (target || seed.state.roster.active_party <= 3) &&
                (!target || (*target >= 1 && *target <= 3)), "Invalid switching party");
    require(!seed.transition.pending && seed.state.field.update_order[0] &&
                *seed.state.field.update_order[0] < 16, "Party switch requires an idle field leader");
    auto next = seed;
    auto& field = next.state.field;
    const auto old_id = *field.update_order[0];
    auto& old = field.objects[old_id];
    if (next.state.roster.active_party < next.party_levels.size())
        next.party_levels[next.state.roster.active_party] = next.actions.player_terrain.level;
    auto party = target.value_or(static_cast<std::uint8_t>(next.state.roster.active_party % 3 + 1));
    std::optional<std::uint8_t> leader;
    for (unsigned attempt = 0; attempt < 3 && !leader; ++attempt) {
        unsigned position = 4;
        for (std::uint8_t id = 0; id < 16; ++id) {
            const auto& member = field.objects[id].membership;
            if (member.enabled && member.party == party && member.position < position) {
                leader = id;
                position = member.position;
            }
        }
        if (!leader) party = static_cast<std::uint8_t>(party % 3 + 1);
    }
    require(leader.has_value(), "No enabled party is available");
    next.state.roster.active_party = party;
    next.party_switch_ready = false;
    if (*leader == old_id) {
        seed = std::move(next);
        return false;
    }
    auto& selected = field.objects[*leader];
    selected.movement = selected.saved_movement = old.movement;
    old.movement = old.saved_movement = {};
    old.saved_facing = old.facing;
    field.party_selectors = {*leader, {}, {}, {}};
    // ChangeParty uses the selected actor's cached tile on the current map,
    // and its retained world position for a different field map.
    const auto position = selected.map == field.current_map ? selected.occupancy_tile
        : FieldTilePosition{wrap8(selected.x / 4096U), wrap8(selected.y / 4096U)};
    auto plan = reload_plan(next, *leader, position, true);
    plan.saved_level = next.party_levels[party] & 3U;
    next.transition.pending = plan;
    next.transition.fade = {FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0,
                            FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0, FieldFadeDirection::darker};
    seed = std::move(next);
    return true;
}
bool consume_field_party_control(FieldLoopSeed& seed) {
    const auto& request = seed.events.pending_request();
    if (!request) return false;
    const auto* defaults = std::get_if<EventDefaultParty>(&request->operation);
    const auto* move = std::get_if<EventPartyPosition>(&request->operation);
    if (!defaults && !move) return false;
    auto next = seed;
    if (defaults) {
        if (!defaults->restore) next.actions.contact.default_party = 1;
        else if (next.actions.contact.default_party != next.state.roster.active_party)
            switch_field_party(next, next.actions.contact.default_party);
    } else {
        auto& field = next.state.field;
        for (unsigned id = 0; id < 16; ++id) {
            auto& actor = field.objects[id];
            if (!actor.membership.enabled || actor.membership.party != next.state.roster.active_party)
                continue;
            const auto cell = actor.occupancy_tile;
            if (cell.x < next.actions.occupancy.width && cell.y < next.actions.occupancy.height) {
                auto& occupant = next.actions.occupancy.cells[
                    std::size_t{cell.y} * next.actions.occupancy.width + cell.x];
                if (occupant == id) occupant.reset();
            }
            actor.x = unsigned{move->x} * 4096U;
            actor.y = unsigned{move->y} * 4096U;
            actor.occupancy_tile = {move->x, move->y};
        }
        require(field.update_order[0] && *field.update_order[0] < 16,
                "Party relocation requires a character leader");
        next.transition.pending = reload_plan(next, *field.update_order[0], {move->x, move->y}, false);
    }
    FieldFlagAccess flags(next.state);
    next.events.complete_request(request->identity, flags);
    seed = std::move(next);
    return true;
}
} // namespace ff6
