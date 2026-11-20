#include "ff6/world_map.hpp"
#include "rom-materials/airship_smoke_height_step.macros.inc"
#include "rom-materials/airship_smoke_jitter_bias.macros.inc"
#include "rom-materials/airship_smoke_jitter_period.macros.inc"
#include "rom-materials/airship_smoke_rotor_period.macros.inc"
#include "rom-materials/airship_smoke_frame_end.macros.inc"
#include "rom-materials/airship_smoke_frame_start.macros.inc"
#include "rom-materials/airship_smoke_vertical_step.macros.inc"
#include "rom-materials/airship_smoke_x_origin.macros.inc"
#include "rom-materials/airship_smoke_spawn_lag.macros.inc"
#include "rom-materials/airship_smoke_spawn_fallback.macros.inc"
#include "rom-materials/airship_crash_zoom.macros.inc"
#include "rom-materials/airship_crash_jitter_phase.macros.inc"
#include "rom-materials/airship_crash_jitter_mask.macros.inc"
#include "rom-materials/airship_crash_jitter_cap.macros.inc"
#include "rom-materials/airship_crash_jitter_bias.macros.inc"
#include "rom-materials/airship_crash_speed.macros.inc"
#include "rom-materials/airship_crash_fade_step.macros.inc"
#include <algorithm>

