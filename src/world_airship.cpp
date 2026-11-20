#include "ff6/world_map.hpp"
#include "rom-materials/airship_transition_falcon_frame.macros.inc"
#include "rom-materials/airship_transition_blackjack_frame.macros.inc"
#include "rom-materials/airship_transition_falcon_rotor.macros.inc"
#include "rom-materials/airship_transition_blackjack_rotor.macros.inc"
#include "rom-materials/airship_transition_rotor_cutoff.macros.inc"
#include "rom-materials/airship_transition_rotor_terminal.macros.inc"
#include "rom-materials/airship_transition_landing_complete.macros.inc"
#include "rom-materials/airship_transition_camera_rotor_period.macros.inc"
#include "rom-materials/airship_transition_lift_period_bound.macros.inc"
#include "rom-materials/airship_transition_acceleration.macros.inc"
#include "rom-materials/airship_transition_sprite_top.macros.inc"
#include "rom-materials/airship_transition_sprite_center.macros.inc"
#include "rom-materials/airship_transition_lift_fade_velocity.macros.inc"
#include "rom-materials/airship_transition_camera_velocity.macros.inc"
#include "rom-materials/airship_transition_landing_velocity_floor.macros.inc"
#include "rom-materials/airship_transition_landing_deceleration.macros.inc"
#include "rom-materials/airship_transition_landing_sprite_limit.macros.inc"
#include "rom-materials/airship_transition_landing_y.macros.inc"
#include "rom-materials/airship_transition_landing_velocity.macros.inc"
#include "rom-materials/airship_transition_camera_center.macros.inc"
#include "rom-materials/airship_transition_camera_frame.macros.inc"
#include "rom-materials/airship_transition_landing_fade_steps.macros.inc"
#include "rom-materials/airship_transition_descent_step.macros.inc"
#include "rom-materials/airship_transition_descent_limit.macros.inc"
#include "rom-materials/airship_transition_camera_words.macros.inc"
#include <algorithm>

