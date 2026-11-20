#pragma once
#include "ff6/colosseum.hpp"
#include "ff6/field_menu.hpp"
#include "ff6/battle_background.hpp"
#include "ff6/battle_monster_graphics.hpp"

namespace ff6 {
enum class ColosseumPage { wager, challenger, closed };
// Event 9A owns this modal menu. It returns a validated selection, never a
// battle outcome; event AF starts combat after the field continuation resumes.
class ColosseumMenu {
  public:
    ColosseumMenu(const ContentPack&, FieldMenuValues, bool shadow_waiting, Buttons held);
    FieldMenuUpdate advance(Buttons);
    RasterScene scene() const;
    bool complete() const { return page_ == ColosseumPage::closed && !intensity_; }
    ColosseumPage page() const { return page_; }
    bool ready() const { return intensity_ == 15; }
    unsigned cursor() const { return cursor_; }
    const FieldMenuValues& values() const { return values_; }
    const std::optional<ColosseumSelection>& selection() const { return selection_; }
  private:
    MenuLayoutAssets assets_;
    MenuItemData items_;
    ColosseumData data_;
    FieldMenuValues values_;
    BattleBackground background_;
    BattleMonsterGraphicsData monsters_;
    std::array<std::uint8_t, 256> alignment_{};
    std::array<std::array<Tile, 6>, 23> actors_{};
    std::array<std::array<Rgb555, 16>, 23> colors_{};
    std::array<Tile, 8> versus_{};
    std::array<RasterLine, 224> challenger_rows_{};
    InputState input_;
    ColosseumPage page_ = ColosseumPage::wager;
    std::optional<ColosseumSelection> selection_;
    bool shadow_waiting_ = false;
    unsigned cursor_ = 0, scroll_ = 0, ticks_ = 0;
    std::uint8_t intensity_ = 0;
    const Character* character(unsigned position) const;
};
} // namespace ff6
