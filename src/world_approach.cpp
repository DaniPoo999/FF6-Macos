#include "ff6/world_map.hpp"
#include "rom-materials/approach_duration.macros.inc"
#include "rom-materials/approach_forward_step.macros.inc"
#include "rom-materials/approach_spotlight_height.macros.inc"
#include "rom-materials/approach_palette_bank.macros.inc"
#include "rom-materials/approach_viewport.macros.inc"
#include "rom-materials/approach_red_gradient.macros.inc"
#include "rom-materials/approach_cyan_step.macros.inc"
#include "rom-materials/approach_cyan_intercept.macros.inc"
#include "rom-materials/approach_skyline_blend.macros.inc"
#include "rom-materials/approach_terrain_blend.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
constexpr auto skyline_arithmetic = static_cast<ColorBlend>(FF6_ROM_VALUE_APPROACH_SKYLINE_BLEND_0);
constexpr auto terrain_arithmetic = static_cast<ColorBlend>(FF6_ROM_VALUE_APPROACH_TERRAIN_BLEND_0);
// UpdateSpotlightHDMA draws a byte-coordinate line from the skyline to its
// fixed base. Its midpoint error rule picks the edge pixel before stepping x
// on shallow slopes, and after stepping x on steep slopes.
void spotlight_edge(WorldApproach& scene, unsigned edge, unsigned top, unsigned bottom,
                    unsigned base_y) {
    const int step = top >= bottom ? -1 : 1;
    const unsigned dx = top >= bottom ? top - bottom : bottom - top;
    int x = static_cast<int>(top);
    unsigned row = 0;
    const auto put = [&] {
        if (row < scene.windows.size()) scene.windows[row][edge] = wrap8(static_cast<unsigned>(x));
        ++row;
    };
    if (dx > base_y) {
        int error = static_cast<int>(dx / 2);
        for (unsigned i = 0; i < dx; ++i) {
            error -= static_cast<int>(base_y);
            if (error < 0) { error += static_cast<int>(dx); put(); }
            x += step;
        }
    } else {
        int error = static_cast<int>(base_y / 2);
        for (unsigned i = 0; i < base_y; ++i) {
            error -= static_cast<int>(dx);
            if (error < 0) { error += static_cast<int>(base_y); x += step; }
            put();
        }
    }
    for (unsigned y = base_y + 1; y < scene.windows.size(); ++y)
        scene.windows[y][edge] = edge % 2 ? 0 : 255;
}
} // namespace
void advance_world_approach(const WorldAssets& assets, WorldApproach& scene, WorldState& world) {
    if (scene.frame >= FF6_ROM_VALUE_APPROACH_DURATION_0) return;
    if (!scene.frame)
        for (auto& row : scene.windows) row = {255, 0, 255, 0};
    auto& vehicle = world.vehicle.value();
    if (scene.skyline_scroll + FF6_ROM_VALUE_APPROACH_SKYLINE_SCROLL_2 <
        FF6_ROM_VALUE_APPROACH_SKYLINE_SCROLL_1)
        scene.skyline_scroll += FF6_ROM_VALUE_APPROACH_SKYLINE_SCROLL_2;
    const unsigned y = (unsigned{world.y} * 256 + vehicle.fraction_y + FF6_ROM_VALUE_APPROACH_FORWARD_STEP_0) & 0xfffffU;
    world.y = static_cast<std::uint16_t>(y / 256);
    vehicle.fraction_y = static_cast<std::uint8_t>(y & 255U);
    // The source visits one of four pairs per picture; two pairs share each
    // visible window. Do not update all searchlights simultaneously.
    const unsigned pair = (scene.frame + 1U) & 3U, window = (pair & 1U) * 2;
    constexpr std::array<unsigned, 8> bases{
#include "rom-materials/approach_spotlight_bases.inc"
};
    spotlight_edge(scene, window, scene.tops[pair * 2], bases[pair * 2], FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0);
    spotlight_edge(scene, window + 1, scene.tops[pair * 2 + 1], bases[pair * 2 + 1], FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0);
    constexpr std::array<unsigned, 4> rates{
#include "rom-materials/approach_spotlight_rates.inc"
}, centers{
#include "rom-materials/approach_spotlight_centers.inc"
}, widths{
#include "rom-materials/approach_spotlight_widths.inc"
};
    auto& angle = scene.angles[pair];
    angle = static_cast<std::uint16_t>((angle + rates[pair]) % 360);
    const int offset = static_cast<int>(assets.sine.at(angle % 180) / 8) * (angle < 180 ? 1 : -1);
    const int center = static_cast<int>(centers[pair]) + offset;
    scene.tops[pair * 2 + 1] = wrap8(static_cast<unsigned>(center + static_cast<int>(widths[pair])));
    // The preceding byte addition clears carry at these bounded coordinates;
    // SBC therefore subtracts width plus one for the left edge.
    scene.tops[pair * 2] = wrap8(static_cast<unsigned>(center - static_cast<int>(widths[pair]) - 1));
    ++scene.frame;
}
void compose_world_approach(const WorldAssets& assets, const WorldApproach& approach,
                            RasterScene& scene) {
    if (assets.approach_cells.size() != 2048 || assets.approach_tiles.empty())
        throw std::invalid_argument("Missing Vector approach skyline");
    TileLayer skyline;
    skyline.columns = skyline.rows = 32;
    skyline.tiles = assets.approach_tiles;
    skyline.palettes.resize(8);
    skyline.palettes[FF6_ROM_VALUE_APPROACH_PALETTE_BANK_0] = assets.approach_colors;
    skyline.cells.resize(1024);
    for (unsigned i = 0; i < 1024; ++i) {
        const unsigned cell = read16(assets.approach_cells, i * 2);
        skyline.cells[i] = {cell & 1023U, (cell >> 10U) & 7U, 2,
                            (cell & 16384U) != 0, (cell & 32768U) != 0};
    }
    skyline.lines.resize(224);
    // The top span adds the skyline to itself inside either searchlight;
    // outside the windows it keeps the base color. The lower terrain span
    // subtracts a fixed gradient. These are independent native blend owners.
    AffineLayer lighting;
    lighting.columns = 32; lighting.rows = 28;
    lighting.tiles.resize(896); lighting.cells.resize(896); lighting.lines.resize(224);
    lighting.visible = false; lighting.operand = true;
    std::copy(assets.approach_colors.begin(), assets.approach_colors.end(), lighting.colors.begin());
    for (unsigned i = 0; i < 896; ++i) lighting.cells[i] = i;
    scene.operand_fallback_rows.resize(224);
    scene.backdrop_mix = {terrain_arithmetic, terrain_arithmetic};
    // No secondary coverage outside a searchlight means leave the base pixel
    // unchanged. This is the native identity operation, not a stored color.
    skyline.mix = {skyline_arithmetic, ColorBlend::none};
    for (unsigned y = 0; y < 224; ++y) {
        auto& row = skyline.lines[y];
        row.enabled = y < FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0;
        row.left = FF6_ROM_VALUE_APPROACH_VIEWPORT_0;
        row.right = FF6_ROM_VALUE_APPROACH_VIEWPORT_1 + 1;
        // Arithmetic floor for a negative initial signed scroll byte.
        row.offset_y = (approach.skyline_scroll < 0
                           ? -((-approach.skyline_scroll + 255) / 256)
                           : approach.skyline_scroll / 256) + 1;
        lighting.lines[y].enabled = y < FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0;
        lighting.lines[y].source_y = static_cast<int>(y * 256);
        lighting.lines[y].left = FF6_ROM_VALUE_APPROACH_VIEWPORT_0;
        lighting.lines[y].right = FF6_ROM_VALUE_APPROACH_VIEWPORT_1 + 1;
        const auto& windows = approach.windows[y];
        for (unsigned x = FF6_ROM_VALUE_APPROACH_VIEWPORT_0;
             y < FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0 && x < FF6_ROM_VALUE_APPROACH_VIEWPORT_1 + 1;
             ++x) {
            const bool lit = (x >= windows[0] && x <= windows[1]) ||
                             (x >= windows[2] && x <= windows[3]);
            const unsigned sy = static_cast<unsigned>(static_cast<int>(y) + row.offset_y) & 255U;
            const auto& cell = skyline.cells[sy / 8 * 32 + x / 8];
            const unsigned px = cell.flip_x ? 7 - x % 8 : x % 8;
            const unsigned py = cell.flip_y ? 7 - sy % 8 : sy % 8;
            lighting.tiles[y / 8 * 32 + x / 8][y % 8 * 8 + x % 8] =
                lit ? assets.approach_tiles.at(cell.tile)[py * 8 + px] : 0;
        }
        if (y >= FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0) {
            const unsigned distance = y - (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0 - 1);
            const unsigned red = (unsigned{FF6_ROM_VALUE_APPROACH_RED_GRADIENT_0} -
                                  distance * FF6_ROM_VALUE_APPROACH_RED_GRADIENT_1) / 256;
            // The extracted affine intercept also covers the compiler-folded
            // constant. Convert back to the first-sample origin for this loop.
            const unsigned cyan = (unsigned{FF6_ROM_VALUE_APPROACH_CYAN_INTERCEPT_0} -
                (FF6_ROM_VALUE_APPROACH_SPOTLIGHT_HEIGHT_0 - 1U) * FF6_ROM_VALUE_APPROACH_CYAN_STEP_0 -
                distance * FF6_ROM_VALUE_APPROACH_CYAN_STEP_0) / 256;
            scene.operand_fallback_rows[y] = static_cast<Rgb555>(red | (cyan << 5) | (cyan << 10));
        }
    }
    for (auto& ground : scene.affine_layers)
        ground.mix = {terrain_arithmetic, terrain_arithmetic};
    scene.affine_layers.push_back(std::move(lighting));
    scene.layers.push_back(std::move(skyline));
}
} // namespace ff6
