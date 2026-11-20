#include "ff6/world_map.hpp"
#include "rom-materials/approach_duration.macros.inc"
#include "rom-materials/approach_initial_camera.macros.inc"
#include "rom-materials/world_judgment_reverse_origin.macros.inc"
#include "rom-materials/world_judgment_forward_heading_low.macros.inc"
#include "rom-materials/world_judgment_forward_heading_high.macros.inc"
#include "rom-materials/world_judgment_reverse_heading.macros.inc"
#include "rom-materials/world_judgment_camera_speed.macros.inc"
#include "rom-materials/world_castle_sink_steps.macros.inc"
#include "rom-materials/world_castle_emerge_steps.macros.inc"
#include "rom-materials/world_script_pause_periods.macros.inc"
#include "rom-materials/world_falcon_scene_extent.macros.inc"
#include "rom-materials/world_falcon_scene_speed.macros.inc"
#include "rom-materials/world_script_walk_speed_base.macros.inc"
#include "rom-materials/airship_crash_speed.macros.inc"
#include "rom-materials/world_vehicle_arrow_period.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
std::uint32_t branch(ByteView script, std::uint32_t pc, const FieldFlags& flags) {
    const auto code = slice(script, pc++, 1)[0];
    const unsigned count = (code & 7U) + 1;
    const auto values = slice(script, pc, count * 2 + 3);
    bool jump = code >= 0xb8;
    for (unsigned i = 0; i < count; ++i) {
        const auto condition = read16(values, i * 2);
        require((condition & 32767U) < flags.story.size(), "Invalid world branch flag");
        const bool mismatch = flags.story[condition & 32767U] == ((condition & 32768U) != 0);
        if (code >= 0xb8) jump &= mismatch;
        else jump |= mismatch;
    }
    return jump ? read24(values, count * 2) : pc + count * 2 + 3;
}
WorldEventExit destination(ByteView script, std::uint32_t pc) {
    const auto data = slice(script, pc + 1, 5);
    const auto header = read16(data, 0);
    WorldEventExit out;
    out.entrance.map = header & 511U;
    out.entrance.set_parent = (header & 512U) != 0;
    out.entrance.lower_level = (header & 1024U) != 0;
    out.entrance.show_name = (header & 2048U) != 0;
    out.entrance.facing = (header >> 12U) & 3U;
    out.entrance.destination = {data[2], data[3]};
    out.flags = data[4];
    out.vehicle = static_cast<std::uint8_t>(header >> 14U);
    out.resume = pc + 6;
    return out;
}
void walk(WorldScriptState& e, WorldState& world) {
    // WorldCmd movement runs to a tile boundary before admitting another
    // command. Its speed is in sixteenths of a pixel, not tiles per frame.
    const unsigned distance = e.walk_fraction + e.walk_speed;
    const auto pixels = std::min<unsigned>(e.walk_distance, distance / 16);
    e.walk_fraction = static_cast<std::uint8_t>(distance % 16);
    world.x = static_cast<std::uint16_t>((int(world.x) + e.walk_x * int(pixels)) & 4095);
    world.y = static_cast<std::uint16_t>((int(world.y) + e.walk_y * int(pixels)) & 4095);
    e.walk_distance = static_cast<std::uint16_t>(e.walk_distance - pixels);
    world.walk_clock = wrap8(unsigned{world.walk_clock} + e.walk_speed);
    world.walk_phase = (world.walk_clock >> 5U) & 3U;
}
} // namespace

std::optional<WorldEventExit> resolve_world_event(ByteView script, std::uint32_t entry,
                                                  const FieldFlags& flags) {
    auto pc = entry;
    for (unsigned budget = 0; budget < 1024; ++budget) {
        const auto code = slice(script, pc, 1)[0];
        if (code == 0xff) return {};
        if (code >= 0xac && code <= 0xbf)
            pc = branch(script, pc, flags);
        else if (code == 0xd2 || code == 0xd3)
            return destination(script, pc);
        else
            throw std::runtime_error("Unsupported world event command " + std::to_string(code) +
                                     " at " + std::to_string(pc));
    }
    throw std::runtime_error("World event exceeded its bounded visit");
}

