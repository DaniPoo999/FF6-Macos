#pragma once
#include "ff6/field_menu.hpp"

namespace ff6 {
// C3/AB4D: only enabled, assigned, ordinary characters enter the twelve-person
// lineup. Selection changes battle order, never persistent field membership.
class FinalOrderSelection {
  public:
    explicit FinalOrderSelection(const Roster& roster);
    bool select(unsigned row);
    void undo();
    void reset();
    void finish();
    const auto& available() const { return available_; }
    const auto& order() const { return order_; }
    unsigned count() const { return count_; }
  private:
    std::array<std::optional<std::uint8_t>, 12> available_{}, order_{};
    unsigned count_ = 0;
};
class FinalOrderMenu {
  public:
    FinalOrderMenu(const ContentPack& pack, FieldMenuValues values, Buttons held_buttons);
    FieldMenuUpdate advance(Buttons buttons);
    RasterScene scene() const;
    const FinalOrderSelection& selection() const { return selection_; }
    unsigned cursor() const { return cursor_; }
    bool complete() const { return closing_ && intensity_ == 0; }
  private:
    MenuLayoutAssets assets_;
    FieldMenuValues values_;
    FinalOrderSelection selection_;
    InputState input_;
    unsigned cursor_ = 0;
    std::uint8_t intensity_ = 0;
    bool closing_ = false;
};
} // namespace ff6
