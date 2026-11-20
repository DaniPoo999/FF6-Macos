#pragma once
#include "ff6/main_menu.hpp"
#include <map>
#include <string>

namespace ff6 {
struct MenuCaption {
    std::uint8_t column = 0, row = 0, page = 0;
    Bytes glyphs;
    bool body = false;
};
struct MenuLayoutAssets {
    MainMenuAssets shared;
    std::map<std::string, MenuCaption, std::less<>> captions;
    std::map<std::string, MenuWindowPlacement, std::less<>> windows;
    std::map<std::string, Bytes, std::less<>> data;
    std::array<std::array<std::uint8_t, 10>, 8> blitz_inputs{};
    std::array<Bytes, 256> item_names;
    std::array<Bytes, 17> item_symbols;
    std::array<Bytes, 256> item_descriptions;
    std::array<Bytes, 20> rare_names, rare_descriptions;
    std::array<Bytes, 256> attack_names;
    std::array<Bytes, 54> magic_descriptions;
    std::array<Bytes, 32> command_names;
    std::array<Bytes, 27> esper_names, esper_descriptions;
    std::array<Bytes, 8> bushido_names, dance_names, blitz_descriptions, bushido_descriptions;
    std::array<Bytes, 24> lore_descriptions;
    std::array<Bytes, 17> esper_bonus_names, esper_bonus_descriptions;
    std::array<Bytes, 384> monster_names;
    std::vector<Glyph> large_font;
    Bytes widths;
    std::vector<Tile> sprites;
    std::array<Rgb555, 16> status_colors{};
};
MenuLayoutAssets load_menu_layouts(const ContentPack& pack);
// Menu text is a native positioned graphic, independent of tilemap pages and
// video transfers. Callers supply recovered visible geometry for each screen.
class MenuCanvas {
  public:
    MenuCanvas(const MenuLayoutAssets& assets, const MenuWindowAppearance& appearance,
               Rgb555 font_color, std::uint8_t intensity = 15);
    void windows(std::initializer_list<std::string_view> names);
    void windows(std::span<const std::string_view> names);
    void caption(std::string_view name, unsigned color = 0, int x_offset = 0, int y_offset = 0);
    void text(ByteView glyphs, int x, int y, unsigned color = 0);
    void number(std::uint32_t value, unsigned digits, int x, int y, unsigned color = 0);
    void inventory_name(std::uint8_t item, int x, int y, unsigned color = 0);
    void description(ByteView glyphs, int x, int y, unsigned revealed = 256);
    void cursor(int x, int y);
    void page_arrow(bool up, int x, int y);
    void scroll_indicator(unsigned position, unsigned maximum, unsigned rate, int x, int y,
                          unsigned ticks);
    void portrait(std::uint8_t identity, int x, int y);
    void rectangle(MenuWindowPlacement placement);
    void glyph(std::uint8_t code, int x, int y, unsigned color = 0, bool flip_x = false,
               bool flip_y = false);
    void color_bar(unsigned amount, int x, int y);
    void elements(std::uint8_t mask, int x, int y);
    void status(std::uint8_t conditions, std::uint8_t persistent, int x, int y);
    void swatch(Rgb555 color, int x, int y);
    void actor(std::span<const Tile> tiles, const std::array<Rgb555, 16>& colors, int x, int y) {
        image(tiles, 2, colors, x, y);
        // Menu actors use the priority below foreground text. Cursor and
        // portrait images retain their separate foreground ownership.
        scene_.objects->sprites.front().depth = 10 * 256;
    }
    RasterScene finish();

  private:
    const MenuLayoutAssets& assets_;
    MenuWindowAppearance appearance_;
    Rgb555 font_color_;
    RasterScene scene_;
    void image(std::span<const Tile> tiles, unsigned columns, const std::array<Rgb555, 16>& colors,
               int x, int y);
};
} // namespace ff6
