#pragma once
#include "ff6/main_menu.hpp"
#include "ff6/input.hpp"

namespace ff6 {
struct NameMenuAssets {
    MainMenuAssets shared{};
    std::array<std::uint8_t, 70> letters{};
    std::array<std::array<std::uint8_t, 2>, 70> cursors{};
    std::array<MenuWindowPlacement, 4> windows{};
    std::array<std::uint8_t, 6> arrow_x{};
    std::array<Tile, 2> arrows{};
    Bytes heading;
    std::uint8_t heading_x = 0, heading_y = 0;
};
NameMenuAssets load_name_menu(const ContentPack& pack);
enum class NameMenuPhase { fading_in, editing, fading_out, complete };
// English six-glyph naming contract. Selection edits the active position;
// B erases, Start accepts a nonblank name. There is no cancellation branch.
class NameMenu {
  public:
    NameMenu(std::array<std::uint8_t, 6> name, std::uint8_t portrait);
    std::vector<std::uint8_t> advance(const NameMenuAssets& assets, Buttons pressed, Buttons repeating);
    RasterScene scene(const NameMenuAssets& assets, const MenuWindowAppearance& window,
                      Rgb555 font_color) const;
    const auto& name() const { return name_; }
    unsigned position() const { return position_; }
    unsigned row() const { return row_; }
    unsigned column() const { return column_; }
    NameMenuPhase phase() const { return phase_; }
  private:
    std::array<std::uint8_t, 6> name_;
    std::uint8_t portrait_, position_ = 0, row_ = 0, column_ = 0, intensity_ = 0;
    unsigned ticks_ = 0, invalid_ticks_ = 0;
    NameMenuPhase phase_ = NameMenuPhase::fading_in;
};
} // namespace ff6
