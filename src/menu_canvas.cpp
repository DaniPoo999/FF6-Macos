#include "ff6/menu_canvas.hpp"
#include "rom-materials/canvas_inventory_separator.macros.inc"
#include "rom-materials/canvas_scroll_period.macros.inc"
#include "rom-materials/canvas_scroll_frames.macros.inc"
#include "rom-materials/canvas_page_arrow_tile.macros.inc"
#include "rom-materials/canvas_status_tiles.macros.inc"
#include "rom-materials/canvas_status_pitch.macros.inc"
#include "rom-materials/canvas_colorbar_tiles.macros.inc"
#include "rom-materials/canvas_status_step.macros.inc"
#include "rom-materials/canvas_colorbar_step.macros.inc"
#include "rom-materials/canvas_status_end.macros.inc"
#include "rom-materials/canvas_element_row.macros.inc"
#include "rom-materials/canvas_element_limit.macros.inc"
#include "rom-materials/canvas_wounded_palette.macros.inc"
#include <algorithm>

namespace ff6 {
MenuLayoutAssets load_menu_layouts(const ContentPack& pack) {
    MenuLayoutAssets result;
    result.shared = load_main_menu(pack);
    for (const auto& resource : pack.resources) {
        if (resource.id.starts_with("field_menu_label_")) {
            const auto& b = resource.bytes;
            if (b.size() < 3 || b.back() != 0)
                throw std::invalid_argument("Invalid menu caption");
            const auto pos = read16(b, 0);
            const unsigned base = pos < 0x5849 ? 0x3849 : 0x7849;
            if (pos < base || pos >= base + 8192 || ((pos - base) & 1U))
                throw std::invalid_argument("Invalid menu caption position");
            const auto cell = (pos - base) / 2;
            result.captions.emplace(resource.id.substr(17),
                                    MenuCaption{static_cast<std::uint8_t>(cell % 32),
                                                static_cast<std::uint8_t>((cell % 1024) / 32),
                                                static_cast<std::uint8_t>(cell / 1024),
                                                Bytes(b.begin() + 2, b.end() - 1), base == 0x3849});
        } else if (resource.id.starts_with("field_menu_data_")) {
            result.data.emplace(resource.id.substr(16), resource.bytes);
        } else if (resource.id.starts_with("field_menu_window_")) {
            const auto& b = resource.bytes;
            if (b.size() != 4)
                throw std::invalid_argument("Invalid menu window");
            const auto pos = read16(b, 0);
            if (pos < 0x5849 || pos >= 0x7849 || ((pos - 0x5849) & 1U))
                throw std::invalid_argument("Invalid menu window position");
            const auto cell = (pos - 0x5849) / 2;
            // A page is a layout identity only. Native callers select the page's
            // windows directly, so each selected window starts at local x/y.
            result.windows.emplace(
                resource.id.substr(18),
                MenuWindowPlacement{0, static_cast<std::uint8_t>(cell % 32),
                                    static_cast<std::uint8_t>((cell % 1024) / 32), b[2], b[3]});
        }
    }
    const auto names = pack.get("text_item_name_bin");
    if (names.size() != 256 * 13)
        throw std::invalid_argument("Invalid item names");
    for (unsigned i = 0; i < 256; ++i) {
        const auto name = names.subspan(i * 13, 13);
        result.item_names[i].assign(name.begin(), name.end());
    }
    const auto symbols = pack.get("text_item_symbol_name_bin");
    if (symbols.size() % 7 || symbols.size() > 17 * 7)
        throw std::invalid_argument("Invalid item symbol names");
    for (unsigned i = 0; i < symbols.size() / 7; ++i)
        result.item_symbols[i].assign(symbols.begin() + i * 7, symbols.begin() + (i + 1) * 7);
    const auto decode_descriptions = [&](auto& destination, const char* pointer_id,
                                         const char* text_id) {
        const auto pointers = pack.get(pointer_id), bytes = pack.get(text_id);
        if (pointers.size() < destination.size() * 2 || pointers.size() % 2)
            throw std::invalid_argument("Invalid description pointers");
        // Padded esper/bonus tables end with one-past-end pointers for unused
        // identities. They are sentinels, not descriptions to dereference.
        for (unsigned i = static_cast<unsigned>(destination.size()) * 2; i < pointers.size();
             i += 2)
            if (read16(pointers, i) != bytes.size())
                throw std::invalid_argument("Invalid unused description sentinel");
        for (unsigned i = 0; i < destination.size(); ++i) {
            auto at = std::size_t{read16(pointers, i * 2)};
            while (at < bytes.size() && bytes[at]) {
                if ((bytes[at] < 128 && bytes[at] != 1) || destination[i].size() >= 128)
                    throw std::invalid_argument("Unsupported menu description glyph");
                destination[i].push_back(bytes[at++]);
            }
            if (at == bytes.size())
                throw std::invalid_argument("Unterminated menu description");
        }
    };
    decode_descriptions(result.item_descriptions, "text_item_desc_ptr_bin", "text_item_desc_bin");
    decode_descriptions(result.rare_descriptions, "text_rare_item_desc_ptr_bin",
                        "text_rare_item_desc_bin");
    decode_descriptions(result.magic_descriptions, "text_magic_desc_ptr_bin",
                        "text_magic_desc_bin");
    decode_descriptions(result.esper_descriptions, "text_genju_attack_desc_ptr_bin",
                        "text_genju_attack_desc_bin");
    decode_descriptions(result.blitz_descriptions, "text_blitz_desc_ptr_bin",
                        "text_blitz_desc_bin");
    decode_descriptions(result.bushido_descriptions, "text_bushido_desc_ptr_bin",
                        "text_bushido_desc_bin");
    decode_descriptions(result.lore_descriptions, "text_lore_desc_ptr_bin", "text_lore_desc_bin");
    decode_descriptions(result.esper_bonus_descriptions, "text_genju_bonus_desc_ptr_bin",
                        "text_genju_bonus_desc_bin");
    const auto fixed = [&](auto& destination, const char* id, unsigned first, unsigned count,
                           unsigned width) {
        const auto bytes = pack.get(id);
        if (bytes.size() != count * width || first + count > destination.size())
            throw std::invalid_argument(std::string{"Invalid menu names: "} + id);
        for (unsigned i = 0; i < count; ++i)
            destination[first + i].assign(bytes.begin() + i * width,
                                          bytes.begin() + (i + 1) * width);
    };
    fixed(result.attack_names, "text_magic_name_bin", 0, 54, 7);
    fixed(result.attack_names, "text_genju_attack_name_bin", 54, 27, 10);
    fixed(result.attack_names, "text_attack_name_bin", 81, 175, 10);
    fixed(result.command_names, "text_battle_cmd_name_bin", 0, 32, 7);
    fixed(result.esper_names, "text_genju_name_bin", 0, 27, 8);
    fixed(result.bushido_names, "text_bushido_name_bin", 0, 8, 12);
    fixed(result.dance_names, "text_dance_name_bin", 0, 8, 12);
    fixed(result.esper_bonus_names, "text_genju_bonus_name_bin", 0, 17, 9);
    fixed(result.monster_names, "text_monster_name_bin", 0, 384, 10);
    const auto rare = pack.get("text_rare_item_name_bin");
    if (rare.size() != 20 * 13)
        throw std::invalid_argument("Invalid rare item names");
    for (unsigned i = 0; i < 20; ++i)
        result.rare_names[i].assign(rare.begin() + i * 13, rare.begin() + (i + 1) * 13);
    result.large_font = decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
    const auto widths = pack.get("font_tables_widths");
    result.widths.assign(widths.begin(), widths.end());
    result.sprites = decode_tiles(pack.get("menu_graphics_menu_sprite_4bpp"), 4);
    const auto palette = pack.get("interface_palettes_menu_pal_pal");
    for (unsigned i = 0; i < 4; ++i)
        result.status_colors[12 + i] = read16(palette, 192 + i * 2) & 32767U;
    result.data["colorbarpal"] = Bytes(palette.begin() + 128, palette.begin() + 160);
    const auto animation = pack.get("data_d8e8c8_menu_animation_and_letters");
    result.data["elementsymbols"] = Bytes(animation.begin() + 70, animation.begin() + 79);
    const auto blitz = pack.get("data_c47a40_blitz_code");
    if (blitz.size() != 96)
        throw std::invalid_argument("Invalid Blitz inputs");
    for (unsigned i = 0; i < 8; ++i)
        std::copy_n(blitz.begin() + i * 12, 10, result.blitz_inputs[i].begin());
    return result;
}
MenuCanvas::MenuCanvas(const MenuLayoutAssets& assets, const MenuWindowAppearance& appearance,
                       Rgb555 font_color, std::uint8_t intensity)
    : assets_(assets), appearance_(appearance), font_color_(font_color) {
    if (font_color > 32767 || intensity > 15)
        throw std::invalid_argument("Invalid menu colors");
    scene_.intensity.assign(224, intensity);
    scene_.objects.emplace();
    scene_.objects->lines.resize(224);
}
void MenuCanvas::windows(std::initializer_list<std::string_view> names) {
    windows(std::span{names.begin(), names.size()});
}
void MenuCanvas::windows(std::span<const std::string_view> names) {
    std::vector<MenuWindowPlacement> placements;
    for (auto name : names)
        placements.push_back(assets_.windows.at(std::string{name}));
    scene_.layers.push_back(make_menu_windows(assets_.shared.windows, placements, appearance_));
}
void MenuCanvas::rectangle(MenuWindowPlacement placement) {
    scene_.layers.push_back(
        make_menu_windows(assets_.shared.windows, std::span{&placement, 1}, appearance_));
}
void MenuCanvas::glyph(std::uint8_t code, int x, int y, unsigned color, bool flip_x, bool flip_y) {
    const std::array bytes{code};
    text(bytes, x, y, color);
    auto& cell = scene_.layers.back().cells.front();
    cell.flip_x = flip_x;
    cell.flip_y = flip_y;
}
void MenuCanvas::color_bar(unsigned amount, int x, int y) {
    if (amount > 31)
        throw std::invalid_argument("Invalid menu color component");
    const auto& colors = assets_.data.at("colorbarpal");
    constexpr auto partial_start = FF6_ROM_VALUE_CANVAS_COLORBAR_TILES_3;
    constexpr auto partial_step = FF6_ROM_VALUE_CANVAS_COLORBAR_STEP_0;
    for (int i = -1; i <= 8; ++i) {
        glyph(255, x + i * 8, y);
        auto& layer = scene_.layers.back();
        const auto tile = i < 0                       ? FF6_ROM_VALUE_CANVAS_COLORBAR_TILES_0
                          : i == 8                    ? FF6_ROM_VALUE_CANVAS_COLORBAR_TILES_1
                          : unsigned(i) < amount / 4  ? FF6_ROM_VALUE_CANVAS_COLORBAR_TILES_2
                          : unsigned(i) == amount / 4 ? partial_start + (amount % 4) * partial_step
                                                      : partial_start;
        layer.tiles[0] = assets_.shared.body_font[tile];
        for (unsigned p = 0; p < 16; ++p)
            layer.palettes[0][p] = read16(colors, p * 2) & 32767U;
    }
}
void MenuCanvas::elements(std::uint8_t mask, int x, int y) {
    const auto& codes = assets_.data.at("elementsymbols");
    unsigned column = 0;
    for (unsigned i = 0; i < 8 && column < FF6_ROM_VALUE_CANVAS_ELEMENT_LIMIT_0; ++i)
        if (mask & (128U >> i)) {
            const std::array code{codes.at(i)};
            description(code, x + static_cast<int>(column++) * int{FF6_ROM_VALUE_CANVAS_ELEMENT_ROW_0} * 4, y);
        }
}
void MenuCanvas::status(std::uint8_t conditions, std::uint8_t persistent, int x, int y) {
    if (conditions & 128U) {
        text(assets_.shared.wounded_label, x, y + 7, FF6_ROM_VALUE_CANVAS_WOUNDED_PALETTE_0);
        return;
    }
    // Extraction authenticates every frame's equal stride and the last endpoint.
    constexpr auto first_tile = FF6_ROM_VALUE_CANVAS_STATUS_TILES_0;
    constexpr auto tile_step = FF6_ROM_VALUE_CANVAS_STATUS_STEP_0;
    static_assert(first_tile + 7 * tile_step == FF6_ROM_VALUE_CANVAS_STATUS_END_0);
    const auto mask = (conditions & 0x70U) | ((conditions & 7U) << 1) | (persistent & 128U);
    unsigned column = 0;
    for (unsigned i = 0; i < 7; ++i)
        if (mask & (128U >> i)) {
            std::array<Tile, 4> tiles;
            for (unsigned q = 0; q < 4; ++q)
                tiles[q] = assets_.sprites.at(first_tile + i * tile_step + (q / 2) * 16 + q % 2);
            image(tiles, 2, assets_.status_colors,
                  x + int(column++) * int{FF6_ROM_VALUE_CANVAS_STATUS_PITCH_0}, y);
        }
}
void MenuCanvas::swatch(Rgb555 color, int x, int y) {
    glyph(255, x, y);
    auto& layer = scene_.layers.back();
    layer.tiles[0].fill(1);
    layer.palettes[0][1] = color;
}
void MenuCanvas::text(ByteView glyphs, int x, int y, unsigned color) {
    if (color >= 8 || glyphs.size() > 64)
        throw std::invalid_argument("Invalid menu text");
    if (glyphs.empty())
        return;
    TileLayer layer;
    layer.columns = static_cast<std::uint16_t>(glyphs.size());
    layer.rows = 1;
    layer.wrap_x = layer.wrap_y = false;
    layer.palettes.resize(1);
    std::copy_n(assets_.shared.text_colors.begin() + color * 4, 4, layer.palettes[0].begin());
    if (color == 0)
        layer.palettes[0][3] = font_color_;
    bool ended = false;
    for (auto glyph : glyphs) {
        ended = ended || glyph == 0;
        layer.cells.push_back({static_cast<std::uint32_t>(layer.tiles.size()), 0, 11 * 256});
        layer.tiles.push_back(ended ? Tile{} : assets_.shared.sidebar_font.at(glyph));
    }
    layer.lines.resize(224);
    for (auto& line : layer.lines) {
        line.offset_x = -x;
        line.offset_y = -y;
    }
    scene_.layers.push_back(std::move(layer));
}
void MenuCanvas::caption(std::string_view name, unsigned color, int x, int y) {
    const auto& label = assets_.captions.at(std::string{name});
    text(label.glyphs, label.column * 8 + x, label.row * 8 - 1 + y, color);
}
void MenuCanvas::number(std::uint32_t value, unsigned digits, int x, int y, unsigned color) {
    text(menu_number(value, digits, digits), x, y, color);
}
void MenuCanvas::inventory_name(std::uint8_t item, int x, int y, unsigned color) {
    // LoadListItemName (C3/80B9) appends a colon after the thirteen-cell name,
    // including its icon and padding. Quantities belong to the caller.
    text(assets_.item_names[item], x, y, color);
    glyph(FF6_ROM_VALUE_CANVAS_INVENTORY_SEPARATOR_0, x + 13 * 8, y, color);
}
void MenuCanvas::image(std::span<const Tile> tiles, unsigned columns,
                       const std::array<Rgb555, 16>& colors, int x, int y) {
    if (!columns || tiles.size() % columns)
        throw std::invalid_argument("Invalid menu image");
    RasterSprite sprite;
    sprite.x = x;
    sprite.y = y;
    sprite.width = static_cast<std::uint16_t>(columns * 8);
    sprite.height = static_cast<std::uint16_t>(tiles.size() / columns * 8);
    sprite.depth = 12 * 256;
    sprite.colors = colors;
    sprite.pixels.resize(sprite.width * sprite.height);
    for (unsigned i = 0; i < tiles.size(); ++i)
        for (unsigned py = 0; py < 8; ++py)
            for (unsigned px = 0; px < 8; ++px)
                sprite.pixels[(i / columns * 8 + py) * sprite.width + i % columns * 8 + px] =
                    tiles[i][py * 8 + px];
    // The cursor/portrait object plane precedes text when they overlap.
    scene_.objects->sprites.insert(scene_.objects->sprites.begin(), std::move(sprite));
}
void MenuCanvas::description(ByteView text, int x, int y, unsigned revealed) {
    RasterSprite sprite;
    sprite.x = x;
    sprite.y = y;
    sprite.width = 224;
    sprite.height = 28;
    sprite.depth = 11 * 256;
    sprite.colors[1] = font_color_;
    sprite.colors[2] = 0;
    sprite.pixels.resize(224 * 28);
    unsigned column = 0, line = 0, visited = 0;
    for (auto letter : text) {
        if (visited++ >= revealed)
            break;
        if (letter == 1) {
            column = 0;
            line = 16;
            continue;
        }
        if (letter < 128 || line > 16)
            throw std::invalid_argument("Invalid description layout");
        const auto& glyph = assets_.large_font.at(letter - 128);
        for (unsigned row = 0; row < 11; ++row)
            for (unsigned pixel = 0; pixel < 12; ++pixel)
                if (column + pixel < 224 && (glyph[row] & (1U << (11U - pixel)))) {
                    sprite.pixels[(line + row) * 224 + column + pixel] = 1;
                    if (column + pixel + 1 < 224 &&
                        !sprite.pixels[(line + row + 1) * 224 + column + pixel + 1])
                        sprite.pixels[(line + row + 1) * 224 + column + pixel + 1] = 2;
                }
        column += assets_.widths.at(letter - 96);
    }
    TileLayer layer;
    layer.columns = 28;
    layer.rows = 4;
    layer.wrap_x = layer.wrap_y = false;
    layer.palettes.push_back(sprite.colors);
    for (unsigned tile_y = 0; tile_y < 4; ++tile_y)
        for (unsigned tile_x = 0; tile_x < 28; ++tile_x) {
            Tile tile{};
            for (unsigned py = 0; py < 8 && tile_y * 8 + py < 28; ++py)
                for (unsigned px = 0; px < 8; ++px)
                    tile[py * 8 + px] = sprite.pixels[(tile_y * 8 + py) * 224 + tile_x * 8 + px];
            layer.cells.push_back({static_cast<std::uint32_t>(layer.tiles.size()), 0, 11 * 256});
            layer.tiles.push_back(tile);
        }
    layer.lines.resize(224);
    for (auto& line : layer.lines) {
        line.offset_x = -x;
        line.offset_y = -y;
    }
    scene_.layers.push_back(std::move(layer));
}
void MenuCanvas::cursor(int x, int y) {
    image(assets_.shared.cursor_tiles, 2, assets_.shared.cursor_colors, x, y);
}
void MenuCanvas::page_arrow(bool up, int x, int y) {
    image(std::span<const Tile>{assets_.sprites}.subspan(FF6_ROM_VALUE_CANVAS_PAGE_ARROW_TILE_0, 1),
          1, assets_.shared.cursor_colors, x, y);
    scene_.objects->sprites.front().flip_y = up;
}
void MenuCanvas::scroll_indicator(unsigned position, unsigned maximum, unsigned rate, int x, int y,
                                  unsigned ticks) {
    if (!maximum)
        return;
    if (position > maximum || maximum > 255 || rate > 65535)
        throw std::invalid_argument("Invalid menu scroll indicator");
    // ScrollArrowTask uses an eight-fractional-bit visual position and a
    // ROM-selected light/dark pulse. Its fixed-point rate belongs to the layout.
    y += static_cast<int>((position * rate) >> 8);
    const bool lit = (ticks / FF6_ROM_VALUE_CANVAS_SCROLL_PERIOD_0) % 2 != 0;
    for (unsigned half = 0; half < 2; ++half) {
        const bool available = half ? position < maximum : position != 0;
        const unsigned tile = lit && available ? FF6_ROM_VALUE_CANVAS_SCROLL_FRAMES_0
                                               : FF6_ROM_VALUE_CANVAS_SCROLL_FRAMES_1;
        image(std::span<const Tile>{assets_.sprites}.subspan(tile, 1), 1,
              assets_.shared.cursor_colors, x,
              y + static_cast<int>(half) * int{FF6_ROM_VALUE_CANVAS_SCROLL_FRAMES_2});
        scene_.objects->sprites.front().flip_y = half != 0;
    }
}
void MenuCanvas::portrait(std::uint8_t id, int x, int y) {
    if (id >= 27)
        throw std::invalid_argument("Invalid menu portrait");
    const auto& shared = assets_.shared;
    std::array<Tile, 25> tiles;
    for (unsigned i = 0; i < 25; ++i)
        tiles[i] =
            shared.portraits.at(shared.portrait_graphics[id] * 25 + shared.portrait_order[i]);
    image(tiles, 5, shared.portrait_colors[shared.portrait_palettes[id]], x, y);
    scene_.objects->sprites.front().depth = 7 * 256;
}
RasterScene MenuCanvas::finish() {
    return std::move(scene_);
}
} // namespace ff6
