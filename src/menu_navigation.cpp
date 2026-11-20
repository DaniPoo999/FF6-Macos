#include "ff6/menu_navigation.hpp"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_upper.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"

namespace ff6 {
namespace {
void validate_buttons(Buttons value) {
    if ((value & ~Buttons{4095}) != 0)
        throw std::invalid_argument("Unsupported menu button bits");
}
void sound(MenuNavigationUpdate& update, MenuSound value) {
    update.sounds.at(update.sound_count++) = value;
}
} // namespace

std::uint8_t menu_sound_effect(MenuSound value) {
    // C3/0EA9..0EC6 selects these protocol-relative sound identities. Bind the
    // compiler's packed switch/result mask to the ROM-derived output domain.
    constexpr auto lower = FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0;
    constexpr auto upper = FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_UPPER_0;
    constexpr auto maximum = FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0;
    static_assert((lower >> 8U) == (upper & 255U) && (upper >> 8U) == maximum);
    static_assert(maximum != 0 && (maximum & (maximum - 1U)) == 0);
    static_assert((lower & 255U) <= maximum && (lower >> 8U) <= maximum);
    switch (value) {
    case MenuSound::select:
        return lower & 255U;
    case MenuSound::move:
    case MenuSound::cancel:
        return lower >> 8U;
    case MenuSound::invalid:
        return upper >> 8U;
    }
    throw std::invalid_argument("Unknown menu sound");
}

MainMenuNavigation::MainMenuNavigation(std::uint8_t remembered_cursor)
    : remembered_(remembered_cursor) {
    if (remembered_ >= 7)
        throw std::invalid_argument("Remembered menu cursor must be 0..6");
}
MenuNavigationUpdate MainMenuNavigation::enter(bool cursor_memory, Buttons repeating) {
    validate_buttons(repeating);
    cursor_ = cursor_memory ? remembered_ : 0;
    phase_ = MenuNavigationPhase::selecting;
    MenuNavigationUpdate update;
    move_cursor(repeating, update);
    return update;
}
void MainMenuNavigation::move_cursor(Buttons repeating, MenuNavigationUpdate& update) {
    // Up wins over Down. One column with horizontal wrapping disabled means
    // neither horizontal direction moves the sidebar or plays a movement sound.
    const bool up = held(repeating, Button::up);
    if (up || held(repeating, Button::down)) {
        cursor_ = static_cast<std::uint8_t>((cursor_ + (up ? 6U : 1U)) % 7U);
        // The original coalesces move/cancel within this iteration, but select
        // and invalid do not set that latch. Preserve request order, not port I/O.
        if (update.sound_count == 0 || update.sounds[0] != MenuSound::cancel)
            sound(update, MenuSound::move);
    }
    remembered_ = cursor_;
}
MenuNavigationUpdate MainMenuNavigation::advance(Buttons pressed, Buttons repeating,
                                                 bool save_enabled) {
    validate_buttons(pressed);
    validate_buttons(repeating);
    if (phase_ == MenuNavigationPhase::handed_off)
        throw std::logic_error("Menu navigation requires entry after a handoff");
    MenuNavigationUpdate update;
    if (phase_ == MenuNavigationPhase::selecting) {
        // The menu-state handler runs BEFORE cursor tasks. In particular, A+Up
        // selects the old row, then may move the cursor during the pending fade.
        if (held(pressed, Button::a)) {
            if (cursor_ == 6 && !save_enabled) {
                sound(update, MenuSound::invalid);
                update.invalid_mosaic = true;
            } else {
                constexpr std::array destinations{MainMenuRequest::items,
                                                  MainMenuRequest::skill_character,
                                                  MainMenuRequest::equipment_character,
                                                  MainMenuRequest::relic_character,
                                                  MainMenuRequest::status_character,
                                                  MainMenuRequest::configuration,
                                                  MainMenuRequest::save};
                update.request = destinations[cursor_];
                sound(update, MenuSound::select);
                phase_ = cursor_ >= 1 && cursor_ <= 4 ? MenuNavigationPhase::handed_off
                                                      : MenuNavigationPhase::leaving;
            }
        } else if (held(pressed, Button::left)) {
            sound(update, MenuSound::move);
            update.request = MainMenuRequest::party_order;
            phase_ = MenuNavigationPhase::handed_off;
        } else if (held(pressed, Button::b)) {
            sound(update, MenuSound::cancel);
            update.request = MainMenuRequest::close_and_refresh_equipment;
            phase_ = MenuNavigationPhase::leaving;
        }
    }
    if (phase_ != MenuNavigationPhase::handed_off)
        move_cursor(repeating, update);
    return update;
}
void MainMenuNavigation::finish_transition() {
    if (phase_ != MenuNavigationPhase::leaving)
        throw std::logic_error("No menu transition to finish");
    phase_ = MenuNavigationPhase::handed_off;
}
} // namespace ff6
