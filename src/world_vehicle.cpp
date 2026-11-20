#include "ff6/world_map.hpp"
#include <algorithm>
#include "rom-materials/world_vehicle_camera_scale.macros.inc"
#include "rom-materials/world_vehicle_angular_motion.macros.inc"
#include "rom-materials/world_vehicle_forward_motion.macros.inc"
#include "rom-materials/world_vehicle_altitude_bound.macros.inc"
#include "rom-materials/world_vehicle_strafe_speed.macros.inc"
#include "rom-materials/world_vehicle_airship_frame_wait.macros.inc"
#include "rom-materials/world_vehicle_esper_frame_wait.macros.inc"
#include "rom-materials/world_vehicle_arrow_cycle.macros.inc"
#include "rom-materials/world_vehicle_sprite_threshold.macros.inc"
#include "rom-materials/world_vehicle_esper_frames.macros.inc"
#include "rom-materials/world_vehicle_ground_origin.macros.inc"

namespace ff6 {
void prepare_world_vehicle_picture(WorldState& world) {
    auto& v = world.vehicle.value();
    v.picture = WorldVehiclePicture{world.x, world.y,
        static_cast<std::int16_t>(world_vehicle_camera_lean(v)),
        static_cast<std::uint8_t>(world_vehicle_ground_rows(v)), v.arrow_clock,
        v.arrows, v.left, v.heading, v.altitude, v.sprite_frame, world.clock};
}
int world_vehicle_lean(std::int16_t turn, std::uint16_t speed) {
    const int magnitude = turn < 0 ? -int(turn) - 1 : int(turn);
    const int size = ((magnitude / 8 * (speed / 16) / 256) * FF6_ROM_VALUE_WORLD_VEHICLE_CAMERA_SCALE_0) / 256;
    return turn < 0 ? size : -size;
}
int world_vehicle_ground_rows(const WorldVehicle& v) {
    const int lean = world_vehicle_camera_lean(v);
    const int pitch = (((v.climb < 0 ? -int(v.climb) - 1 : int(v.climb)) / 8 *
                       (v.speed / 16) / 256) * FF6_ROM_VALUE_WORLD_VEHICLE_CAMERA_SCALE_0) / 256;
    const int base = v.movement_flags & 16U ? v.camera_ground_rows
                                          : FF6_ROM_VALUE_WORLD_VEHICLE_GROUND_ORIGIN_0 + (v.climb < 0 ? pitch : -pitch);
    return base + (lean < 0 ? -lean - 1 : lean) / 2;
}
int world_vehicle_camera_lean(const WorldVehicle& v) {
    return v.movement_flags & 8U ? v.camera_lean : world_vehicle_lean(v.turn, v.speed);
}
void advance_world_vehicle(const WorldAssets& a, WorldState& world, Buttons input, FieldFlags& flags, bool player_control) {
    auto& v = world.vehicle.value();
    auto controls = v.controls;
    const unsigned strafe_heading = v.heading / 256;
    const bool strafe = player_control && held(input, Button::y);
    if (player_control) {
        // GetVehicleInput: A accelerates; releasing it brakes. Authored pauses
        // intentionally retain momentum and must not inherit this rule.
        controls = held(input, Button::a) && !strafe ? 4 : 2;
        if (!strafe) {
            if (held(input, Button::right)) controls |= 8;
            else if (held(input, Button::left)) controls |= 16;
            if (held(input, Button::up)) controls |= 64;
            else if (held(input, Button::down)) controls |= 32;
        }
    }
    // UpdateAutoCtrl retains forward momentum through pauses; only BACK brakes.
    // Motion operands authenticate both player and scripted controllers; they
    // are ROM-selected tuning, distinct from the fixed-point encoding below.
    int turn = v.turn;
    if (controls & 8U) turn = std::max(-FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_0, turn - FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_1);
    else if (controls & 16U) turn = std::min(FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_0, turn + FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_1);
    else if (turn) turn += turn < 0 ? FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_2 : -FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_2;
    v.turn = static_cast<std::int16_t>(turn);
    if (controls & 4U) v.speed = static_cast<std::uint16_t>(std::min(FF6_ROM_VALUE_WORLD_VEHICLE_FORWARD_MOTION_0 - 1, int(v.speed) + FF6_ROM_VALUE_WORLD_VEHICLE_FORWARD_MOTION_1));
    if (controls & 2U) v.speed = static_cast<std::uint16_t>(std::max(0, int(v.speed) - FF6_ROM_VALUE_WORLD_VEHICLE_FORWARD_MOTION_2));
    int climb = v.climb;
    if (controls & 64U) climb = std::max(-FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_0, climb - FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_1);
    else if (controls & 32U) climb = std::min(FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_0, climb + FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_1);
    else if (climb) climb += climb < 0 ? FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_2 : -FF6_ROM_VALUE_WORLD_VEHICLE_ANGULAR_MOTION_2;
    v.climb = static_cast<std::int16_t>(climb);
    int heading = static_cast<int>(v.heading);
    if (controls & 1U) {
        if (controls & 16U) heading += 512;
        if (controls & 8U) heading -= 512;
    }
    if (player_control && !strafe) {
        if (held(input, Button::l) && held(input, Button::left)) heading += 512;
        if (held(input, Button::r) && held(input, Button::right)) heading -= 512;
    }
    if (!(v.movement_flags & 2U)) heading += turn;
    v.heading = static_cast<std::uint32_t>((heading + 360 * 256) % (360 * 256));
    if (v.choose_direction) {
        if (held(input, Button::right)) flags.story.reset(0x1b7);
        else if (held(input, Button::left)) flags.story.set(0x1b7);
    }
    v.left = flags.story[0x1b7];
    const unsigned angle = v.movement_flags & 32U ? v.movement_heading : v.heading / 256;
    int dx = int(v.speed * unsigned{a.sine.at(angle % 180)} / 256) * (angle < 180 ? -1 : 1);
    int dy = int(v.speed * unsigned{a.sine.at(angle % 180 + 90)} / 256) *
                   (angle < 90 || angle >= 270 ? -1 : 1);
    if (strafe) {
        // EE/6D13: Y translates relative to the current heading. Opposed and
        // three-way directions fall back to the source's north entry.
        const unsigned direction = (held(input, Button::up) ? 8U : 0U) |
            (held(input, Button::down) ? 4U : 0U) |
            (held(input, Button::left) ? 2U : 0U) |
            (held(input, Button::right) ? 1U : 0U);
        if (direction) {
            constexpr std::array<unsigned, 11> offsets{
#include "rom-materials/world_strafe_angles.inc"
            };
            const unsigned side = (strafe_heading + (direction < offsets.size() ? offsets[direction] : 0U)) % 360U;
            dx += int(a.sine.at(side % 180)) * (side < 180 ? -FF6_ROM_VALUE_WORLD_VEHICLE_STRAFE_SPEED_0 : FF6_ROM_VALUE_WORLD_VEHICLE_STRAFE_SPEED_0);
            dy += int(a.sine.at(side % 180 + 90)) * (side < 90 || side >= 270 ? -FF6_ROM_VALUE_WORLD_VEHICLE_STRAFE_SPEED_0 : FF6_ROM_VALUE_WORLD_VEHICLE_STRAFE_SPEED_0);
        }
    }
    const unsigned x = static_cast<unsigned>(int(world.x) * 256 + v.fraction_x + dx) & 0xfffffU;
    const unsigned y = static_cast<unsigned>(int(world.y) * 256 + v.fraction_y + dy) & 0xfffffU;
    world.x = static_cast<std::uint16_t>(x / 256);
    world.y = static_cast<std::uint16_t>(y / 256);
    v.fraction_x = static_cast<std::uint8_t>(x & 255);
    v.fraction_y = static_cast<std::uint8_t>(y & 255);
    const int altitude = v.altitude + climb;
    if (!(v.movement_flags & 64U) && altitude >= 0 && altitude < FF6_ROM_VALUE_WORLD_VEHICLE_ALTITUDE_BOUND_0)
        v.altitude = static_cast<std::uint16_t>(altitude);
    if (v.arrows) v.arrow_clock = (v.arrow_clock + 1) & (FF6_ROM_VALUE_WORLD_VEHICLE_ARROW_CYCLE_0 - 1U);
    if (world.map != 2 && !v.esper) {
        v.sprite_wait = wrap8(unsigned{v.sprite_wait} - 1U);
        if (!v.sprite_wait) {
            v.sprite_phase = wrap8(unsigned{v.sprite_phase} - 1U);
            v.sprite_wait = static_cast<std::uint8_t>((wrap8(FF6_ROM_VALUE_WORLD_VEHICLE_AIRSHIP_FRAME_WAIT_0 + 0U - v.speed / 256U) >> 1U) + 1U);
        }
        const unsigned turn_group = v.turn >= FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? 8U : v.turn < -FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? 4U : 0U;
        const unsigned pitch = v.climb >= FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? 1U : v.climb < -FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? 2U : 0U;
        v.sprite_frame = static_cast<std::uint8_t>(a.airship_direction_frames[turn_group + pitch + 1] +
                                                  (v.sprite_phase & 1U));
    }
    if (v.esper) {
        // EE/4AD4: turning selects a pair of frames; forward speed controls
        // their cadence. Keep the byte countdown, including zero wrapping.
        v.sprite_wait = wrap8(unsigned{v.sprite_wait} - 1U);
        if (!v.sprite_wait) {
            v.sprite_phase = wrap8(unsigned{v.sprite_phase} - 1U);
            v.sprite_wait = static_cast<std::uint8_t>(
                (wrap8(FF6_ROM_VALUE_WORLD_VEHICLE_ESPER_FRAME_WAIT_0 + 0U - v.speed / 256U) >> 1U) + 1U);
        }
        const unsigned pair = v.turn >= FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? FF6_ROM_VALUE_WORLD_VEHICLE_ESPER_FRAMES_1 : v.turn < -FF6_ROM_VALUE_WORLD_VEHICLE_SPRITE_THRESHOLD_0 ? FF6_ROM_VALUE_WORLD_VEHICLE_ESPER_FRAMES_2 : FF6_ROM_VALUE_WORLD_VEHICLE_ESPER_FRAMES_0;
        v.sprite_frame = static_cast<std::uint8_t>(pair + (v.sprite_phase & 1U));
    }
}
} // namespace ff6
