#include "ff6/world_map.hpp"
#include "rom-materials/falcon_rotor_bit.macros.inc"
#include "rom-materials/falcon_rise_rotor_period.macros.inc"
#include "rom-materials/falcon_lift_bounds.macros.inc"
#include "rom-materials/falcon_wake_height.macros.inc"
#include "rom-materials/falcon_wake_frame.macros.inc"
#include "rom-materials/falcon_splash_height_origin.macros.inc"
#include "rom-materials/falcon_splash_height_mask.macros.inc"
#include "rom-materials/falcon_splash_phase_end.macros.inc"
#include "rom-materials/falcon_splash_drift_mask.macros.inc"
#include "rom-materials/falcon_splash_drift_bias.macros.inc"
#include "rom-materials/falcon_splash_period_mask.macros.inc"
#include "rom-materials/falcon_splash_y_mask.macros.inc"
#include "rom-materials/falcon_splash_y_origin.macros.inc"
#include "rom-materials/falcon_splash_x_displacement.source.macros.inc"
#include "rom-materials/falcon_splash_first_phase.macros.inc"
#include "rom-materials/falcon_splash_bank_span.macros.inc"
#include "rom-materials/falcon_depart_angle.macros.inc"
#include "rom-materials/falcon_depart_amplitude.macros.inc"
#include "rom-materials/falcon_depart_acceleration_x.macros.inc"
#include "rom-materials/falcon_depart_acceleration_y.macros.inc"
#include "rom-materials/falcon_depart_rotor_slow.macros.inc"
#include "rom-materials/falcon_depart_rotor_fast.macros.inc"
#include "rom-materials/falcon_ship_frame.macros.inc"
#include "rom-materials/falcon_fade_position.macros.inc"
#include <algorithm>

