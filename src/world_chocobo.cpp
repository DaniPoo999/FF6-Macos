#include "ff6/world_map.hpp"
#include "rom-materials/chocobo_dismount_duration.macros.inc"
#include "rom-materials/chocobo_turn_parameters.macros.inc"
#include "rom-materials/chocobo_speed_parameters.macros.inc"
#include "rom-materials/chocobo_animation_cadence_range.macros.inc"
#include "rom-materials/chocobo_pose_thresholds.macros.inc"
#include "rom-materials/chocobo_turn_pose_offsets.macros.inc"
#include <algorithm>

namespace ff6 {
void enter_world_chocobo(WorldState& world) {
    world.chocobo.emplace();
    // InitChoco selects north/south from the map header's upper facing bit.
    // The source masks $6000 and multiplies the result by 90 degrees.
    world.chocobo->heading = std::uint32_t{world.facing & 2U} * 90U * 256U;
    world.x = static_cast<std::uint16_t>((world.x & 4080U) + 8);
    world.y = static_cast<std::uint16_t>((world.y & 4080U) + 8);
}
bool advance_world_chocobo(const WorldAssets& assets, WorldState& world, Buttons buttons) {
    auto& c = world.chocobo.value();
    if (c.dismount) {
        ++world.clock;
        // The duration derives from the original phase delay and bird travel
        // endpoints. Its two-pixel increment is explicit source arithmetic.
        return ++*c.dismount == FF6_ROM_VALUE_CHOCOBO_DISMOUNT_DURATION_0;
    }
    if (held(buttons, Button::b)) {
        c.dismount = 0;
        c.speed = 0;
        c.turn = 0;
        ++world.clock;
        return false;
    }
    // EE/GetChocoInput selects a symmetric turn bound and four matching
    // acceleration/decay steps. The ROM recipes authenticate those selections.
    int turn = c.turn;
    if (held(buttons, Button::right))
        turn = std::max(-FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_0,
                        turn - FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_1);
    else if (held(buttons, Button::left))
        turn = std::min(FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_0,
                        turn + FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_1);
    else if (turn > 0)
        turn -= FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_1;
    else if (turn < 0)
        turn += FF6_ROM_VALUE_CHOCOBO_TURN_PARAMETERS_1;
    c.turn = static_cast<std::int16_t>(turn);
    c.speed = static_cast<std::uint16_t>(held(buttons, Button::up) || held(buttons, Button::a)
                                             ? std::min(FF6_ROM_VALUE_CHOCOBO_SPEED_PARAMETERS_0 - 1,
                                                        int(c.speed) + FF6_ROM_VALUE_CHOCOBO_SPEED_PARAMETERS_1)
                                             : std::max(0, int(c.speed) - FF6_ROM_VALUE_CHOCOBO_SPEED_PARAMETERS_2));
    int heading = static_cast<int>(c.heading) + turn;
    if (held(buttons, Button::l) && held(buttons, Button::left))
        heading += 512;
    if (held(buttons, Button::r) && held(buttons, Button::right))
        heading -= 512;
    c.heading = static_cast<std::uint32_t>((heading + 360 * 256) % (360 * 256));
    const unsigned angle = c.heading / 256;
    const auto x = world.x, y = world.y;
    const auto fx = c.fraction_x, fy = c.fraction_y;
    const auto move = [&](unsigned direction) {
        const auto sine = assets.sine.at(direction % 180);
        const auto cosine = assets.sine.at(direction % 180 + 90);
        const int dx = int(unsigned{c.speed} * sine / 256) * (direction < 180 ? -1 : 1);
        const int dy =
            int(unsigned{c.speed} * cosine / 256) * (direction < 90 || direction >= 270 ? -1 : 1);
        const unsigned nx = static_cast<unsigned>(int(x) * 256 + fx + dx) & 0xfffffU;
        const unsigned ny = static_cast<unsigned>(int(y) * 256 + fy + dy) & 0xfffffU;
        world.x = static_cast<std::uint16_t>(nx / 256);
        world.y = static_cast<std::uint16_t>(ny / 256);
        if (world_tile_properties(assets, world) & 1U) {
            world.x = x;
            world.y = y;
            return false;
        }
        c.fraction_x = static_cast<std::uint8_t>(nx & 255U);
        c.fraction_y = static_cast<std::uint8_t>(ny & 255U);
        return true;
    };
    // FixChocoboCollision tries perpendicular motion in a quadrant-dependent
    // order. It restores the viewing direction, including on blocked attempts.
    if (!move(angle) && angle % 90 != 0) {
        const unsigned side = angle % 90 < 45 ? 90 : 270;
        if (!move((angle + side) % 360))
            move((angle + 360 - side) % 360);
    }
    if (!c.turn && !c.speed) {
        c.phase = c.frame = FF6_ROM_VALUE_CHOCOBO_IDLE_FRAME_0;
        c.frame_wait = c.period;
    } else {
        c.period = static_cast<std::uint8_t>(FF6_ROM_VALUE_CHOCOBO_ANIMATION_CADENCE_RANGE_0 -
            (c.turn ? 0 : c.speed * 4 / 256));
        if (c.frame_wait)
            --c.frame_wait;
        else {
            c.phase = static_cast<std::uint8_t>(c.phase + 1 >= FF6_ROM_VALUE_CHOCOBO_ANIMATION_CADENCE_RANGE_1
                ? FF6_ROM_VALUE_CHOCOBO_ANIMATION_CADENCE_RANGE_2 : c.phase + 1);
            c.frame = static_cast<std::uint8_t>(c.phase +
                (c.turn >= FF6_ROM_VALUE_CHOCOBO_POSE_THRESHOLDS_0
                    ? FF6_ROM_VALUE_CHOCOBO_TURN_POSE_OFFSETS_0
                    : c.turn < static_cast<std::int16_t>(FF6_ROM_VALUE_CHOCOBO_POSE_THRESHOLDS_1)
                        ? FF6_ROM_VALUE_CHOCOBO_TURN_POSE_OFFSETS_1 : 0));
            c.frame_wait = c.period;
        }
    }
    ++world.clock;
    return false;
}
} // namespace ff6
