#pragma once
#include "ff6/battle_display.hpp"
#include "ff6/menu_windows.hpp"

namespace ff6 {
struct BattleReadoutRectangle {
    std::uint8_t x = 0, y = 0, width = 0, height = 0;
};
struct BattleReadoutAssets {
    MenuWindowSkins skins;
    std::array<std::uint8_t, 36> window_pattern{};
    std::array<BattleReadoutRectangle, 2> windows{};
    std::array<BattleReadoutRectangle, 4> text_regions{};
    std::array<Bytes, 4> programs;
    std::array<Tile, 256> font{};
    std::array<Rgb555, 32> font_colors{};
    std::array<std::array<std::uint8_t, 10>, 384> monster_names{};
};
BattleReadoutAssets load_battle_readout_assets(const ContentPack& pack);
struct BattleReadoutCharacter {
    std::array<std::uint8_t, 6> name{};
    CombatVitals hp, mp;
    BattleCounterReadout counters;
};
struct BattleReadoutView {
    // Already compacted menu order, distinct from combat/drawing slots.
    std::array<std::optional<BattleReadoutCharacter>, 4> characters;
    std::array<std::optional<std::uint16_t>, 4> monsters;
    MenuWindowAppearance window;
    Rgb555 font_color = 32767;
    bool show_gauges = true;
};
// Closed-menu counter publication in compacted menu order. UpdateCharText
// prepares one row; the display publishes phase-1 before the next preparation.
struct BattleGaugePublication {
    std::array<BattleCounterReadout, 4> prepared{}, visible{};
    std::uint8_t phase = 0;
};
void advance_battle_gauge_publication(BattleGaugePublication& gauges,
                                      const std::array<BattleCounterReadout, 4>& counters);
BattleReadoutView make_battle_readout_view(const BattleParty& party, const BattleState& battle,
                                           bool hide_names, bool show_gauges);
struct BattleReadoutLayers {
    TileLayer windows, text;
};
struct BattleTextNames {
    std::array<std::array<std::uint8_t, 12>, 8> dances{};
    std::array<std::array<std::uint8_t, 13>, 256> items{};
    std::array<std::array<std::uint8_t, 7>, 16> item_symbols{};
    std::array<std::array<std::uint8_t, 7>, 32> commands{};
    std::array<std::array<std::uint8_t, 7>, 54> magic{};
    // English menu text reads a shifted eight-glyph span, not esper attack names.
    std::array<std::array<std::uint8_t, 8>, 27> esper{}, summon_names{};
    std::array<std::array<std::uint8_t, 10>, 175> attacks{};
};
enum class BattleTextLanguage { menu, list };
// Shared glyph and frame construction. These draw into a retained 32x8 canvas;
// the caller owns atomic publication and per-line display sampling.
void draw_battle_window(const BattleReadoutAssets& assets, BattleReadoutRectangle rectangle,
                        TileLayer& canvas);
void draw_battle_text(const BattleReadoutAssets& assets, const BattleReadoutView& view,
                      BattleReadoutRectangle region, ByteView program, TileLayer& canvas,
                      const BattleTextNames* names = nullptr,
                      BattleTextLanguage language = BattleTextLanguage::menu);
// Closed initial windows/readouts only. The caller owns menu opening and input.
// English text's two stored tile rows become twelve visible pixels per line.
BattleReadoutLayers make_battle_readout_layers(const BattleReadoutAssets& assets,
                                               const BattleReadoutView& view);
} // namespace ff6