namespace ff6 {
void advance_falcon_rise(const WorldAssets& assets, WorldFalconRise& rise) {
    if (rise.finished) return;
    if (assets.sine.size() < FF6_ROM_VALUE_FALCON_DEPART_ANGLE_0 + 1)
        throw std::invalid_argument("Missing Falcon departure curve");
    // UpdateSpriteAnim_0e/0f (EE/4F26): lift the hull out of the water
    // while publishing alternating groups of five splash images.
    if (!rise.departing) {
        if (!--rise.rotor_wait) {
            rise.rotor ^= FF6_ROM_VALUE_FALCON_ROTOR_BIT_0;
            rise.rotor_wait = FF6_ROM_VALUE_FALCON_RISE_ROTOR_PERIOD_0;
        }
        rise.picture_x = static_cast<std::int16_t>(rise.x / 256);
        rise.picture_y = static_cast<std::int16_t>(rise.y / 256);
        rise.y = static_cast<std::uint16_t>(std::max(FF6_ROM_VALUE_FALCON_LIFT_BOUNDS_1,
            int(rise.y) - (FF6_ROM_VALUE_FALCON_LIFT_BOUNDS_0 + 1)));
        const unsigned height = rise.y / 256U;
        rise.wake_frame = height < FF6_ROM_VALUE_FALCON_WAKE_HEIGHT_0 ? 0 :
            static_cast<std::uint8_t>(FF6_ROM_VALUE_FALCON_WAKE_FRAME_0 +
                (height - FF6_ROM_VALUE_FALCON_WAKE_HEIGHT_0) / 8);
        const unsigned base = ((height - FF6_ROM_VALUE_FALCON_SPLASH_HEIGHT_ORIGIN_0) &
            unsigned{FF6_ROM_VALUE_FALCON_SPLASH_HEIGHT_MASK_0}) * 3 / 4;
        for (unsigned i = 0; i < rise.picture.size(); ++i) {
            auto& splash = rise.splashes[rise.first + i];
            if (splash.phase && !--splash.wait) {
                splash.phase = static_cast<std::uint8_t>((splash.phase + 1U) %
                    unsigned{FF6_ROM_VALUE_FALCON_SPLASH_PHASE_END_0});
                splash.wait = splash.period;
            }
            splash.frame = assets.falcon_splash_frames.at(base + splash.phase);
            // The original emitter shares the first slot's horizontal drift.
            splash.x = static_cast<std::uint16_t>(splash.x + rise.drift);
            rise.picture[i] = splash;
        }
        ++rise.random;
        for (unsigned i = 0; i < rise.picture.size(); ++i) {
            auto& splash = rise.splashes[rise.first + i];
            if (splash.phase) continue;
            const auto noise = [&](unsigned offset) {
                return assets.battles.random[wrap8(unsigned(rise.random) + offset)];
            };
            rise.drift = static_cast<std::uint8_t>((noise(1) & unsigned{FF6_ROM_VALUE_FALCON_SPLASH_DRIFT_MASK_0}) +
                unsigned{FF6_ROM_VALUE_FALCON_SPLASH_DRIFT_BIAS_0});
            splash.period = splash.wait = static_cast<std::uint8_t>((noise(2) & unsigned{FF6_ROM_VALUE_FALCON_SPLASH_PERIOD_MASK_0}) + 1U);
            splash.y = static_cast<std::uint8_t>((noise(3) & unsigned{FF6_ROM_VALUE_FALCON_SPLASH_Y_MASK_0}) +
                unsigned{FF6_ROM_VALUE_FALCON_SPLASH_Y_ORIGIN_0});
            splash.x = wrap8(unsigned(rise.x / 256) +
                unsigned{FF6_ROM_VALUE_FALCON_SPLASH_X_DISPLACEMENT_SOURCE_0} - height -
                unsigned{FF6_ROM_VALUE_FALCON_SPLASH_X_DISPLACEMENT_SOURCE_1});
            splash.phase = FF6_ROM_VALUE_FALCON_SPLASH_FIRST_PHASE_0;
            break;
        }
        rise.first = static_cast<std::uint8_t>(FF6_ROM_VALUE_FALCON_SPLASH_BANK_SPAN_0 / 8 - rise.first);
        rise.departing = rise.y == FF6_ROM_VALUE_FALCON_LIFT_BOUNDS_1;
    } else {
        // UpdateSpriteAnim_14 (EE/50D6): a sine-shaped drift precedes the
        // accelerating departure. Explicit unsigned subtraction preserves the
        // carry contribution to the two accelerations without signed overflow.
        rise.angle = static_cast<std::uint16_t>(std::min(FF6_ROM_VALUE_FALCON_DEPART_ANGLE_0, int(rise.angle) + 2));
        rise.x += assets.sine[rise.angle] * FF6_ROM_VALUE_FALCON_DEPART_AMPLITUDE_0 / 256;
        if (rise.angle == FF6_ROM_VALUE_FALCON_DEPART_ANGLE_0) {
            const bool x_carry = rise.x >= rise.speed_x;
            const bool y_carry = rise.y >= rise.speed_y;
            rise.x -= rise.speed_x;
            rise.y = static_cast<std::uint16_t>(rise.y - rise.speed_y);
            rise.speed_x = static_cast<std::uint16_t>(rise.speed_x + unsigned{FF6_ROM_VALUE_FALCON_DEPART_ACCELERATION_X_0} + x_carry);
            rise.speed_y = static_cast<std::uint16_t>(rise.speed_y + unsigned{FF6_ROM_VALUE_FALCON_DEPART_ACCELERATION_Y_0} + y_carry);
        }
        if (!--rise.rotor_wait) {
            rise.rotor ^= FF6_ROM_VALUE_FALCON_ROTOR_BIT_0;
            rise.rotor_wait = rise.angle == FF6_ROM_VALUE_FALCON_DEPART_ANGLE_0 ?
                FF6_ROM_VALUE_FALCON_DEPART_ROTOR_FAST_0 : FF6_ROM_VALUE_FALCON_DEPART_ROTOR_SLOW_0;
        }
        rise.picture_x = static_cast<std::int16_t>(rise.x >= 0 ? rise.x / 256 : -((-rise.x + 255) / 256));
        rise.picture_y = static_cast<std::int16_t>(rise.y / 256);
        // The last water images persist below the departing hull, as in the
        // source's published sprite list; their final frames are transparent.
    }
    rise.ship_frame = static_cast<std::uint8_t>(FF6_ROM_VALUE_FALCON_SHIP_FRAME_0 + rise.rotor);
    const unsigned target = (static_cast<unsigned>(rise.x) & 65535U) < FF6_ROM_VALUE_FALCON_FADE_POSITION_0 ? 0 : 15;
    if (rise.brightness < target) ++rise.brightness;
    else if (rise.brightness > target) --rise.brightness;
    rise.finished = !target && !rise.brightness;
}
} // namespace ff6
