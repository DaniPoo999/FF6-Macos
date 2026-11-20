#pragma once
#include "ff6/field_menu.hpp"
#include "ff6/field_sprites.hpp"

namespace ff6 {
// Roster membership is the authoritative result. Reserve cells retain their
// visual order while party cells are rebuilt from each member's position.
class PartySelection {
  public:
    PartySelection(Roster roster, unsigned parties, bool clear, std::uint16_t forced);
    unsigned parties() const { return parties_; }
    const Roster& roster() const { return roster_; }
    std::optional<std::uint8_t> character(unsigned cell) const;
    bool locked(unsigned cell) const;
    bool swap(unsigned first, unsigned second);
    bool ready() const;
    void update_characters(const Roster& edited);

  private:
    Roster roster_;
    unsigned parties_;
    std::uint16_t forced_;
    std::array<std::optional<std::uint8_t>, 16> reserve_{};
};

class PartySelectionMenu {
  public:
    PartySelectionMenu(const ContentPack& pack, FieldMenuValues values, unsigned parties,
                       bool clear, std::uint16_t forced, Buttons buttons);
    FieldMenuUpdate advance(const ContentPack& pack, Buttons buttons);
    RasterScene scene();
    const PartySelection& selection() const { return selection_; }
    unsigned cursor() const { return cursor_; }
    std::optional<unsigned> picked() const { return picked_; }
    bool complete() const { return closing_ && intensity_ == 0; }
    const Roster& roster() const { return selection_.roster(); }

  private:
    MenuLayoutAssets assets_;
    FieldSpriteAssets sprites_;
    FieldMenuValues values_;
    PartySelection selection_;
    std::optional<FieldMenu> status_;
    InputState input_;
    std::array<std::array<Rgb555, 16>, 23> colors_{};
    std::array<FieldSpriteTiles, 23> actors_{};
    unsigned cursor_ = 0, clock_ = 0, error_ = 0;
    std::optional<unsigned> picked_;
    std::uint8_t intensity_ = 0;
    bool closing_ = false;
    void move(Buttons repeat);
    std::array<int, 2> cursor_position(unsigned cell) const;
};
} // namespace ff6