std::optional<WorldEventExit> advance_world_script(ByteView script, WorldScriptState& e,
                                                   WorldState& world) {
    FieldFlags flags;
    return advance_world_script(script, e, world, flags);
}
std::optional<WorldEventExit> advance_world_script(ByteView script, WorldScriptState& e,
                                                   WorldState& world, FieldFlags& flags) {
    e.battle.reset();
    if (e.presentation.approach && e.presentation.approach->frame < FF6_ROM_VALUE_APPROACH_DURATION_0) return {};
    if (e.presentation.crash && !e.presentation.crash->finished) return {};
    if (e.presentation.ruin && !e.presentation.ruin->finished) return {};
    if (e.presentation.judgment && !e.presentation.judgment->finished) return {};
    if (e.presentation.falcon && !e.presentation.falcon->finished) return {};
    if (world.vehicle) world.vehicle->controls = 0;
    if (e.movement_visits) {
        --e.movement_visits;
        if (world.vehicle)
            world.vehicle->controls = slice(script, e.position - 2, 1)[0];
        return {};
    }
    if (e.walk_distance) {
        walk(e, world);
        return {};
    }
    if (e.wait) {
        --e.wait;
        return {};
    }
    if (auto& castle = e.presentation.castle; castle && !castle->finished) {
        // The two directions have different endpoints: sinking advances from
        // y=100 to below 140; emergence includes y=100 and retains the castle
        // when the dust ends. Both move one eighth of a pixel per picture.
        const unsigned last_frame = castle->emerging ? FF6_ROM_VALUE_WORLD_CASTLE_EMERGE_STEPS_0
            : FF6_ROM_VALUE_WORLD_CASTLE_SINK_STEPS_0 - 1;
        if (++castle->frame == last_frame) {
            if (castle->emerging) {
                castle->finished = true;
            } else {
                castle.reset();
            }
        }
        return {};
    }
    for (unsigned budget = 0; budget < 64; ++budget) {
        const auto code = slice(script, e.position, 1)[0];
        if (code == 0xff) return {};
        if (code == 0xd2 || code == 0xd3) return destination(script, e.position);
        if (code >= 0xb0 && code <= 0xbf) {
            e.position = branch(script, e.position, flags);
            continue;
        }
        ++e.position;
        if (code == 0xc8 || code == 0xc9) {
            const auto flag = read16(slice(script, e.position, 2), 0);
            require(flag < flags.story.size(), "Invalid authored world flag");
            flags.story[flag] = code == 0xc8;
            e.position += 2;
        } else if (world.vehicle && code < 0x80) {
            // VehicleCmd_00 applies controls on the initial visit AND the
            // terminal zero-distance visit: operand N means N+1 updates.
            e.movement_visits = slice(script, e.position++, 1)[0];
            world.vehicle->controls = code;
            return {};
        } else if (world.vehicle && code >= 0xc0 && code <= 0xc6) {
            auto& v = *world.vehicle;
            const unsigned value = code == 0xc0 ? slice(script, e.position, 1)[0]
                                          : read16(slice(script, e.position, 2), 0);
            e.position += code == 0xc0 ? 1 : 2;
            if (code == 0xc0) v.movement_flags = static_cast<std::uint8_t>(value);
            else if (code == 0xc1) {
                require(value < 360, "Invalid vehicle viewing direction");
                v.heading = value * 256U + v.heading % 256;
            } else if (code == 0xc2) {
                require(value < 360, "Invalid vehicle movement direction");
                v.movement_heading = static_cast<std::uint16_t>(value);
            } else if (code == 0xc3) {
                const int lean = value < 32768 ? int(value) : int(value) - 65536;
                require(lean >= -224 && lean <= 224, "Invalid authored camera lean");
                v.camera_lean = static_cast<std::int16_t>(lean);
            } else if (code == 0xc4) {
                require(value && value <= 224, "Invalid authored camera extent");
                v.camera_ground_rows = static_cast<std::uint16_t>(value);
            } else if (code == 0xc5) v.altitude = static_cast<std::uint16_t>(value);
            else if (code == 0xc6) v.speed = static_cast<std::uint16_t>(value);
        } else if ((world.vehicle && code == 0xc7) ||
                   (!world.vehicle && code >= 0xc5 && code <= 0xc7)) {
            const auto data = slice(script, e.position, 2);
            e.parked_airship = FieldTilePosition{data[0], data[1]};
            e.position += 2;
        } else if (world.vehicle && code == 0xfe) {
            e.presentation.approach.emplace();
            world.minimap = false;
            world.vehicle->heading = FF6_ROM_VALUE_APPROACH_INITIAL_CAMERA_0 * 256;
            world.vehicle->altitude = FF6_ROM_VALUE_APPROACH_INITIAL_CAMERA_1;
            world.vehicle->turn = world.vehicle->climb = 0;
            world.vehicle->speed = 0;
            return {};
        } else if (world.vehicle && code >= 0xca && code <= 0xcf) {
            const auto args = slice(script, e.position, 2);
            e.position += 2;
            e.battle = {{args[0], args[1]}};
            return {};
        } else if (world.vehicle && code >= 0xda && code <= 0xdc) {
            auto& v = *world.vehicle;
            if (code == 0xda) {
                v.arrows = v.choose_direction = true;
                v.arrow_clock &= FF6_ROM_VALUE_WORLD_VEHICLE_ARROW_PERIOD_0 - 1;
            } else if (code == 0xdb) v.choose_direction = false;
            else v.arrows = false;
        } else if (world.vehicle && code == 0xde) {
            const auto data = slice(script, e.position, 2);
            e.position += 2;
            world.vehicle->center_x = data[0];
            world.vehicle->center_y = data[1];
        } else if (world.vehicle && (code == 0xf3 || code == 0xf5)) {
            auto& shot = e.presentation.judgment.emplace();
            shot.reverse = code == 0xf3;
            if (shot.reverse) {
                shot.start_x = FF6_ROM_VALUE_WORLD_JUDGMENT_REVERSE_ORIGIN_0;
                shot.start_y = FF6_ROM_VALUE_WORLD_JUDGMENT_REVERSE_ORIGIN_1;
                shot.end_x = FF6_ROM_VALUE_WORLD_JUDGMENT_REVERSE_ORIGIN_2;
            }
            world.minimap = false;
            auto& v = *world.vehicle;
            v.picture.reset(); v.controls = 0; v.turn = v.climb = 0;
            v.heading = (shot.reverse ? FF6_ROM_VALUE_WORLD_JUDGMENT_REVERSE_HEADING_0
                : FF6_ROM_VALUE_WORLD_JUDGMENT_FORWARD_HEADING_LOW_0 +
                  FF6_ROM_VALUE_WORLD_JUDGMENT_FORWARD_HEADING_HIGH_0 * 256U) * 256;
            v.speed = FF6_ROM_VALUE_WORLD_JUDGMENT_CAMERA_SPEED_0;
            return {};
        } else if (world.vehicle && code == 0xf4) {
            e.presentation.falcon_graphics = true;
        } else if (world.vehicle && code == 0xf7) {
            e.presentation.pigeon = true;
        } else if (world.vehicle && code == 0xfa) {
            e.presentation.falcon.emplace();
            world.minimap = false;
            world.vehicle->picture.reset();
            world.vehicle->speed = FF6_ROM_VALUE_WORLD_FALCON_SCENE_SPEED_0;
            world.vehicle->camera_ground_rows = FF6_ROM_VALUE_WORLD_FALCON_SCENE_EXTENT_0;
            world.vehicle->center_y = FF6_ROM_VALUE_WORLD_FALCON_SCENE_EXTENT_0;
            return {};
        } else if (world.vehicle && code == 0xf8) {
            e.presentation.ruin.emplace();
            world.minimap = false;
            world.vehicle->picture.reset();
            world.vehicle->controls = 0;
            world.vehicle->speed = 0;
            return {};
        } else if (world.vehicle && code == 0xfb) {
            e.presentation.smoke.emplace();
        } else if (world.vehicle && code == 0xfc) {
            e.presentation.smoke.reset();
            e.presentation.crash.emplace();
            world.vehicle->picture.reset();
            world.vehicle->speed = FF6_ROM_VALUE_AIRSHIP_CRASH_SPEED_0;
            world.vehicle->controls = 0;
            return {};
        } else if (world.vehicle && code == 0xfd) {
            // VehicleCmd_fd selects the Esper picture; it does not replace the
            // authored steering, perspective or movement continuation.
            world.vehicle->esper = true;
        } else if (code == 0xd0 || code == 0xd1) {
            e.presentation.actor_visible = code == 0xd0;
        } else if (code >= 0xd6 && code <= 0xd9) {
            e.presentation.target_brightness = code == 0xd9 ? 0 : 15;
        } else if (code == 0xdd || code == 0xdf || (!world.vehicle && code >= 0xda && code <= 0xde)) {
            world.minimap = code >= 0xde;
            if (world.vehicle && code == 0xdd) return {};
        } else if (code == 0xe0) {
            const unsigned count = slice(script, e.position++, 1)[0];
            e.wait = static_cast<std::uint16_t>(FF6_ROM_VALUE_WORLD_SCRIPT_PAUSE_PERIODS_0 - 1 +
                ((count ? count : 256) - 1) * FF6_ROM_VALUE_WORLD_SCRIPT_PAUSE_PERIODS_1);
            return {};
        } else if (!world.vehicle && !world.chocobo && code >= 0x80 && code <= 0xab) {
            unsigned steps = 1;
            if (code < 0xa0) {
                constexpr std::array<std::int8_t, 4> dx{0, 1, 0, -1}, dy{-1, 0, 1, 0};
                e.walk_x = dx[code & 3U];
                e.walk_y = dy[code & 3U];
                world.facing = code & 3U;
                steps = ((code >> 2U) & 7U) + 1;
            } else {
                constexpr std::array<std::int8_t, 12> dx{1, 1, -1, -1, 1, 2, 2, 1, -1, -2, -2, -1};
                constexpr std::array<std::int8_t, 12> dy{-1, 1, 1, -1, -2, -1, 1, 2, 2, 1, -1, -2};
                const unsigned i = code - 0xa0;
                e.walk_x = dx[i]; e.walk_y = dy[i];
                world.facing = static_cast<std::uint8_t>(std::abs(e.walk_x) == 2
                    ? (e.walk_x > 0 ? 1 : 3) : (e.walk_y > 0 ? 2 : 0));
            }
            e.walk_distance = static_cast<std::uint16_t>(steps * 16);
            e.presentation.pose.reset();
            walk(e, world);
            return {};
        } else if (!world.vehicle && code >= 0xc0 && code <= 0xc4) {
            e.walk_speed = static_cast<std::uint8_t>(static_cast<unsigned>(FF6_ROM_VALUE_WORLD_SCRIPT_WALK_SPEED_BASE_0) << (code - 0xc0));
        } else if (!world.vehicle && code >= 0xcc && code <= 0xcf) {
            world.facing = code - 0xcc;
        } else if (!world.vehicle && code < 0x80) {
            e.presentation.pose = code;
        } else if (!world.vehicle && code == 0xfc) {
            e.presentation.ship = true;
        } else if (!world.vehicle && (code == 0xfd || code == 0xfe)) {
            e.presentation.castle = WorldCastleAnimation{0, code == 0xfe, false};
            return {};
        } else {
            throw std::runtime_error("Unsupported authored world command " + std::to_string(code) +
                                     " at " + std::to_string(e.position - 1));
        }
    }
    throw std::runtime_error("Authored world script exceeded its bounded visit");
}
} // namespace ff6
