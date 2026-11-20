#include "ff6/world_map.hpp"
#include "rom-materials/world_explosion_small_radius_bound.macros.inc"
#include "rom-materials/world_explosion_large_radius_bound.macros.inc"
#include "rom-materials/world_explosion_cadence.macros.inc"
#include "rom-materials/world_explosion_initial_wait.macros.inc"
#include "rom-materials/world_explosion_initial_frames.macros.inc"
#include "rom-materials/world_explosion_palette_initial.macros.inc"
#include "rom-materials/world_explosion_palette_delta.macros.inc"
#include "rom-materials/world_explosion_palette_steps.macros.inc"
#include "rom-materials/world_explosion_radius_origin.macros.inc"
#include "rom-materials/world_explosion_fade_radius.macros.inc"

namespace ff6 {
std::array<std::uint8_t, 256> world_explosion_widths(unsigned radius) {
    if (radius > 255) throw std::invalid_argument("Invalid world explosion radius");
    std::array<std::uint8_t, 256> widths{};
    // UpdateMode7Circle is a fixed-point circle recurrence, not a device
    // operation. Its larger step for small radii gives the authored edge shape.
    unsigned vertical = radius * 256, horizontal = 0;
    const unsigned step = radius < FF6_ROM_VALUE_WORLD_EXPLOSION_SMALL_RADIUS_BOUND_0 / 256 ? 4 :
                          radius < FF6_ROM_VALUE_WORLD_EXPLOSION_LARGE_RADIUS_BOUND_0 / 256 ? 2 : 1;
    if (!radius) return widths;
    for (;;) {
        horizontal = (horizontal + (vertical / 256) * step) & 65535U;
        const unsigned decrement = (horizontal / 256) * step;
        if (decrement > vertical) break;
        vertical -= decrement;
        widths[vertical / 256] = static_cast<std::uint8_t>(horizontal / 256);
    }
    return widths;
}
bool advance_world_ruin_explosion(WorldRuinExplosion& scene) {
    if (scene.finished) return false;
    ++scene.frames;
    // RuinScene presents each update for two frames, including its sixteen
    // initial waits. Do not advance the airship while this owner is active.
    if (scene.frames % FF6_ROM_VALUE_WORLD_EXPLOSION_CADENCE_0) return false;
    const unsigned visit = scene.frames / FF6_ROM_VALUE_WORLD_EXPLOSION_CADENCE_0;
    if (visit > FF6_ROM_VALUE_WORLD_EXPLOSION_INITIAL_WAIT_0) {
        const unsigned scale = FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_INITIAL_0 -
            std::min(unsigned{FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_STEPS_0},
                     visit - FF6_ROM_VALUE_WORLD_EXPLOSION_INITIAL_WAIT_0) *
                FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_DELTA_0;
        if (scale >= FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_INITIAL_0 -
                     FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_STEPS_0 *
                         FF6_ROM_VALUE_WORLD_EXPLOSION_PALETTE_DELTA_0)
            scene.palette_scale = static_cast<std::uint8_t>(scale);
        // The native picture retains the radius preceding the source update.
        // Convert the extracted fade origin into that retained coordinate.
        scene.radius = std::min(unsigned{FF6_ROM_VALUE_WORLD_EXPLOSION_RADIUS_ORIGIN_0 - 1},
                                (visit - (FF6_ROM_VALUE_WORLD_EXPLOSION_INITIAL_WAIT_0 + 1)) *
                                    (255 - FF6_ROM_VALUE_WORLD_EXPLOSION_RADIUS_ORIGIN_0));
    }
    if (scene.brightness < scene.target) ++scene.brightness;
    else if (scene.brightness > scene.target) --scene.brightness;
    if (scene.radius >= FF6_ROM_VALUE_WORLD_EXPLOSION_FADE_RADIUS_0)
        scene.target = static_cast<std::uint8_t>(
            (FF6_ROM_VALUE_WORLD_EXPLOSION_RADIUS_ORIGIN_0 - scene.radius) / 2);
    scene.finished = scene.brightness == 0;
    return scene.frames == FF6_ROM_VALUE_WORLD_EXPLOSION_INITIAL_FRAMES_0;
}
} // namespace ff6
