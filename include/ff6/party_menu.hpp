#pragma once
#include "ff6/main_menu.hpp"
#include "ff6/menu_navigation.hpp"
#include "ff6/party.hpp"

namespace ff6 {
// Applies the original display-time HP clamp, and MP clamp only when visible.
// Validates the whole party before publishing mutations or replacing the view.
void populate_party_menu(Roster& roster, MainMenuView& view);

enum class CharacterMenuRequest { sidebar, character_screen, party_equipment_selection };
struct CharacterNavigationUpdate {
    std::optional<CharacterMenuRequest> request;
    std::optional<std::uint8_t> selected_slot;
    std::array<MenuSound, 2> sounds{};
    std::uint8_t sound_count = 0;
    bool invalid_mosaic = false;
};
class CharacterMenuNavigation {
  public:
    explicit CharacterMenuNavigation(std::uint8_t remembered_slot = 0);
    // Initialization chooses a valid slot but does not move or update memory.
    void enter(CharacterMenuKind kind, const MenuParty& party, bool cursor_memory);
    CharacterNavigationUpdate advance(Buttons pressed, Buttons repeating, const Roster& roster,
                                      const PartyData& data);
    void finish_transition();
    std::uint8_t cursor() const {
        return cursor_;
    }
    std::uint8_t remembered_cursor() const {
        return remembered_;
    }
    CharacterMenuKind kind() const {
        return kind_;
    }
    MenuNavigationPhase phase() const {
        return phase_;
    }

  private:
    std::uint8_t cursor_ = 0, remembered_ = 0;
    CharacterMenuKind kind_ = CharacterMenuKind::status;
    MenuNavigationPhase phase_ = MenuNavigationPhase::handed_off;
};
} // namespace ff6