namespace ff6 {
// The native compiler folds this clamp into a test of the cap bit. Bind both
// source operands so the restoration recipe cannot leave a separate mask value.
static_assert(FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_MASK_0 ==
              FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_CAP_0 * 2 - 1);
void advance_airship_smoke(const WorldAssets& assets, WorldAirshipSmoke& smoke) {
    if (assets.airship_smoke_parameters.size() != 32768 * 6)
        throw std::invalid_argument("Invalid smoking airship parameters");
    const auto parameters = [&](unsigned index) {
        return slice(assets.airship_smoke_parameters, (index / 2) * 6, 6);
    };
    // UpdateSpriteAnim_10/11: this damaged ship has its own rotor, slowly
    // dropping anchor and five cycling smoke emitters, independent of steering.
    if (!--smoke.jitter_wait) {
        const auto noise = parameters(smoke.sample);
        smoke.height = static_cast<std::uint16_t>(smoke.height + unsigned{FF6_ROM_VALUE_AIRSHIP_SMOKE_HEIGHT_STEP_0});
        smoke.sample = static_cast<std::uint16_t>(smoke.sample + 2U);
        smoke.jitter_x = static_cast<std::int8_t>(int(noise[0]) - FF6_ROM_VALUE_AIRSHIP_SMOKE_JITTER_BIAS_0);
        smoke.jitter_y = static_cast<std::int8_t>(int(noise[1]) - FF6_ROM_VALUE_AIRSHIP_SMOKE_JITTER_BIAS_0);
        smoke.jitter_wait = FF6_ROM_VALUE_AIRSHIP_SMOKE_JITTER_PERIOD_0;
    }
    if (!--smoke.rotor_wait) {
        smoke.rotor ^= 1;
        smoke.rotor_wait = FF6_ROM_VALUE_AIRSHIP_SMOKE_ROTOR_PERIOD_0;
    }
    for (unsigned i = 0; i < smoke.particles.size(); ++i) {
        auto& particle = smoke.particles[(smoke.first + i) % 5];
        if (particle.frame && !--particle.wait) {
            if (++particle.frame == FF6_ROM_VALUE_AIRSHIP_SMOKE_FRAME_END_0) particle.frame = 0;
            particle.wait = particle.period;
        }
        particle.y = wrap8(unsigned{particle.y} + unsigned{FF6_ROM_VALUE_AIRSHIP_SMOKE_VERTICAL_STEP_0});
        particle.x = static_cast<std::uint16_t>((int(particle.x) + particle.drift) & 65535);
        smoke.picture[i] = particle;
    }
    smoke.first = static_cast<std::uint8_t>((smoke.first + 1U) % 5U);
    // Preserve the source's two early slots aliasing the last emitter.
    const unsigned spawn = smoke.first >= FF6_ROM_VALUE_AIRSHIP_SMOKE_SPAWN_LAG_0 ?
        smoke.first - unsigned{FF6_ROM_VALUE_AIRSHIP_SMOKE_SPAWN_LAG_0} :
        unsigned{FF6_ROM_VALUE_AIRSHIP_SMOKE_SPAWN_FALLBACK_0};
    auto& particle = smoke.particles[spawn];
    const auto noise = parameters(smoke.sample);
    if (!particle.frame && noise[2]) {
        const unsigned drift = read16(noise, 4);
        particle = {FF6_ROM_VALUE_AIRSHIP_SMOKE_FRAME_START_0, noise[3], noise[3],
            wrap8(smoke.height / 256U + static_cast<unsigned>(int(smoke.jitter_y))),
            FF6_ROM_VALUE_AIRSHIP_SMOKE_X_ORIGIN_0, static_cast<std::int16_t>(drift < 32768 ? int(drift) : int(drift) - 65536)};
    }
}

void advance_airship_crash(const WorldAssets& assets, WorldAirshipCrash& crash, WorldState& world) {
    if (crash.finished) return;
    if (!world.vehicle || assets.crash_jitter.size() < 1024 || assets.sine.size() < 270)
        throw std::invalid_argument("Invalid crash presentation input");
    auto& v = *world.vehicle;
    // AirshipCrash (EE/0B30): cleared carry adds a borrow to the zoom operand,
    // switching to zero scale once the floor is reached, then fade to black.
    if (crash.scale < FF6_ROM_VALUE_AIRSHIP_CRASH_ZOOM_1 +
        (FF6_ROM_VALUE_AIRSHIP_CRASH_ZOOM_0 + 1)) {
        crash.scale = 0;
        crash.target_brightness = 0;
    }
    else crash.scale = static_cast<std::uint16_t>(crash.scale - (FF6_ROM_VALUE_AIRSHIP_CRASH_ZOOM_0 + 1));
    if (!(crash.frame & unsigned{FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_PHASE_0})) {
        const int noise = std::min(int(FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_CAP_0),
            int(assets.crash_jitter.at(crash.frame) & unsigned{FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_MASK_0})) -
            FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_BIAS_0;
        crash.angle = static_cast<std::uint16_t>((int(crash.angle) + noise + 360) % 360);
    }
    const unsigned angle = (crash.angle + (crash.frame & unsigned{FF6_ROM_VALUE_AIRSHIP_CRASH_JITTER_PHASE_0})) % 360;
    v.heading = angle * 256;
    v.picture.reset();
    // The crash owns position and camera. Normal steering/climb must not run
    // in parallel or change its constant forward speed and descent curve.
    const int dx = int(unsigned{FF6_ROM_VALUE_AIRSHIP_CRASH_SPEED_0} * assets.sine.at(angle % 180) / 256) * (angle < 180 ? -1 : 1);
    const int dy = int(unsigned{FF6_ROM_VALUE_AIRSHIP_CRASH_SPEED_0} * assets.sine.at(angle % 180 + 90) / 256) *
        (angle < 90 || angle >= 270 ? -1 : 1);
    const unsigned x = static_cast<unsigned>(int(world.x) * 256 + v.fraction_x + dx) & 0xfffffU;
    const unsigned y = static_cast<unsigned>(int(world.y) * 256 + v.fraction_y + dy) & 0xfffffU;
    world.x = static_cast<std::uint16_t>(x / 256); v.fraction_x = static_cast<std::uint8_t>(x & 255);
    world.y = static_cast<std::uint16_t>(y / 256); v.fraction_y = static_cast<std::uint8_t>(y & 255);
    if (crash.brightness < crash.target_brightness) crash.brightness += FF6_ROM_VALUE_AIRSHIP_CRASH_FADE_STEP_0;
    else if (crash.brightness > crash.target_brightness) crash.brightness -= FF6_ROM_VALUE_AIRSHIP_CRASH_FADE_STEP_0;
    ++crash.frame;
    crash.finished = crash.brightness < 256;
}
} // namespace ff6
