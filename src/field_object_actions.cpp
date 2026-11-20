#include "ff6/field_object_actions.hpp"
#include "rom-materials/field_short_jump_counter.macros.inc"
#include "rom-materials/field_long_jump_counter.macros.inc"
#include <algorithm>
#include <sstream>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
void position_priority(FieldObject& actor, const FieldMap& map) {
    actor.occupancy_tile = field_object_cell(map, actor);
    actor.terrain_level = field_terrain_at(map, actor.occupancy_tile).flags & 7U;
    actor.lower.priority = 2;
    actor.upper.priority = actor.terrain_level >= 1 && actor.terrain_level <= 3 ? 2 : 3;
}
bool follows(const FieldObjects& field, std::uint8_t identity) {
    return identity == FieldObjects::camera || field.update_order[0] == identity;
}
std::uint32_t following(ByteView script, std::uint32_t cursor, std::size_t count) {
    (void)slice(script, cursor, count);
    const auto next = std::size_t{cursor} + count;
    require(next < script.size(), "Object action continuation outside resource");
    return static_cast<std::uint32_t>(next);
}
FieldActionResult move(FieldObjects& field, FieldOccupancy& occupancy, std::uint8_t identity,
                       const FieldMap& map, const FieldMoveRates& rates, bool camera_locked) {
    auto& actor = field.objects[identity];
    const auto direction = actor.move_direction;
    require(direction >= 1 && direction <= 16, "Undefined scripted movement direction");
    const bool follow = follows(field, identity);
    require(identity != FieldObjects::camera || camera_locked || field.update_order[0] == identity,
            "Unlocked camera action has unresolved player-counter side effects");
    if (identity != FieldObjects::camera && direction < 5) {
        actor.occupancy_tile = field_object_cell(map, actor);
        constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
        const FieldTilePosition destination{
            static_cast<std::uint8_t>((actor.occupancy_tile.x + dx[direction - 1]) &
                                      (occupancy.width - 1U)),
            static_cast<std::uint8_t>((actor.occupancy_tile.y + dy[direction - 1]) &
                                      (occupancy.height - 1U))};
        if (!(actor.movement.options & 8U)) {
            auto& target =
                occupancy.cells[std::size_t{destination.y} * occupancy.width + destination.x];
            if (!(actor.movement.options & 1U) && target) {
                actor.velocity_x = actor.velocity_y = 0;
                return {FieldActionResult::Kind::blocked, follow, {}};
            }
            // Special/high/low objects still encounter occupants, but do not
            // reserve a destination or change terrain priority on departure.
            if (!actor.special_graphics && actor.draw_group == FieldObject::DrawGroup::normal) {
                target = identity;
                update_field_object_priority_before(actor,
                                                    field_terrain_at(map, destination).flags);
            }
        }
    }
    const auto velocity =
        field_move_velocity(rates, static_cast<FieldMove>(direction), actor.speed);
    actor.velocity_x = velocity[0];
    actor.velocity_y = velocity[1];
    --actor.action_steps;
    return {FieldActionResult::Kind::moving, follow, {}};
}
FieldActionResult visit(FieldState& state, FieldOccupancy& occupancy, std::uint8_t identity,
                        const FieldMap& map, const FieldActionData& data,
                        std::uint8_t& random_index, FieldActionInputs inputs,
                        std::size_t command_budget) {
    auto& actor = state.field.objects[identity];
    const auto script = ByteView(data.script);
    FieldFlagAccess flags(state);
    if (actor.action_wait != 0) {
        --actor.action_wait;
        return {};
    }
    for (std::size_t count = 0;; ++count) {
        if (actor.action_steps != 0 && actor.move_direction != 0)
            return move(state.field, occupancy, identity, map, data.movement, inputs.camera_locked);
        require(count < command_budget, "Object action command budget exhausted without yielding");
        if (!actor.action_script) {
            // Reset initializes the constant words to 0000/FFFF. A newly
            // script-controlled object with no assignment consequently selects
            // pose zero and stops. Express that initialization quirk directly;
            // never fetch a program from original RAM or another resource.
            actor.pending_pose = 0;
            actor.movement.mode = 0;
            actor.velocity_x = actor.velocity_y = 0;
            return {FieldActionResult::Kind::finished, follows(state.field, identity), {}};
        }
        require(*actor.action_script < script.size(), "Object action cursor outside resource");
        const auto cursor = *actor.action_script;
        const auto command = script[cursor];
        const auto next = [&](std::size_t length = 1) {
            actor.action_script = following(script, cursor, length);
        };
        const auto operand = [&] { return slice(script, cursor, 2)[1]; };
        if (command < 0x80) {
            actor.pending_pose = command;
            next();
        } else if (command < 0xa0) {
            actor.facing = command & 3U;
            actor.move_direction = static_cast<std::uint8_t>(actor.facing + 1U);
            actor.action_steps = static_cast<std::uint8_t>(((command - 0x80U) >> 2U) + 1U);
            next();
        } else if (command < 0xac) {
            actor.move_direction = static_cast<std::uint8_t>(command - 0x9bU);
            actor.facing = data.movement.facing[actor.move_direction - 1];
            require(actor.facing < 4, "Invalid scripted movement facing");
            actor.action_steps = 1;
            next();
        } else if (command >= 0xc0 && command <= 0xc5) {
            actor.speed = static_cast<std::uint8_t>(command - 0xc0U);
            next();
        } else if (command >= 0xcc && command <= 0xcf) {
            actor.facing = command & 3U;
            actor.pending_pose = data.poses.standing[actor.facing];
            next();
        } else if (command >= 0xe1 && command <= 0xe6) {
            const unsigned page = (command - 0xe1U) % 3U;
            flags.set(static_cast<std::uint16_t>(page * 256U + operand()), command <= 0xe3);
            next(2);
        } else if (command >= 0xfa && command <= 0xfd) {
            bool branch = true;
            if (command < 0xfc) {
                random_index = static_cast<std::uint8_t>(unsigned{random_index} + 1U);
                branch = data.random[random_index] >= 128;
            }
            if (!branch) {
                next(2);
            } else {
                const auto distance = operand();
                const bool backward = (command & 1U) == 0;
                require(!backward || distance <= cursor, "Object action branch before resource");
                const auto target = backward ? cursor - distance : cursor + distance;
                require(target < script.size(), "Object action branch after resource");
                actor.action_script = target;
            }
        } else {
            switch (command) {
            case 0xc6:
            case 0xc7:
                actor.walking_animation = command == 0xc6;
                next();
                break;
            case 0xc8:
                require(operand() < 4, "Object priority operand exceeds defined modes");
                actor.layer_priority_mode = operand();
                next(2);
                break;
            case 0xc9: {
                const auto vehicle = operand() & 224U;
                actor.ride = static_cast<FieldRide>(static_cast<unsigned>(actor.ride) |
                                                    ((vehicle >> 5U) & 3U));
                actor.rider_visible =
                    actor.rider_visible || actor.special_graphics || (vehicle & 128U);
                actor.special_graphics = actor.ride == FieldRide::none && actor.rider_visible;
                if (actor.special_pose)
                    actor.special_pose->speed = static_cast<std::uint8_t>(actor.ride);
                next(2);
                break;
            }
            case 0xd0:
                // The already-visible original path pops an unpushed CPU stack
                // word. Reject that unresolved precondition rather than invent
                // an emulated-stack consequence or silently claim idempotence.
                require(!actor.membership.visible,
                        "Already-visible object show has an unresolved original stack path");
                actor.membership.visible = true;
                actor.layer_priority_mode = 0;
                actor.upper.priority = actor.lower.priority = 2;
                if (identity < 16)
                    state.roster.membership[identity].visible = true;
                next();
                break;
            case 0xd1:
                hide_field_object(state, occupancy, identity);
                next();
                break;
            case 0xd5:
            case 0xd7: {
                release_field_occupancy(occupancy, std::span(&actor.occupancy_tile, 1));
                if (command == 0xd5) {
                    const auto args = slice(script, cursor, 3);
                    actor.x = unsigned{args[1]} * 4096U;
                    actor.y = unsigned{args[2]} * 4096U;
                    next(3);
                } else {
                    require(state.field.update_order[0].has_value() &&
                                *state.field.update_order[0] <= 48,
                            "Object position copy requires selected player");
                    const auto& player = state.field.objects[*state.field.update_order[0]];
                    require(player.x <= 0xffffff && player.y <= 0xffffff,
                            "Invalid selected player position");
                    actor.x = player.x & 0xffff00U;
                    actor.y = player.y & 0xffff00U;
                    next();
                }
                position_priority(actor, map);
                return {FieldActionResult::Kind::positioned, false, {}};
            }
            case 0xdc:
            case 0xdd:
                actor.jump_counter = command == 0xdc ? FF6_ROM_VALUE_FIELD_SHORT_JUMP_COUNTER_0
                                                     : FF6_ROM_VALUE_FIELD_LONG_JUMP_COUNTER_0;
                next();
                break;
            case 0xe0:
                require(identity != FieldObjects::camera || inputs.camera_locked ||
                            state.field.update_order[0] == identity,
                        "Unlocked camera pause has unresolved player-cursor side effects");
                actor.velocity_x = actor.velocity_y = 0;
                actor.action_wait = operand();
                next(2);
                return {FieldActionResult::Kind::waiting, follows(state.field, identity), {}};
            case 0xf9: {
                if (inputs.event_running || inputs.collision_active)
                    return {FieldActionResult::Kind::event_wait, false, {}};
                const auto target = read24(slice(script, cursor, 4), 1);
                require(target < script.size(), "Object event target outside resource");
                (void)following(script, cursor, 4);
                return {FieldActionResult::Kind::event_request, false, target};
            }
            case 0xff:
                actor.action_script.reset();
                actor.movement.mode = 0;
                actor.velocity_x = actor.velocity_y = 0;
                return {FieldActionResult::Kind::finished, follows(state.field, identity), {}};
            default: {
                std::ostringstream why;
                why << "Unsupported object action 0x" << std::hex << unsigned{command}
                    << " at resource offset 0x" << cursor;
                throw std::invalid_argument(why.str());
            }
            }
        }
    }
}
} // namespace
FieldActionData load_field_action_data(const ContentPack& pack) {
    const auto script = field_event_script(pack), random = pack.get("encounter_rng_tbl_bin");
    require(random.size() == 256, "Invalid field random table extent");
    FieldActionData result;
    result.script.assign(script.begin(), script.end());
    result.movement = load_field_move_rates(pack);
    result.poses = load_field_sprites(pack).selection;
    std::copy(random.begin(), random.end(), result.random.begin());
    return result;
}
FieldActionResult advance_field_object_action(FieldState& state, FieldOccupancy& occupancy,
                                              std::uint8_t identity, const FieldMap& map,
                                              const FieldActionData& data,
                                              std::uint8_t& random_index, FieldActionInputs inputs,
                                              std::size_t command_budget) {
    require(identity <= FieldObjects::camera, "Invalid action object identity");
    require(!data.script.empty() && data.script.size() <= 0x1000000 && command_budget > 0,
            "Invalid object action resource extent or command budget");
    const auto& actor = state.field.objects[identity];
    require(actor.x <= 0xffffff && actor.y <= 0xffffff && actor.x % 4096 == 0 &&
                actor.y % 4096 == 0,
            "Object action visit requires aligned tile position");
    require(actor.layer_priority_mode < 4 && actor.movement.options < 16,
            "Invalid object action options");
    (void)occupancy.at(0, 0);
    require(map.definition.layouts[0].width == occupancy.width &&
                map.definition.layouts[0].height == occupancy.height,
            "Action map and occupancy dimensions disagree");
    auto next = state;
    auto cells = occupancy;
    auto random = random_index;
    const auto result = visit(next, cells, identity, map, data, random, inputs, command_budget);
    state = std::move(next);
    occupancy = std::move(cells);
    random_index = random;
    return result;
}
void consume_field_object_event(FieldState& state, std::uint8_t identity,
                                const FieldActionData& data, FieldEventRunner& events) {
    require(identity <= FieldObjects::camera, "Invalid action event identity");
    require(!data.script.empty() && data.script.size() <= 0x1000000,
            "Invalid object event resource extent");
    auto& actor = state.field.objects[identity];
    require(actor.action_script.has_value(), "Missing action event cursor");
    const auto command = slice(data.script, *actor.action_script, 4);
    require(command[0] == 0xf9, "Action cursor is not an event handoff");
    const auto next = following(data.script, *actor.action_script, 4);
    events.begin_object_event(read24(command, 1));
    actor.action_script = next;
}
} // namespace ff6
