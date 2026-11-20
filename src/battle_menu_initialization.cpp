#include "ff6/battle_menu_surfaces.hpp"
#include "rom-materials/battle_window_priority.macros.inc"
#include "rom-materials/battle_mp_window_priority.macros.inc"
#include "rom-materials/slot_reel_cell_origins.macros.inc"
#include "rom-materials/slot_reel_flip_row.macros.inc"
#include "rom-materials/slot_reel_priority.macros.inc"
#include "rom-materials/battle_window_palette_lift.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
BattleWindowInitialization make_battle_window_initialization(
    const BattleMenuSurfaceAssets& a, const BattleReadoutView& view,
    const std::array<Rgb555, 8>& retained_slot_colors) {
    if (view.window.skin >= a.readout.skins.size() ||
        std::ranges::any_of(retained_slot_colors, [](auto c) { return c > 32767; }))
        throw std::invalid_argument("Invalid initial battle window appearance");
    const auto& skin = a.readout.skins[view.window.skin];
    const auto colors = view.window.colors.value_or(skin.colors);
    if (std::ranges::any_of(colors, [](auto c) { return c > 32767; }))
        throw std::invalid_argument("Invalid battle window color");
    TileLayer work;
    work.columns = 32; work.rows = 8;
    work.wrap_x = work.wrap_y = false;
    work.tiles.assign(skin.tiles.begin(), skin.tiles.end());
    work.tiles.emplace_back(); // Explicitly cleared graphics at this initialization boundary.
    work.tiles.insert(work.tiles.end(), a.slot_tiles.begin(), a.slot_tiles.end());
    work.palettes.resize(2);
    for (unsigned i = 0; i < 7; ++i)
        work.palettes[0][i + 1] = blend_color(colors[i], FF6_ROM_VALUE_BATTLE_WINDOW_PALETTE_LIFT_0 * 1057U, ColorBlend::add);
    std::ranges::copy(retained_slot_colors, work.palettes[0].begin() + 8);
    // Palette 1 is reserved for reset cells. Their texels are all transparent;
    // its colors are immaterial here, unlike the retained upper slot colors.
    const auto reset = [&] { work.cells.assign(256, RasterCell{28, 1, 4}); };
    const auto draw = [&](BattleReadoutRectangle r, std::uint16_t depth = 4 + 3 * FF6_ROM_VALUE_BATTLE_WINDOW_PRIORITY_0) {
        draw_battle_window(a.readout, r, work);
        for (unsigned y = r.y; y < unsigned{r.y} + r.height; ++y)
            for (unsigned x = r.x; x < unsigned{r.x} + r.width; ++x) {
                auto& cell = work.cells[y * 32 + x];
                cell.palette = 0; cell.depth = depth;
                cell.flip_x = cell.flip_y = false;
            }
    };
    const auto closed = [&] { for (auto r : a.readout.windows) draw(r); };
    BattleWindowInitialization out;
    reset(); closed(); out.panels[0] = work;
    draw(a.windows[0]); out.command_cache[0] = work;
    closed(); draw(a.windows[12]); out.command_cache[2] = work;
    closed(); draw(a.windows[11]); out.command_cache[1] = work;
    out.panels[1] = work;
    draw(a.windows[1]);
    // The original setup emits three four-cell runs over six consecutive rows.
    // Extraction validates every destination and attribute before deriving the
    // placement and flip boundary. Tile identities share the same source rows.
    constexpr std::array<unsigned, 6> first{
#include "rom-materials/slot_reel_initial_tiles.inc"
};
    for (unsigned row = 0; row < 6; ++row)
        for (unsigned reel = 0; reel < 3; ++reel)
            for (unsigned x = 0; x < 4; ++x)
                work.cells[row * 32 + FF6_ROM_VALUE_SLOT_REEL_CELL_ORIGINS_0 +
                           reel * (FF6_ROM_VALUE_SLOT_REEL_CELL_ORIGINS_1 - FF6_ROM_VALUE_SLOT_REEL_CELL_ORIGINS_0) + x] =
                    {29 + first[row] + x, 0, 4 + 3 * FF6_ROM_VALUE_SLOT_REEL_PRIORITY_0,
                     false, row >= FF6_ROM_VALUE_SLOT_REEL_FLIP_ROW_0};
    out.panels[2] = work;
    draw(a.windows[2]); out.panels[3] = work;
    draw(a.windows[5]); draw(a.windows[3], 4 + 3 * FF6_ROM_VALUE_BATTLE_MP_WINDOW_PRIORITY_0); out.panels[4] = work;
    draw(a.windows[4]); out.panels[5] = work;
    draw(a.windows[6]); out.panels[6] = work;
    for (unsigned panel = 7; panel < 9; ++panel) {
        reset(); draw(a.windows[panel], 5 + 3 * FF6_ROM_VALUE_BATTLE_WINDOW_PRIORITY_0);
        for (auto& cell : work.cells) if (cell.tile == 28) cell.depth = 5;
        out.panels[panel] = work;
    }
    return out;
}
} // namespace ff6