namespace ff6 {
void begin_airship_liftoff(WorldState& w) {
    if (w.vehicle || w.chocobo || w.airship_transition || w.map > 1 || w.remaining)
        throw std::logic_error("Airship liftoff requires a stopped party on the world map");
    w.dx = w.dy = 0;
    w.airship_transition.emplace();
    w.airship_transition->sprite_frame = w.map
        ? FF6_ROM_VALUE_AIRSHIP_TRANSITION_FALCON_FRAME_0
        : FF6_ROM_VALUE_AIRSHIP_TRANSITION_BLACKJACK_FRAME_0;
}
void begin_airship_landing(WorldState& w) {
    if (!w.vehicle || w.airship_transition || w.map > 1)
        throw std::logic_error("Airship landing requires free flight");
    auto& v = *w.vehicle;
    v.speed = 0;
    v.turn = v.climb = 0;
    v.controls = 0;
    w.airship_transition.emplace();
    w.airship_transition->phase = WorldAirshipTransition::Phase::descend;
}
bool advance_airship_transition(const WorldAssets& a, WorldState& w) {
    constexpr int step_count =
        static_cast<int>(std::tuple_size_v<decltype(a.airship_transition_steps)> / 7);
    constexpr unsigned last_step = step_count - 1U;
    auto& t = w.airship_transition.value();
    using Phase = WorldAirshipTransition::Phase;
    ++w.clock;
    // These are the source's visible rotor/camera phases (EE/94D4..97FE),
    // expressed as a native animation rather than a hardware transfer schedule.
    if (t.phase == Phase::descend) {
        auto& v = *w.vehicle;
        v.picture.reset();
        if (int(v.altitude) + v.climb < 0) {
            v.altitude = 0;
            v.climb = 0;
            t.phase = Phase::land_camera;
            t.sprite_y = FF6_ROM_VALUE_AIRSHIP_TRANSITION_SPRITE_CENTER_0;
            t.numerator = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_0 |
                          (FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_1 << 8U);
            t.denominator = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_2 |
                            (FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_3 << 8U);
            t.end_scale = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_4 |
                          (FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_5 << 8U);
            t.ground_rows = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_6 |
                            (FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_WORDS_7 << 8U);
            t.center_y = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_CENTER_0;
            t.period = 1;
        } else {
            v.altitude = static_cast<std::uint16_t>(int(v.altitude) + v.climb);
            // The private sum preserves the compiler-folded clamp operand;
            // subtract the shared step and decode its signed source word here.
            v.climb = static_cast<std::int16_t>(std::max(
                int{FF6_ROM_VALUE_AIRSHIP_TRANSITION_DESCENT_LIMIT_0} -
                    FF6_ROM_VALUE_AIRSHIP_TRANSITION_DESCENT_STEP_0 - 65536,
                int(v.climb) - FF6_ROM_VALUE_AIRSHIP_TRANSITION_DESCENT_STEP_0));
        }
        return false;
    }
    if (!--t.timer) {
        t.propeller ^= 1U;
        if (t.phase == Phase::lift_ground)
            t.period = std::max<std::uint16_t>(
                FF6_ROM_VALUE_AIRSHIP_TRANSITION_LIFT_PERIOD_BOUND_0 - 1, t.period - 1);
        else if (t.phase == Phase::land_ground)
            t.period = std::min<std::uint16_t>(
                FF6_ROM_VALUE_AIRSHIP_TRANSITION_ROTOR_TERMINAL_0, t.period + 1);
        t.timer = t.phase == Phase::lift_camera || t.phase == Phase::land_camera
            ? FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_ROTOR_PERIOD_0 : t.period;
    }
    if (t.phase == Phase::lift_ground || t.phase == Phase::land_ground) {
        t.sprite_frame = static_cast<std::uint8_t>(
            (w.map ? FF6_ROM_VALUE_AIRSHIP_TRANSITION_FALCON_FRAME_0
                   : FF6_ROM_VALUE_AIRSHIP_TRANSITION_BLACKJACK_FRAME_0) +
            (t.propeller && t.period < FF6_ROM_VALUE_AIRSHIP_TRANSITION_ROTOR_CUTOFF_0
                ? (w.map ? FF6_ROM_VALUE_AIRSHIP_TRANSITION_FALCON_ROTOR_0
                         : FF6_ROM_VALUE_AIRSHIP_TRANSITION_BLACKJACK_ROTOR_0) : 0));
        if (t.phase == Phase::lift_ground) {
            if (t.period < FF6_ROM_VALUE_AIRSHIP_TRANSITION_LIFT_PERIOD_BOUND_0) {
                t.sprite_y = std::max(int{FF6_ROM_VALUE_AIRSHIP_TRANSITION_SPRITE_TOP_0} - 65536,
                                      int(t.sprite_y) - t.velocity / 256);
                t.velocity += FF6_ROM_VALUE_AIRSHIP_TRANSITION_ACCELERATION_0;
            }
            if (t.velocity >= FF6_ROM_VALUE_AIRSHIP_TRANSITION_LIFT_FADE_VELOCITY_0 && t.brightness)
                --t.brightness;
            if (!t.brightness) {
                t.phase = Phase::lift_camera;
                t.velocity = FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_VELOCITY_0;
                t.timer = 1;
                t.frame = 0;
                w.vehicle.emplace();
            }
        } else {
            if (t.velocity >= FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_VELOCITY_FLOOR_0) {
                t.velocity -= FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_DECELERATION_0;
                t.sprite_y = std::min(FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_SPRITE_LIMIT_0,
                                      int(t.sprite_y) + t.velocity);
            }
            t.brightness = std::min<std::uint8_t>(15, t.brightness + 1);
            if (t.period == FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_COMPLETE_0) return true;
        }
        return false;
    }
    const bool lift = t.phase == Phase::lift_camera;
    if (lift) {
        t.velocity -= FF6_ROM_VALUE_AIRSHIP_TRANSITION_ACCELERATION_0;
        t.sprite_y = std::min(FF6_ROM_VALUE_AIRSHIP_TRANSITION_SPRITE_CENTER_0,
                             int(t.sprite_y) + t.velocity / 256);
        t.brightness = std::min<std::uint8_t>(15, t.brightness + 1);
    } else {
        t.sprite_y = std::max(int{FF6_ROM_VALUE_AIRSHIP_TRANSITION_SPRITE_TOP_0} - 65536,
                             int(t.sprite_y) - t.velocity / 256);
        t.velocity += FF6_ROM_VALUE_AIRSHIP_TRANSITION_ACCELERATION_0;
        if (t.frame >= FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_FADE_STEPS_0 -
                           FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_FADE_STEPS_1 && t.brightness)
            --t.brightness;
    }
    t.sprite_frame = static_cast<std::uint8_t>(FF6_ROM_VALUE_AIRSHIP_TRANSITION_CAMERA_FRAME_0 + t.propeller);
    // The count belongs to the extracted seven-byte record format. Retain the
    // prior expression types so private data regeneration preserves native code.
    const auto at = (lift ? std::size_t{t.frame} : last_step - std::size_t{t.frame}) * 7;
    const int sign = lift ? -1 : 1;
    t.numerator = static_cast<std::uint16_t>(t.numerator + sign * read16(a.airship_transition_steps, at));
    t.denominator = static_cast<std::uint16_t>(t.denominator + sign * read16(a.airship_transition_steps, at + 2));
    t.end_scale = static_cast<std::uint16_t>(t.end_scale + sign * a.airship_transition_steps[at + 4]);
    t.ground_rows = static_cast<std::uint16_t>(t.ground_rows + sign * a.airship_transition_steps[at + 5]);
    t.center_y = static_cast<std::uint16_t>(int(t.center_y) - sign * a.airship_transition_steps[at + (lift ? 5 : 6)]);
    ++t.frame;
    if (t.frame == step_count) {
        if (lift) {
            w.vehicle->sprite_frame = t.sprite_frame;
            return true;
        }
        // The on-foot camera owns the second half of landing. Preserve the
        // parked tile independently of the old subpixel flight position.
        w.vehicle.reset();
        w.x &= 4080U;
        w.y &= 4080U;
        w.facing = 2;
        w.forest = (world_tile_properties(a, w) & 32U) != 0;
        w.encounter_origin = {wrap8(w.x / 16), wrap8(w.y / 16)};
        t.phase = Phase::land_ground;
        t.sprite_y = int{FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_Y_0} - 65536;
        t.velocity = FF6_ROM_VALUE_AIRSHIP_TRANSITION_LANDING_VELOCITY_0;
        t.timer = 1;
        t.brightness = 0;
        t.sprite_frame = w.map ? FF6_ROM_VALUE_AIRSHIP_TRANSITION_FALCON_FRAME_0
                              : FF6_ROM_VALUE_AIRSHIP_TRANSITION_BLACKJACK_FRAME_0;
    }
    return false;
}
} // namespace ff6
