#include "ff6/battle_readout.hpp"
#include "rom-materials/battle_window_color_arithmetic.macros.inc"
#include "rom-materials/battle_window_gradient_geometry.macros.inc"
#include "rom-materials/battle_window_priority.macros.inc"
#include "rom-materials/battle_window_palette_lift.macros.inc"
#include "rom-materials/battle_window_gradient_period.macros.inc"
#include "rom-materials/battle_atb_palette_flags.macros.inc"
#include "rom-materials/battle_text_initial_flags.macros.inc"
#include "rom-materials/battle_text_layout.macros.inc"
#include "rom-materials/battle_readout_window_rows.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool condition, const char* reason) {
    if (!condition)
        throw std::invalid_argument(reason);
}
void validate(BattleReadoutRectangle r) {
    require(r.width > 0 && r.height > 0 && unsigned{r.x} + r.width <= 32 &&
                unsigned{r.y} + r.height <= 8,
            "Battle readout rectangle exceeds canvas");
}
// Scoped battle text languages. Unsupported commands never disappear silently.
class ReadoutText {
  public:
    ReadoutText(const BattleReadoutAssets& assets, const BattleReadoutView& view,
                BattleReadoutRectangle region, TileLayer& output, const BattleTextNames* names,
                BattleTextLanguage language)
        : assets_(assets), view_(view), region_(region), output_(output), names_(names),
          language_(language) {}
    void run(ByteView script) {
        std::size_t position = 0;
        const auto next = [&]() {
            require(position < script.size(), "Truncated battle readout command");
            return script[position++];
        };
        while (position < script.size()) {
            const auto command = next();
            if (command == 0) {
                require(position == script.size(), "Trailing battle readout program data");
                return;
            }
            if (command >= 32) {
                glyph(command);
            } else if (language_ == BattleTextLanguage::menu && command == 1) {
                row_ += 2;
                column_ = 0;
            } else if (command == 3) {
                glyph(next());
            } else if (command == 4) {
                flags_ = next();
            } else if (command == 5) {
                const auto count = next();
                spaces(count == 0 ? 256 : count);
            } else if (language_ == BattleTextLanguage::list && command == 2) {
                const auto quantity = next();
                require(quantity <= 99, "Invalid displayed item quantity");
                glyph(quantity >= 10 ? static_cast<std::uint8_t>(180 + quantity / 10) : 255);
                glyph(quantity ? static_cast<std::uint8_t>(180 + quantity % 10) : 255);
            } else if (language_ == BattleTextLanguage::list && (command == 14 || command == 18)) {
                const auto identity = next();
                require(names_ != nullptr, "Missing item names");
                if (command == 14) {
                    if (identity == 255)
                        spaces(13);
                    else
                        name(names_->items[identity]);
                } else if (identity == 255 || names_->items[identity][0] == 255) {
                    spaces(7); // The English ListTextCmd_12 blank-width quirk.
                } else {
                    const unsigned symbol = names_->items[identity][0];
                    require(symbol >= 216 && symbol < 216 + names_->item_symbols.size(),
                            "Invalid item symbol");
                    name(names_->item_symbols[symbol - 216]);
                }
            } else if (language_ == BattleTextLanguage::list && command == 6) {
                const auto identity = next();
                if (identity == 255)
                    spaces(10);
                else {
                    require(names_ && identity < 8, "Invalid Magitek name identity");
                    name(names_->attacks[50U + identity]);
                }
            } else if (language_ == BattleTextLanguage::menu && command == 22) {
                // English command layouts use the original unused-command
                // handler as explicit padding. It consumes no operand.
            } else if (language_ == BattleTextLanguage::menu && command == 13) {
                const auto identity = next();
                if (identity == 255)
                    spaces(7);
                else {
                    require(names_ && identity < names_->commands.size(),
                            "Invalid battle command name");
                    name(names_->commands[identity]);
                }
            } else if ((command == 15 ||
                        (language_ == BattleTextLanguage::menu && command == 17))) {
                const auto identity = next();
                if (identity == 255)
                    spaces(7);
                else {
                    require(names_ != nullptr, "Missing battle attack names");
                    if (identity < 54)
                        name(names_->magic[identity]);
                    else if (identity < 81)
                        name(names_->esper[identity - 54U]);
                    else
                        name(names_->attacks[identity - 81U]);
                }
                if (command == 17 && (identity == 255 || identity < 54))
                    spaces(3);
            } else if (language_ == BattleTextLanguage::list && command == 26) {
                const auto identity = next();
                if (identity == 255) spaces(8);
                else {
                    require(names_ && identity < 27, "Invalid summon menu identity");
                    name(names_->summon_names[identity]);
                }
            } else if (language_ == BattleTextLanguage::list && command == 22) {
                const auto value = next();
                glyph(value >= 100 ? static_cast<std::uint8_t>(180 + value / 100) : 255);
                glyph(value >= 10 ? static_cast<std::uint8_t>(180 + value / 10 % 10) : 255);
                glyph(static_cast<std::uint8_t>(180 + value % 10));
            } else if (language_ == BattleTextLanguage::list && command == 23) {
                const auto identity = next();
                if (identity == 255) spaces(12);
                else {
                    require(names_ && identity < 8, "Invalid Dance menu identity");
                    name(names_->dances[identity]);
                }
            } else if (language_ == BattleTextLanguage::list && command == 24) {
                const auto identity = next();
                if (identity == 255) spaces(10);
                else name(assets_.monster_names[identity]);
                glyph(255); // English ListTextCmd_18 publishes an eleventh blank.
            } else if (language_ == BattleTextLanguage::menu && command >= 7 && command <= 10) {
                character(command - 7U, next());
            } else if (language_ == BattleTextLanguage::menu && command == 11) {
                const auto slot = next();
                require(slot < 4, "Invalid battle monster name row");
                if (const auto identity = view_.monsters[slot]) {
                    require(*identity < assets_.monster_names.size(),
                            "Invalid monster name identity");
                    for (auto c : assets_.monster_names[*identity])
                        glyph(c);
                } else
                    spaces(10);
                glyph(255);
            } else
                throw std::invalid_argument("Unsupported battle readout command");
        }
        throw std::invalid_argument("Unterminated battle readout program");
    }

