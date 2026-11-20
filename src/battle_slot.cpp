#include "ff6/battle_slot.hpp"
#include "rom-materials/battle_slot_forced_roll_mask.macros.inc"
#include "rom-materials/battle_slot_slip_allowance.macros.inc"
#include "rom-materials/battle_slot_reel_step.macros.inc"
#include "rom-materials/slot_reel_first_column.macros.inc"
#include "rom-materials/slot_reel_first_row.macros.inc"
#include "rom-materials/slot_reel_spacing_bytes.macros.inc"
#include "rom-materials/slot_reel_vertical_delta.macros.inc"
#include "rom-materials/battle_window_color_arithmetic.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition) throw std::invalid_argument(why);
}
}
BattleSlotData load_battle_slot(const ContentPack& pack) {
    BattleSlotData out;
    const auto reels = pack.get("battle_slot_reels"), rates = pack.get("battle_slot_rates"),
               tiles = pack.get("battle_slot_tile_bases"), colors = pack.get("battle_slot_palettes");
    require(reels.size() == 96 && rates.size() == 6 && tiles.size() == 16 && colors.size() == 48,
            "Invalid Slot resource extent");
    for (unsigned reel = 0; reel < 3; ++reel)
        for (unsigned i = 0; i < 16; ++i) {
            const auto symbol = read16(reels, (reel * 16 + i) * 2);
            require(symbol < 6, "Invalid Slot symbol");
            out.reels[reel][i] = static_cast<std::uint8_t>(symbol);
        }
    for (unsigned i = 0; i < 6; ++i) {
        out.rates[i] = rates[i];
        const auto tile = read16(tiles, i * 2) & 1023U;
        require(tile >= 0x240 && tile + 19 < 0x280, "Invalid Slot symbol tiles");
        out.tile_bases[i] = static_cast<std::uint8_t>(tile - 0x240);
    }
    for (unsigned i = 0; i < 8; ++i) out.gradient[i] = read16(colors, i * 2) & 32767U;
    for (unsigned i = 0; i < 12; ++i) out.colors[i] = read16(colors, 24 + i * 2) & 32767U;
    return out;
}
std::uint8_t battle_slot_outcome(std::array<std::uint8_t, 3> symbols) {
    require(std::ranges::all_of(symbols, [](auto s) { return s < 6; }), "Invalid Slot result");
    // C2/B4A3: a mixed pair of sevens and a bar is the losing Joker outcome.
    if (symbols[0] == symbols[1]) {
        if (symbols[1] == symbols[2]) return symbols[0] + 1;
        if (symbols[0] == 0 && symbols[2] == 2) return 0;
    }
    return 7;
}
std::array<std::uint8_t, 3> BattleSlotState::symbols(const BattleSlotData& data) const {
    return {data.reels[0][positions[0] >> 4U], data.reels[1][positions[1] >> 4U],
            data.reels[2][positions[2] >> 4U]};
}
std::optional<std::uint8_t> BattleSlotState::confirm(const BattleSlotData& data,
    bool joker_allowed, const std::array<std::uint8_t, 256>& random, std::uint8_t& random_index) {
    const auto symbol = symbols(data);
    if (!committed[0]) {
        roll = random[random_index];
        random_index = wrap8(unsigned{random_index} + 1U);
        if (!joker_allowed) roll |= FF6_ROM_VALUE_BATTLE_SLOT_FORCED_ROLL_MASK_0;
        committed[0] = true;
    } else if (!committed[1]) {
        if (stopped[0]) {
            desired_second = 255;
            if (!(roll & data.rates.at(symbol[0]))) {
                desired_second = symbol[0]; slip_remaining = FF6_ROM_VALUE_BATTLE_SLOT_SLIP_ALLOWANCE_0;
            }
            committed[1] = true;
        }
    } else if (!committed[2]) {
        if (stopped[1]) {
            desired_third = 255;
            if (symbol[0] == symbol[1]) {
                desired_third = symbol[0];
                if (roll & data.rates.at(symbol[0])) desired_third |= 128;
                else slip_remaining = FF6_ROM_VALUE_BATTLE_SLOT_SLIP_ALLOWANCE_0;
            }
            committed[2] = true;
        }
    } else if (stopped[2]) return battle_slot_outcome(symbol);
    return {};
}
void BattleSlotState::advance(const BattleSlotData& data) {
    // Slot publishes before advancing by the ROM-selected step. A press commits
    // a reel, but stopping waits for its next symbol boundary.
    visible_positions = positions;
    for (unsigned i = 0; i < 3; ++i) {
        if (stopped[i]) continue;
        positions[i] = wrap8(unsigned{positions[i]} - FF6_ROM_VALUE_BATTLE_SLOT_REEL_STEP_0);
        if (!committed[i] || (positions[i] & 15U)) continue;
        const auto symbol = data.reels[i][positions[i] >> 4U];
        const auto desired = i == 1 ? desired_second : desired_third;
        if (i == 0 || desired == 255) stopped[i] = true;
        else if (i == 2 && (desired & 128U)) stopped[i] = symbol != (desired & 127U);
        else if (symbol == desired || !slip_remaining) stopped[i] = true;
        else --slip_remaining;
    }
}
std::vector<TileLayer> make_battle_slot_layers(const BattleSlotData& data,
    const std::array<Tile, 64>& tiles, const TileLayer& panel, const BattleSlotState& state,
    const BattleMenuTransition& transition, unsigned fade) {
    require(fade <= 31, "Invalid Slot fade");
    auto shade = panel;
    // SlotGradientPal/SlotIconPal: both palettes are complemented, then faded.
    // Their difference gives the icon's original color minus reel shading.
    for (unsigned i = 0; i < 8; ++i)
        shade.palettes[0][i + 8] = blend_color(data.gradient[i] ^ 32767U,
            static_cast<Rgb555>(fade * 1057), ColorBlend::subtract);
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 32; ++x)
            if (y < FF6_ROM_VALUE_SLOT_REEL_FIRST_ROW_0 ||
                y >= FF6_ROM_VALUE_SLOT_REEL_FIRST_ROW_0 + 6 ||
                x < FF6_ROM_VALUE_SLOT_REEL_FIRST_COLUMN_0 ||
                x >= FF6_ROM_VALUE_SLOT_REEL_FIRST_COLUMN_0 +
                     2 * (FF6_ROM_VALUE_SLOT_REEL_SPACING_BYTES_0 / 2) + 4 ||
                (x - FF6_ROM_VALUE_SLOT_REEL_FIRST_COLUMN_0) %
                     (FF6_ROM_VALUE_SLOT_REEL_SPACING_BYTES_0 / 2) == 4)
                shade.cells[y * 32 + x].tile = 28; // Blank outside the three reel apertures.
    shade.mix = {static_cast<ColorBlend>(FF6_ROM_VALUE_BATTLE_WINDOW_COLOR_ARITHMETIC_0),
                 ColorBlend::none};
    shade.lines.assign(224, RasterLine{});
    bool visible = false;
    for (unsigned y = 0; y < 224; ++y) {
        const auto& binding = transition.windows[y];
        auto& line = shade.lines[y];
        line.enabled = binding.canvas == BattleWindowCanvas::slot;
        if (line.enabled) visible = true;
        line.offset_x = binding.offset_x; line.offset_y = binding.offset_y;
    }
    if (!visible) return {};
    std::vector<TileLayer> result;
    // Shade wins the equal-depth tie over the underlying frame's reel cells.
    for (auto& cell : shade.cells) cell.depth = 11;
    result.push_back(std::move(shade));
    for (unsigned reel = 0; reel < 3; ++reel) {
        TileLayer icons;
        icons.columns = 4; icons.rows = 32; icons.wrap_x = false;
        icons.visible = false; icons.operand = true;
        icons.tiles.assign(tiles.begin(), tiles.end()); icons.palettes.resize(1);
        for (auto& tile : icons.tiles) for (auto& pixel : tile) if (!pixel) pixel = 1;
        for (unsigned i = 0; i < 12; ++i)
            icons.palettes[0][i + 4] = blend_color(data.colors[i] ^ 32767U,
                static_cast<Rgb555>(fade * 1057), ColorBlend::subtract);
        // Transparent icon pixels subtract black, rather than exposing an
        // unrelated battlefield operand below the opaque Slot aperture.
        icons.palettes[0][1] = 0;
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 4; ++x)
                icons.cells.push_back({unsigned{data.tile_bases.at(data.reels[reel][y / 2])} +
                                       (y % 2) * 16 + x, 0, 30});
        icons.lines.assign(224, RasterLine{});
        for (unsigned y = 0; y < 224; ++y) {
            auto& line = icons.lines[y];
            const auto& binding = transition.windows[y];
            const int local_y = static_cast<int>(y) + binding.offset_y;
            const int left = FF6_ROM_VALUE_SLOT_REEL_FIRST_COLUMN_0 * 8 +
                static_cast<int>(reel) * (FF6_ROM_VALUE_SLOT_REEL_SPACING_BYTES_0 / 2) * 8 -
                binding.offset_x;
            // Six authored row calls fill each aperture; the row origin is ROM data.
            line.enabled = binding.canvas == BattleWindowCanvas::slot &&
                local_y >= FF6_ROM_VALUE_SLOT_REEL_FIRST_ROW_0 * 8 &&
                local_y < (FF6_ROM_VALUE_SLOT_REEL_FIRST_ROW_0 + 6) * 8;
            line.left = static_cast<std::uint16_t>(std::clamp(left, 0, 256));
            line.right = static_cast<std::uint16_t>(std::clamp(left + 32, 0, 256));
            line.offset_x = -left;
            // Original window and reel positions share the atlas coordinate
            // system. Convert their extracted difference to this named canvas;
            // its atlas origin is row 16, in the documented 8-pixel tile format.
            line.offset_y = binding.offset_y + state.visible_positions[reel] -
                (FF6_ROM_VALUE_SLOT_REEL_VERTICAL_DELTA_0 - 16 * 8);
        }
        result.push_back(std::move(icons));
    }
    return result;
}
} // namespace ff6
