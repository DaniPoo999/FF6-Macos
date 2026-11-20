#include "ff6/world_map.hpp"
#include "rom-materials/world_judgment_blue_light.macros.inc"
#include "rom-materials/world_judgment_intensity_mask.macros.inc"
#include "rom-materials/world_judgment_start_x_step.macros.inc"
#include "rom-materials/world_judgment_start_y_forward.macros.inc"
#include "rom-materials/world_judgment_start_y_reverse.macros.inc"
#include "rom-materials/world_judgment_end_x_forward.macros.inc"
#include "rom-materials/world_judgment_end_x_reverse.macros.inc"
#include "rom-materials/world_judgment_duration.macros.inc"
#include "rom-materials/world_judgment_fade_remaining.macros.inc"
#include "rom-materials/world_judgment_width_mask.macros.inc"
#include "rom-materials/world_judgment_width_rejected.macros.inc"
#include "rom-materials/world_judgment_particle_phases.macros.inc"
#include "rom-materials/world_judgment_particle_y_forward.macros.inc"
#include "rom-materials/world_judgment_particle_y_reverse.macros.inc"
#include "rom-materials/world_judgment_drift_mask.macros.inc"
#include "rom-materials/world_judgment_drift_base.macros.inc"
#include "rom-materials/world_judgment_period_mask.macros.inc"
#include "rom-materials/world_judgment_spawn_y_mask.macros.inc"
#include "rom-materials/world_judgment_spawn_y_base.macros.inc"
#include "rom-materials/world_judgment_spawn_x.macros.inc"
#include "rom-materials/world_judgment_frame_base.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
// UpdateSpotlightHDMA's midpoint line, expressed as picture-space edges.
// The shallow and steep branches differ in when they publish the x step.
void beam_edge(WorldJudgment& shot, unsigned edge, unsigned start, unsigned end,
               unsigned top) {
    const int step = start >= end ? -1 : 1;
    const unsigned dx = start >= end ? start - end : end - start, dy = 224 - top;
    int x = static_cast<int>(start);
    unsigned row = top;
    for (unsigned y = 0; y < top; ++y) shot.edges[y][edge] = edge ? 0 : 255;
    const auto put = [&] {
        if (row < 224) shot.edges[row][edge] = wrap8(static_cast<unsigned>(x));
        ++row;
    };
    if (dx > dy) {
        int error = static_cast<int>(dx / 2);
        for (unsigned i = 0; i < dx; ++i) {
            error -= static_cast<int>(dy);
            if (error < 0) { error += static_cast<int>(dx); put(); }
            x += step;
        }
    } else {
        int error = static_cast<int>(dy / 2);
        for (unsigned i = 0; i < dy; ++i) {
            error -= static_cast<int>(dx);
            if (error < 0) { error += static_cast<int>(dy); x += step; }
            put();
        }
    }
}
} // namespace
bool advance_world_judgment(const WorldAssets& assets, WorldJudgment& shot) {
    if (shot.finished) return false;
    const bool cue = shot.frame == 0;
    // EE/1378: each variant sweeps a narrow illuminated stripe while the
    // camera flies forward. Fixed-point endpoints retain their fractional drift.
    const unsigned intensity = assets.battles.random[shot.random] &
        FF6_ROM_VALUE_WORLD_JUDGMENT_INTENSITY_MASK_0;
    shot.light = static_cast<Rgb555>(intensity | (intensity << 5) |
        FF6_ROM_VALUE_WORLD_JUDGMENT_BLUE_LIGHT_0);
    shot.start_x = static_cast<std::uint16_t>(shot.start_x +
        (shot.reverse ? -FF6_ROM_VALUE_WORLD_JUDGMENT_START_X_STEP_0
                      : FF6_ROM_VALUE_WORLD_JUDGMENT_START_X_STEP_0));
    shot.start_y = static_cast<std::uint16_t>(shot.start_y -
        (shot.reverse ? FF6_ROM_VALUE_WORLD_JUDGMENT_START_Y_REVERSE_0
                      : FF6_ROM_VALUE_WORLD_JUDGMENT_START_Y_FORWARD_0));
    shot.end_x = static_cast<std::uint16_t>(shot.end_x +
        (shot.reverse ? -FF6_ROM_VALUE_WORLD_JUDGMENT_END_X_REVERSE_0
                      : FF6_ROM_VALUE_WORLD_JUDGMENT_END_X_FORWARD_0));
    ++shot.frame;
    unsigned width = assets.battles.random[shot.frame & 255U] & FF6_ROM_VALUE_WORLD_JUDGMENT_WIDTH_MASK_0;
    if (width == FF6_ROM_VALUE_WORLD_JUDGMENT_WIDTH_REJECTED_0) width = 0;
    beam_edge(shot, 0, wrap8(shot.start_x / 256U - width),
              wrap8(shot.end_x / 256U - width - 1), shot.start_y / 256U);
    beam_edge(shot, 1, wrap8(shot.start_x / 256U + width),
              wrap8(shot.end_x / 256U + width + 1), shot.start_y / 256U);

    // UpdateSpriteAnim_15/16 alternates five-particle banks. The latest spawn
    // supplies the shared horizontal drift, including for older particles.
    for (unsigned i = 0; i < shot.picture.size(); ++i) {
        auto& p = shot.particles[shot.first + i];
        if (p.phase && !--p.wait) {
            p.phase = static_cast<std::uint8_t>((p.phase + 1U) %
                FF6_ROM_VALUE_WORLD_JUDGMENT_PARTICLE_PHASES_0);
            p.wait = p.period;
        }
        p.frame = assets.falcon_splash_frames.at(FF6_ROM_VALUE_WORLD_JUDGMENT_FRAME_BASE_0 + p.phase);
        p.y = wrap8(p.y + (shot.reverse ? FF6_ROM_VALUE_WORLD_JUDGMENT_PARTICLE_Y_REVERSE_0
                                          : FF6_ROM_VALUE_WORLD_JUDGMENT_PARTICLE_Y_FORWARD_0));
        p.x = static_cast<std::uint16_t>(p.x + (shot.reverse ? int(shot.drift) : -int(shot.drift)));
        shot.picture[i] = p;
    }
    shot.random = wrap8(unsigned(shot.random) - 1);
    for (unsigned i = 0; i < shot.picture.size(); ++i) {
        auto& p = shot.particles[shot.first + i];
        if (p.phase) continue;
        const auto noise = [&](unsigned back) {
            return assets.battles.random[wrap8(unsigned(shot.random) - back)];
        };
        shot.drift = static_cast<std::uint8_t>(
            (noise(1) & FF6_ROM_VALUE_WORLD_JUDGMENT_DRIFT_MASK_0) + FF6_ROM_VALUE_WORLD_JUDGMENT_DRIFT_BASE_0);
        p.period = p.wait = static_cast<std::uint8_t>(
            (noise(2) & FF6_ROM_VALUE_WORLD_JUDGMENT_PERIOD_MASK_0) + 1);
        // The source's cleared carry subtracts one additional pixel here.
        p.y = wrap8(shot.start_y / 256U - (noise(3) & FF6_ROM_VALUE_WORLD_JUDGMENT_SPAWN_Y_MASK_0) -
                    (FF6_ROM_VALUE_WORLD_JUDGMENT_SPAWN_Y_BASE_0 + 1U));
        p.x = static_cast<std::uint16_t>(int(shot.start_x / 256U) +
            (shot.reverse ? -FF6_ROM_VALUE_WORLD_JUDGMENT_SPAWN_X_0 : FF6_ROM_VALUE_WORLD_JUDGMENT_SPAWN_X_0));
        p.phase = 1;
        break;
    }
    shot.first = static_cast<std::uint8_t>(shot.picture.size() - shot.first);
    const unsigned remaining = FF6_ROM_VALUE_WORLD_JUDGMENT_DURATION_0 - shot.frame,
                   target = remaining < FF6_ROM_VALUE_WORLD_JUDGMENT_FADE_REMAINING_0 ? remaining * 2 : 15;
    for (unsigned i = 0; i < 2; ++i) {
        if (shot.brightness < target) ++shot.brightness;
        else if (shot.brightness > target) --shot.brightness;
    }
    shot.finished = remaining == 0;
    return cue;
}
} // namespace ff6