  private:
    template <std::size_t N> void name(const std::array<std::uint8_t, N>& text) {
        for (const auto value : text)
            glyph(value);
    }
    void glyph(std::uint8_t value) {
        require(column_ < region_.width && row_ + 1 < region_.height,
                "Battle readout text exceeds its region");
        require((flags_ & 0xe3U) == 0x21U, "Unsupported battle readout glyph bank/priority");
        const auto at = (unsigned{region_.y} + row_) * 32 + region_.x + column_++;
        const auto palette = (flags_ >> 2U) & 7U;
        require(palette != 4, "Battle text selects an unowned retained palette");
        output_.cells[at] = {255, palette, 10};
        output_.cells[at + 32] = {value, palette, 10};
    }
    void spaces(unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            glyph(255);
    }
    void number(std::uint16_t value, unsigned digits) {
        // The thousands byte is not reduced modulo ten. TrimZeroes3 also
        // examines it before the caller skips it, even for out-of-range MP.
        std::array<std::uint8_t, 4> text{static_cast<std::uint8_t>(180U + value / 1000U),
                                         static_cast<std::uint8_t>(180U + (value / 100U) % 10U),
                                         static_cast<std::uint8_t>(180U + (value / 10U) % 10U),
                                         static_cast<std::uint8_t>(180U + value % 10U)};
        bool leading = true;
        for (unsigned n = 0; n < 3; ++n) {
            if (text[n] != 180)
                leading = false;
            if (leading)
                text[n] = digits == 4 ? 255 : 200;
        }
        for (unsigned n = 4 - digits; n < 4; ++n)
            glyph(text[n]);
    }
    void gauge(std::uint8_t value) {
        glyph(249);
        const int filled = (value >> 3U) + 1;
        for (int column = 0; column < 4; ++column)
            glyph(static_cast<std::uint8_t>(240 + std::clamp(filled - column * 8, 0, 8)));
        glyph(250);
    }
    void character(unsigned slot, std::uint8_t kind) {
        constexpr std::array<unsigned, 7> widths{
#include "rom-materials/battle_readout_widths.inc"
        };
        require(kind < widths.size(), "Invalid battle character readout kind");
        const auto& c = view_.characters[slot];
        if (!c) {
            spaces(widths[kind]);
            return;
        }
        switch (kind) {
        case 0:
            for (auto value : c->name)
                glyph(value);
            break;
        case 1:
            number(c->hp.current, 4);
            break;
        case 2:
            if (view_.show_gauges) {
                const auto saved = flags_;
                // The ROM attributes, rather than the stale palette comment, select the colors.
                flags_ = c->counters.atb == 255 ? FF6_ROM_VALUE_BATTLE_ATB_PALETTE_FLAGS_0 : FF6_ROM_VALUE_BATTLE_ATB_PALETTE_FLAGS_1;
                gauge(c->counters.atb);
                flags_ = saved;
            } else {
                glyph(206);
                number(c->hp.maximum, 4);
                glyph(255);
            }
            break;
        case 3:
            number(c->mp.current, 3);
            break;
        case 4:
            number(c->mp.maximum, 3);
            break;
        case 5:
            gauge(c->counters.morph);
            break;
        case 6:
            gauge(c->counters.condemned);
            break;
        }
    }
    const BattleReadoutAssets& assets_;
    const BattleReadoutView& view_;
    BattleReadoutRectangle region_;
    TileLayer& output_;
    const BattleTextNames* names_;
    BattleTextLanguage language_;
    unsigned row_ = 0, column_ = 0;
    std::uint8_t flags_ = FF6_ROM_VALUE_BATTLE_TEXT_INITIAL_FLAGS_0;
};
} // namespace

