#include "ff6/field_player_effects.hpp"
#include "rom-materials/field_poison_mosaic_speed.macros.inc"
#include "rom-materials/field_poison_mosaic_start.macros.inc"
#include <algorithm>

namespace ff6 {
std::uint8_t advance_field_mosaic(FieldStepEffects& effects) {
    if (!effects.mosaic_speed) return 1;
    const auto decrement = unsigned{wrap8(unsigned{effects.mosaic_speed} + 1U)} * 16U;
    if (effects.mosaic_counter < decrement) {
        effects.mosaic_counter = effects.mosaic_speed = 0;
        return 1;
    }
    effects.mosaic_counter = static_cast<std::uint16_t>(effects.mosaic_counter - decrement);
    const auto phase = unsigned{effects.mosaic_counter} >> 8U;
    if (phase > 30) throw std::invalid_argument("Invalid field mosaic phase");
    return static_cast<std::uint8_t>(1U + (phase <= 15 ? phase : 30U - phase));
}
namespace {
void require(bool value, const char* reason) {
    if (!value)
        throw std::invalid_argument(reason);
}
void step_vitals(Roster& roster, FieldStepEffects& effects) {
    for (std::size_t id = 0; id < roster.characters.size(); ++id) {
        const auto& membership = roster.membership[id];
        if (!membership.enabled || membership.party != roster.active_party)
            continue;
        auto& character = roster.characters[id];
        // The relic tests its identity, not the aggregated equipment-effect bit.
        // Zombie, petrify and wounded suppress healing; poison does not.
        if (!(character.status & 0xc2U) &&
            (character.equipment[4] == 229 || character.equipment[5] == 229)) {
            const auto maximum = maximum_vital(character.hp, 9999);
            const auto healed =
                wrap16(unsigned{character.hp.current} + (unsigned{character.attributes[2]} >> 2U));
            character.hp.current = std::min(healed, maximum);
        }
        if (character.status & 4U) {
            // DoPoisonDmg's selected pulse settings remain private ROM inputs.
            effects.mosaic_speed = FF6_ROM_VALUE_FIELD_POISON_MOSAIC_SPEED_0;
            effects.mosaic_counter = FF6_ROM_VALUE_FIELD_POISON_MOSAIC_START_0;
            const auto damage = maximum_vital(character.hp, 9999) >> 5U;
            character.hp.current = character.hp.current <= damage
                                       ? 1
                                       : static_cast<std::uint16_t>(character.hp.current - damage);
        }
    }
}
FieldTilePosition adjacent(const FieldOccupancy& grid, FieldTilePosition position,
                           std::uint8_t facing) {
    require(facing < 4, "Invalid interaction facing");
    constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
    return {static_cast<std::uint8_t>((position.x + dx[facing]) & (grid.width - 1U)),
            static_cast<std::uint8_t>((position.y + dy[facing]) & (grid.height - 1U))};
}
bool same_level(std::uint8_t terrain, std::uint8_t player, std::uint8_t target) {
    switch (terrain & 7U) {
    case 1:
        return (target & 7U) != 2;
    case 2:
        return (target & 2U) != 0;
    case 3:
        return (target & 4U) == 0;
    default:
        return player == target || (player == 1 && (target & 4U));
    }
}
std::optional<std::uint8_t> talk_target(FieldState& state, const FieldActionContext& context,
                                        const FieldMap& map) {
    const auto player_id = state.field.update_order[0];
    require(player_id && *player_id <= FieldObjects::camera, "Missing interaction player");
    auto& player = state.field.objects[*player_id];
    require(player.facing < 4, "Invalid player interaction facing");
    if (player.movement.mode != 2)
        return {};
    const auto& grid = context.occupancy;
    (void)grid.at(0, 0);
    require(grid.width == map.definition.layouts[0].width &&
                grid.height == map.definition.layouts[0].height,
            "Interaction map and occupancy disagree");
    player.occupancy_tile = field_object_cell(map, player);
    auto position = adjacent(grid, player.occupancy_tile, player.facing);
    const auto near = grid.at(position.x, position.y);
    auto through_facing = player.facing;
    if (near && *near != *player_id) {
        require(*near <= FieldObjects::camera, "Invalid adjacent object identity");
        const auto& target = state.field.objects[*near];
        if (target.movement.options & 4U)
            return {};
        if (same_level(context.player_tile.flags, context.player_terrain.level,
                       target.terrain_level))
            return near;
        // The source retains the rejected object's identity here. Its facing,
        // rather than the player's, controls the second counter lookup.
        through_facing = target.facing;
    }
    const auto flags = field_terrain_at(map, position).flags;
    if (flags == 0xf7 || (flags & 7U) != 7)
        return {};
    position = adjacent(grid, position, through_facing);
    const auto far = grid.at(position.x, position.y);
    require(!far || *far <= FieldObjects::camera, "Invalid counter object identity");
    // This path intentionally has no second elevation, collision-option or
    // self-identity test. Adding those would change original counter behavior.
    return far;
}
} // namespace
bool apply_world_step_effects(Roster& roster, FieldStepEffects& effects) {
    if (effects.steps != 9999999) effects.steps = (effects.steps + 1U) & 0xffffffU;
    auto pulse = effects;
    pulse.mosaic_counter = 0;
    step_vitals(roster, pulse);
    return pulse.mosaic_counter != 0;
}
void apply_field_step_effects(FieldState& state, FieldStepEffects& effects, FieldStep::Kind kind) {
    require(kind == FieldStep::Kind::cardinal || kind == FieldStep::Kind::diagonal,
            "Step effects require a successful player step");
    require(effects.steps <= 0xffffff, "Invalid 24-bit step counter");
    auto next = state;
    auto result = effects;
    // Original checks equality with the limit, not a saturating >= comparison.
    if (result.steps != 9999999)
        result.steps = (result.steps + 1U) & 0xffffffU;
    if (kind == FieldStep::Kind::cardinal)
        step_vitals(next.roster, result);
    if (next.roster.active_party >= 1 && next.roster.active_party <= 3) {
        const auto start = 0x2c4U + (unsigned{next.roster.active_party} - 1U) * 4U;
        for (unsigned bit = start; bit < start + 4; ++bit)
            FieldFlagAccess(next).set(static_cast<std::uint16_t>(bit), false);
    }
    if (kind == FieldStep::Kind::cardinal) {
        FieldFlagAccess(next).set(0x1b5, false);
        FieldFlagAccess(next).set(0x1bf, false);
    }
    result.entrances_disabled = false;
    result.encounters_enabled = true;
    result.on_trigger = false;
    state = std::move(next);
    effects = result;
}
std::optional<std::uint8_t> try_field_talk(FieldState& state, const FieldActionContext& context,
                                           const FieldMap& map, const FieldPoseTables& poses,
                                           FieldEventRunner& events, std::uint8_t caller,
                                           FieldTalkInputs inputs) {
    if (!inputs.fully_bright || inputs.menu_opening || inputs.map_loading || inputs.event_running ||
        inputs.dialogue_open || !inputs.confirm_down)
        return {};
    require(caller <= FieldObjects::camera, "Invalid interaction caller");
    auto next = state;
    const auto identity = talk_target(next, context, map);
    if (!identity || next.field.objects[*identity].movement.mode == 4) {
        state = std::move(next); // A failed lookup still refreshes the player cache.
        return {};
    }
    auto& target = next.field.objects[*identity];
    auto& player = next.field.objects[caller];
    require(!target.special_graphics, "Special graphics do not own a decoded interaction script");
    require(target.facing < 4 && player.facing < 4 && target.movement.mode < 16 &&
                target.movement.options < 16 && player.movement.mode < 16 &&
                player.movement.options < 16,
            "Invalid activated object state");
    // Only the old low movement nibble is saved for the target. The caller's
    // complete mode/options are saved later, including when identities alias.
    target.saved_movement = {target.movement.mode, 0};
    const auto opposite = static_cast<std::uint8_t>((player.facing + 2U) & 3U);
    target.saved_facing = target.facing;
    if (!(target.movement.options & 2U)) {
        target.facing = opposite;
        target.pending_pose = poses.standing[opposite];
    }
    target.movement.mode = 4;
    auto event = events;
    event.begin_object_event(target.interaction_data);
    player.saved_movement = player.movement;
    player.movement = {4, 0};
    state = std::move(next);
    events = std::move(event);
    return identity;
}
} // namespace ff6
