#pragma once
#include "ff6/input.hpp"
#include <optional>

namespace ff6 {
enum class MenuSound { select, move, cancel, invalid };
std::uint8_t menu_sound_effect(MenuSound sound);

// Character options hand off to character selection, not directly to a submenu.
// Closing also requires the field owner to recalculate equipment effects.
enum class MainMenuRequest {
    items,
    skill_character,
    equipment_character,
    relic_character,
    status_character,
    configuration,
    save,
    party_order,
    close_and_refresh_equipment
};
struct MenuNavigationUpdate {
    std::optional<MainMenuRequest> request;
    std::array<MenuSound, 2> sounds{};
    std::uint8_t sound_count = 0;
    bool invalid_mosaic = false;
};
enum class MenuNavigationPhase { selecting, leaving, handed_off };

// Owns only the sidebar's interaction. Caller owns party data, fades, character
// selection, event timers and input sampling. See docs/MENU_NAVIGATION.md.
class MainMenuNavigation {
  public:
    explicit MainMenuNavigation(std::uint8_t remembered_cursor = 0);
    // Cursor-task initialization also consumes this sample's repeat directions.
    // Memory affects restoration only; the task always records its latest row.
    MenuNavigationUpdate enter(bool cursor_memory, Buttons repeating = 0);
    // One menu-state/task iteration, using already sampled/mapped logical input.
    // While leaving, only the cursor task runs. Caller must finish the transition.
    MenuNavigationUpdate advance(Buttons pressed, Buttons repeating, bool save_enabled);
    void finish_transition();

    std::uint8_t cursor() const {
        return cursor_;
    }
    std::uint8_t remembered_cursor() const {
        return remembered_;
    }
    MenuNavigationPhase phase() const {
        return phase_;
    }

  private:
    void move_cursor(Buttons repeating, MenuNavigationUpdate& update);
    std::uint8_t cursor_ = 0, remembered_ = 0;
    MenuNavigationPhase phase_ = MenuNavigationPhase::handed_off;
};
} // namespace ff6