void draw_battle_text(const BattleReadoutAssets& assets, const BattleReadoutView& view,
                      BattleReadoutRectangle region, ByteView program, TileLayer& canvas,
                      const BattleTextNames* names, BattleTextLanguage language) {
    validate(region);
    require(canvas.columns == 32 && canvas.rows == 8 && canvas.cells.size() == 256,
            "Invalid battle text canvas");
    ReadoutText(assets, view, region, canvas, names, language).run(program);
}
void draw_battle_window(const BattleReadoutAssets& assets, BattleReadoutRectangle rectangle,
                        TileLayer& canvas) {
    require(canvas.columns == 32 && canvas.rows == 8 && canvas.cells.size() == 256,
            "Invalid battle window canvas");
    const auto& p = assets.window_pattern;

    validate(rectangle);
    require(rectangle.width >= 2 && rectangle.height >= 2, "Battle window lacks borders");
    const auto set = [&](unsigned x, unsigned y, unsigned pattern_index) {
        require(p[pattern_index] < 28, "Invalid battle window tile");
        canvas.cells[(rectangle.y + y) * 32 + rectangle.x + x].tile = p[pattern_index];
    };
    for (unsigned y = 0; y < rectangle.height; ++y)
        for (unsigned x = 0; x < rectangle.width; ++x)
            set(x, y,
                y == 0                      ? 16 + x % 4
                : y + 1 == rectangle.height ? 20 + x % 4
                                            : ((y - 1) % 4) * 4 + x % 4);
    for (unsigned y = 0; y < rectangle.height; ++y) {
        set(0, y, 28 + y % 2);
        set(rectangle.width - 1U, y, 32 + y % 2);
    }
    set(0, 0, 24);
    set(rectangle.width - 1U, 0, 25);
    set(0, rectangle.height - 1U, 26);
    set(rectangle.width - 1U, rectangle.height - 1U, 27);
}

void advance_battle_gauge_publication(BattleGaugePublication& gauges,
                                      const std::array<BattleCounterReadout, 4>& counters) {
    require(gauges.phase < 4, "Invalid battle gauge publication phase");
    const auto row = (unsigned{gauges.phase} + 3U) & 3U;
    gauges.visible[row] = gauges.prepared[row];
    gauges.phase = static_cast<std::uint8_t>((unsigned{gauges.phase} + 1U) & 3U);
    gauges.prepared[gauges.phase] = counters[gauges.phase];
}

