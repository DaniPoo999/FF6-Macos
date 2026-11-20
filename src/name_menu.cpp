#include "ff6/name_menu.hpp"
#include "rom-materials/name_portrait_x.macros.inc"
#include "rom-materials/name_portrait_y.macros.inc"
#include "rom-materials/name_indicator_y.macros.inc"
#include "rom-materials/name_indicator_x_offset.macros.inc"
#include "rom-materials/name_indicator_period.macros.inc"
#include "rom-materials/name_text_cell.macros.inc"
#include "rom-materials/menu_erase_sound.macros.inc"
#include "rom-materials/menu_mosaic_envelope.macros.inc"
#include "rom-materials/name_alphabet_cells.macros.inc"
#include "rom-materials/name_indicator_attribute.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
TileLayer image(std::span<const Tile> tiles, unsigned columns, const std::array<Rgb555, 16>& colors,
                int x, int y) {
    TileLayer result;
    result.columns = static_cast<std::uint16_t>(columns);
    result.rows = static_cast<std::uint16_t>(tiles.size() / columns);
    result.wrap_x = result.wrap_y = false;
    result.tiles.assign(tiles.begin(), tiles.end());
    result.palettes.push_back(colors);
    for (unsigned i = 0; i < tiles.size(); ++i)
        result.cells.push_back({i, 0, 10 * 256, false, false});
    result.lines.resize(224);
    for (auto& line : result.lines) {
        line.offset_x = -x;
        line.offset_y = -y;
    }
    return result;
}
} // namespace
NameMenuAssets load_name_menu(const ContentPack& pack) {
    NameMenuAssets out;
    out.shared = load_main_menu(pack);
    const auto letters = slice(pack.get("data_d8e8c8_menu_animation_and_letters"), 0, 70);
    std::copy(letters.begin(), letters.end(), out.letters.begin());
    const auto cursors = pack.get("menu_name_cursor");
    require(cursors.size() == 145 && cursors[0] == 1 && cursors[1] == 0 && cursors[2] == 0 &&
            cursors[3] == 10 && cursors[4] == 7, "Invalid name cursor layout");
    for (unsigned i = 0; i < 70; ++i) {
        require(letters[i] >= 0x80, "Unsupported name alphabet glyph");
        out.cursors[i] = {cursors[5 + i * 2], cursors[6 + i * 2]};
    }
    const auto windows = pack.get("menu_name_windows");
    require(windows.size() == 16, "Invalid name windows");
    for (unsigned i = 0; i < 4; ++i) {
        const auto pos = read16(windows, i * 4);
        require(pos >= 0x5849 && pos < 0x6049 && (pos - 0x5849) % 2 == 0,
                "Invalid name window position");
        const auto cell = (pos - 0x5849) / 2;
        out.windows[i] = {0, static_cast<std::uint8_t>(cell % 32),
                          static_cast<std::uint8_t>(cell / 32),
                          windows[i * 4 + 2], windows[i * 4 + 3]};
    }
    const auto heading = pack.get("menu_name_heading");
    require(heading.size() >= 10 && heading.back() == 0, "Invalid naming heading");
    std::copy_n(heading.begin(), 6, out.arrow_x.begin());
    const auto pos = read16(heading, 6);
    require(pos >= 0x4049 && pos < 0x4849 && (pos - 0x4049) % 2 == 0,
            "Invalid naming heading position");
    const auto cell = (pos - 0x4049) / 2;
    out.heading_x = static_cast<std::uint8_t>(cell % 32);
    out.heading_y = static_cast<std::uint8_t>(cell / 32);
    out.heading.assign(heading.begin() + 8, heading.end() - 1);
    const auto sprites = decode_tiles(pack.get("menu_graphics_menu_sprite_4bpp"), 4);
    const auto arrow = pack.get("menu_name_arrow");
    // Frame identity is resource metadata; placement and holds are ROM-selected.
    require(arrow.size() == 19 && read16(arrow, 10) == 0xb448 && arrow[12] == FF6_ROM_VALUE_NAME_INDICATOR_PERIOD_0 &&
                read16(arrow, 13) == 0xb44d && arrow[15] == FF6_ROM_VALUE_NAME_INDICATOR_PERIOD_0 &&
                read16(arrow, 16) == 0xb448 && arrow[18] == 255,
            "Invalid name indicator frames");
    for (unsigned i = 0; i < 2; ++i) {
        require(arrow[i * 5] == 1 && arrow[i * 5 + 1] == FF6_ROM_VALUE_NAME_INDICATOR_X_OFFSET_0 && arrow[i * 5 + 2] == 0 &&
                arrow[i * 5 + 4] == FF6_ROM_VALUE_NAME_INDICATOR_ATTRIBUTE_0 && arrow[i * 5 + 3] < sprites.size(),
                "Invalid name indicator sprite");
        out.arrows[i] = sprites[arrow[i * 5 + 3]];
    }
    return out;
}
NameMenu::NameMenu(std::array<std::uint8_t, 6> name, std::uint8_t portrait)
    : name_(name), portrait_(portrait) {
    require(portrait < 27, "Invalid naming portrait");
    require(std::ranges::all_of(name, [](auto v) { return v >= 0x80; }), "Invalid name glyph");
    while (position_ < 5 && name_[position_] != 0xff)
        ++position_;
}
std::vector<std::uint8_t> NameMenu::advance(const NameMenuAssets& assets, Buttons pressed,
                                          Buttons repeating) {
    require(((pressed | repeating) & ~Buttons{4095}) == 0, "Invalid naming buttons");
    std::vector<std::uint8_t> sounds;
    ++ticks_;
    if (invalid_ticks_)
        --invalid_ticks_;
    if (phase_ == NameMenuPhase::fading_in) {
        intensity_ = static_cast<std::uint8_t>(
            std::min(15U, unsigned{intensity_} + (intensity_ ? 2U : 1U)));
        if (intensity_ == 15)
            phase_ = NameMenuPhase::editing;
        return sounds;
    }
    if (phase_ == NameMenuPhase::fading_out) {
        intensity_ = static_cast<std::uint8_t>(intensity_ >= 2 ? intensity_ - 2 : 0);
        if (!intensity_)
            phase_ = NameMenuPhase::complete;
        return sounds;
    }
    if (phase_ != NameMenuPhase::editing)
        return sounds;
    // MoveCursor precedes the name edit. Only one direction wins per visit;
    // vertical edges clamp, horizontal edges wrap within the same row.
    const auto previous_row = row_, previous_column = column_;
    if (held(repeating, Button::up)) {
        if (row_)
            --row_;
    } else if (held(repeating, Button::down)) {
        if (row_ < 6)
            ++row_;
    } else if (held(repeating, Button::left)) {
        column_ = static_cast<std::uint8_t>((column_ + 9) % 10);
    } else if (held(repeating, Button::right)) {
        column_ = static_cast<std::uint8_t>((column_ + 1) % 10);
    }
    if (previous_row != row_ || previous_column != column_)
        sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
    if (held(pressed, Button::start)) {
        if (std::ranges::all_of(name_, [](auto v) { return v == 0xff; })) {
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
            invalid_ticks_ = FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0;
        } else {
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            phase_ = NameMenuPhase::fading_out;
        }
    } else if (held(pressed, Button::a)) {
        sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        name_[position_] = assets.letters[row_ * 10 + column_];
        if (position_ < 5)
            ++position_;
    } else if (held(pressed, Button::b)) {
        sounds.push_back(FF6_ROM_VALUE_MENU_ERASE_SOUND_0);
        if (position_ == 5 && name_[5] != 0xff) {
            name_[5] = 0xff;
        } else if (position_) {
            name_[position_] = 0xff;
            name_[--position_] = 0xff;
        }
    }
    return sounds;
}
RasterScene NameMenu::scene(const NameMenuAssets& assets, const MenuWindowAppearance& appearance,
                           Rgb555 font_color) const {
    const auto& shared = assets.shared;
    require(font_color <= 0x7fff && shared.sidebar_font.size() == 256, "Invalid naming font");
    RasterScene scene;
    scene.intensity.assign(224, intensity_);
    // DrawNameChangeMenu paints the name box last. Its bottom edge overlaps
    // the alphabet's top edge; resource declaration order would erase it.
    const std::array ordered{assets.windows[1], assets.windows[2],
                             assets.windows[3], assets.windows[0]};
    scene.layers.push_back(make_menu_windows(shared.windows, ordered, appearance));
    TileLayer text;
    text.columns = text.rows = 32;
    text.wrap_x = text.wrap_y = false;
    text.tiles = shared.sidebar_font;
    text.tiles.emplace_back();
    text.cells.assign(1024, {256, 0, 9 * 256, false, false});
    std::array<Rgb555, 16> colors{};
    std::copy_n(shared.text_colors.begin(), 4, colors.begin());
    colors[3] = font_color;
    text.palettes.push_back(colors);
    text.lines.resize(224);
    for (auto& line : text.lines)
        line.offset_y = 1;
    // The extracted row combines the authored origin, lower glyph displacement,
    // matching scroll bands and cursor spacing into native cell positions.
    const auto put = [&](unsigned x, unsigned y, std::uint8_t glyph) {
        require(x < 32 && y < 32, "Naming text outside canvas");
        text.cells[y * 32 + x].tile = glyph;
    };
    for (unsigned i = 0; i < assets.heading.size(); ++i)
        put(assets.heading_x + i, assets.heading_y, assets.heading[i]);
    for (unsigned i = 0; i < 6; ++i)
        put(FF6_ROM_VALUE_NAME_TEXT_CELL_0 % 32 + i, FF6_ROM_VALUE_NAME_TEXT_CELL_0 / 32, name_[i]);
    constexpr std::array<unsigned, 10> alphabet_cells{
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_0,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_1,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_2,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_3,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_4,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_5,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_6,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_7,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_8,
        FF6_ROM_VALUE_NAME_ALPHABET_CELLS_9};
    constexpr auto alphabet_pitch = alphabet_cells[1] - alphabet_cells[0];
    constexpr auto alphabet_gap = [=] {
        for (unsigned col = 1; col < alphabet_cells.size(); ++col)
            if (alphabet_cells[col] - alphabet_cells[col - 1] != alphabet_pitch) return col;
        return static_cast<unsigned>(alphabet_cells.size());
    }();
    static_assert(alphabet_gap > 0 && alphabet_gap < alphabet_cells.size());
    constexpr auto gap_width = alphabet_cells[alphabet_gap] - alphabet_cells[alphabet_gap - 1] - alphabet_pitch;
    // Preserve the native compiler's supported row expression while requiring
    // every decoded cell to agree. Neither pitch nor split is a retained preset.
    static_assert([=] {
        for (unsigned col = 0; col < alphabet_cells.size(); ++col)
            if (alphabet_cells[col] != alphabet_cells[0] + col * alphabet_pitch +
                    (col >= alphabet_gap) * gap_width) return false;
        return true;
    }());
    constexpr auto alphabet_row_pitch =
        (FF6_ROM_VALUE_NAME_ALPHABET_CELLS_10 - alphabet_cells[0]) / 32;
    static_assert((FF6_ROM_VALUE_NAME_ALPHABET_CELLS_10 - alphabet_cells[0]) % 32 == 0);
    constexpr std::array<unsigned, 70> alphabet_grid{
#include "rom-materials/name_alphabet_cells.inc"
    };
    static_assert([=] {
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned col = 0; col < 10; ++col)
                if (alphabet_grid[row * 10 + col] !=
                        alphabet_cells[col] + row * alphabet_row_pitch * 32) return false;
        return true;
    }());
    for (unsigned row = 0; row < 7; ++row)
        for (unsigned col = 0; col < 10; ++col)
            put(alphabet_cells[0] % 32 + col * alphabet_pitch + (col >= alphabet_gap) * gap_width,
                alphabet_cells[0] / 32 + row * alphabet_row_pitch, assets.letters[row * 10 + col]);
    scene.layers.push_back(std::move(text));
    const auto cursor = assets.cursors[row_ * 10 + column_];
    scene.layers.push_back(image(shared.cursor_tiles, 2, shared.cursor_colors, cursor[0], cursor[1]));
    scene.layers.push_back(image(std::span{&assets.arrows[((ticks_ ? ticks_ - 1 : 0) / FF6_ROM_VALUE_NAME_INDICATOR_PERIOD_0) % 2], 1},
                                 1, shared.cursor_colors,
                                 assets.arrow_x[position_] + FF6_ROM_VALUE_NAME_INDICATOR_X_OFFSET_0,
                                 FF6_ROM_VALUE_NAME_INDICATOR_Y_0));
    std::array<Tile, 25> portrait{};
    const auto graphic = shared.portrait_graphics[portrait_];
    for (unsigned i = 0; i < 25; ++i)
        portrait[i] = shared.portraits.at(graphic * 25 + shared.portrait_order[i]);
    scene.layers.push_back(image(portrait, 5,
                                 shared.portrait_colors[shared.portrait_palettes[portrait_]],
                                 FF6_ROM_VALUE_NAME_PORTRAIT_X_0,
                                 FF6_ROM_VALUE_NAME_PORTRAIT_Y_0));
    if (invalid_ticks_) {
        const auto phase = unsigned{FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0} - invalid_ticks_;
        const auto size = static_cast<std::uint8_t>(phase < FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_1
            ? phase + FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_2
            : FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0 - phase);
        // Menu mosaic affects backgrounds only; the portrait and cursors stay sharp.
        for (auto& layer : std::span{scene.layers}.first(2))
            for (unsigned row = 0; row < 224; ++row) {
                layer.lines[row].sample_width = size;
                layer.lines[row].sample_row = static_cast<int>(row - row % size);
            }
    }
    return scene;
}
} // namespace ff6