BattleReadoutAssets load_battle_readout_assets(const ContentPack& pack) {
    BattleReadoutAssets a;
    a.skins = load_menu_window_skins(pack);
    const auto font = decode_tiles(pack.get("menu_graphics_small_font_2bpp"), 2);
    const auto palette = pack.get("menu_graphics_battle_font_pal_pal");
    const auto names = pack.get("text_monster_name_bin");
    const auto pattern = pack.get("battle_window_pattern");
    const auto windows = pack.get("battle_readout_windows");
    const auto positions = pack.get("battle_readout_positions");
    const auto programs = pack.get("battle_readout_programs");
    require(font.size() == 256 && palette.size() == 64 && names.size() == 3840 &&
                pattern.size() == 36 && windows.size() == 8 && positions.size() == 32 &&
                programs.size() == 56,
            "Invalid battle readout resource extent");
    std::ranges::copy(font, a.font.begin());
    for (unsigned i = 0; i < 32; ++i)
        a.font_colors[i] = read16(palette, i * 2) & 32767U;
    for (unsigned i = 0; i < 384; ++i)
        std::copy_n(names.begin() + i * 10, 10, a.monster_names[i].begin());
    for (unsigned i = 0; i < 36; ++i) {
        require(pattern[i] >= 32 && pattern[i] < 60, "Invalid battle window pattern tile");
        a.window_pattern[i] = static_cast<std::uint8_t>(pattern[i] - 32U);
    }
    for (unsigned i = 0; i < 2; ++i) {
        const auto address = read16(windows, i * 4 + 2);
        require(address >= 0x8d13 && (address - 0x8d13) % 2 == 0,
                "Invalid battle window position encoding");
        const auto cell = (address - 0x8d13U) / 2U;
        require(cell < 256, "Battle window position exceeds canvas");
        a.windows[i] = {static_cast<std::uint8_t>(cell % 32), static_cast<std::uint8_t>(cell / 32),
                        windows[i * 4], windows[i * 4 + 1]};
        validate(a.windows[i]);
    }
    constexpr std::array<unsigned, 5> starts{0, 16, 32, 44, 56};
    for (unsigned i = 0; i < 4; ++i) {
        const auto address = read16(positions, i * 8 + 2);
        require(address >= 0x5855 && (address - 0x5855) % 2 == 0,
                "Invalid battle text position encoding");
        const auto cell = (address - 0x5855U) / 2U;
        const auto width = read16(positions, i * 8 + 4), height = read16(positions, i * 8 + 6);
        require(cell < 256 && width <= 64 && width % 2 == 0 && height <= 8,
                "Invalid battle text region");
        a.text_regions[i] = {
            static_cast<std::uint8_t>(cell % 32), static_cast<std::uint8_t>(cell / 32),
            static_cast<std::uint8_t>(width / 2), static_cast<std::uint8_t>(height)};
        validate(a.text_regions[i]);
        a.programs[i].assign(programs.begin() + starts[i], programs.begin() + starts[i + 1]);
    }
    return a;
}
BattleReadoutView make_battle_readout_view(const BattleParty& party, const BattleState& battle,
                                           bool hide_names, bool show_gauges) {
    BattleReadoutView view;
    view.show_gauges = show_gauges;
    unsigned row = 0;
    if (!hide_names)
        for (unsigned i = 0; i < 4; ++i) {
            const auto& slot = party.slots[i];
            if (slot.graphics && slot.present && !slot.enemy) {
                const auto& display = battle.character_display[i];
                view.characters[row++] = BattleReadoutCharacter{slot.name, display.hp, display.mp,
                                                                battle.display.counters[i]};
            }
        }
    for (unsigned i = 0; i < 4; ++i)
        view.monsters[i] = battle.display.names[i].identity;
    return view;
}
BattleReadoutLayers make_battle_readout_layers(const BattleReadoutAssets& assets,
                                               const BattleReadoutView& view) {
    require(view.window.skin < 8 && view.font_color <= 32767, "Invalid battle readout appearance");
    BattleReadoutLayers out;
    auto& window = out.windows;
    const auto& skin = assets.skins[view.window.skin];
    window.columns = 32;
    window.rows = 8;
    window.wrap_x = window.wrap_y = false;
    window.tiles.assign(skin.tiles.begin(), skin.tiles.end());
    window.tiles.emplace_back();
    window.cells.assign(256, RasterCell{28, 0, 4 + 3 * FF6_ROM_VALUE_BATTLE_WINDOW_PRIORITY_0});
    window.palettes.resize(1);
    const auto colors = view.window.colors.value_or(skin.colors);
    for (unsigned i = 0; i < 7; ++i)
        window.palettes[0][i + 1] = blend_color(colors[i], FF6_ROM_VALUE_BATTLE_WINDOW_PALETTE_LIFT_0 * 1057U, ColorBlend::add);
    for (const auto rectangle : assets.windows)
        draw_battle_window(assets, rectangle, window);
    window.lines.resize(224);
    for (unsigned y = 0; y < 224; ++y) {
        auto& line = window.lines[y];
        line.enabled = y >= FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1 && y < FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_0;
        line.offset_y = -FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1;
        line.blend = static_cast<ColorBlend>(FF6_ROM_VALUE_BATTLE_WINDOW_COLOR_ARITHMETIC_0);
        line.operand = static_cast<Rgb555>((y >= FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0 ? (y - FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0) / FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_PERIOD_0 : 0) * 1057);
    }
    auto& text = out.text;
    text.columns = 32;
    text.rows = 8;
    text.wrap_x = text.wrap_y = false;
    text.tiles.assign(assets.font.begin(), assets.font.end());
    text.cells.assign(256, RasterCell{255, 0, 10});
    text.palettes.resize(8);
    for (unsigned palette = 0; palette < 8; ++palette)
        for (unsigned c = 0; c < 4; ++c)
            text.palettes[palette][c] = assets.font_colors[palette * 4 + c];
    text.palettes[0][3] = view.font_color;
    for (unsigned i = 0; i < 4; ++i) {
        validate(assets.text_regions[i]);
        draw_battle_text(assets, view, assets.text_regions[i], assets.programs[i], text);
    }
    text.lines.resize(224);
    for (unsigned y = 0; y < 224; ++y) {
        auto& line = text.lines[y];
        line.enabled = y >= FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_1 && y < FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_0;
        // The original visible/skipped bands compress the stored glyph rows.
        line.offset_y = y >= FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_1
            ? static_cast<std::int32_t>((y - FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_1) /
                FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_2 * FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_3) - FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_4 : 0;
    }
    return out;
}
} // namespace ff6
