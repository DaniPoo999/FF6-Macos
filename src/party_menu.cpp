#include "ff6/party_menu.hpp"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include <algorithm>

namespace ff6 {
void populate_party_menu(Roster& roster, MainMenuView& view) {
    auto next = view;
    auto characters = roster.characters;
    const auto party = current_menu_party(roster);
    next.party = {};
    for (std::size_t slot = 0; slot < 4; ++slot) {
        if (!party[slot])
            continue;
        auto& character = characters[*party[slot]];
        const auto maximum_hp = maximum_vital(character.hp, 9999);
        const auto show_mp = menu_mp_visible(roster, character);
        const auto maximum_mp = maximum_vital(character.mp, 999);
        character.hp.current = std::min(character.hp.current, maximum_hp);
        if (show_mp)
            character.mp.current = std::min(character.mp.current, maximum_mp);
        // Graphics and palette loaders share this authenticated ROM selection.
        const std::uint8_t portrait = (character.status & 0x20U) != 0 ? FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0
                                      : *character.properties == 1    ? 1
                                                                      : character.graphics;
        if (portrait >= 27 || character.level == 0 || character.level > 99)
            throw std::invalid_argument("Unsupported party menu portrait or level");
        // Hidden MP is absent from the display. Its authoritative current value
        // is retained, including an over-maximum value the original never clamps.
        next.party[slot] =
            MenuCharacterView{character.name,
                              character.level,
                              portrait,
                              character.hp.current,
                              maximum_hp,
                              static_cast<std::uint16_t>(show_mp ? character.mp.current : 0),
                              static_cast<std::uint16_t>(show_mp ? maximum_mp : 0),
                              show_mp,
                              (character.status & 0x80U) != 0,
                              roster.membership[*party[slot]].back_row};
    }
    roster.characters = characters;
    view = std::move(next);
}
CharacterMenuNavigation::CharacterMenuNavigation(std::uint8_t remembered_slot)
    : remembered_(remembered_slot) {
    if (remembered_ >= 4)
        throw std::invalid_argument("Remembered character cursor must be 0..3");
}
void CharacterMenuNavigation::enter(CharacterMenuKind kind, const MenuParty& party,
                                    bool cursor_memory) {
    if (kind != CharacterMenuKind::skills && kind != CharacterMenuKind::equipment &&
        kind != CharacterMenuKind::relic && kind != CharacterMenuKind::status)
        throw std::invalid_argument("Unknown character menu kind");
    for (const auto identity : party)
        if (identity && *identity >= 16)
            throw std::invalid_argument("Invalid character in menu party");
    const auto first = std::ranges::find_if(party, [](auto slot) { return slot.has_value(); });
    if (first == party.end())
        throw std::invalid_argument("Character selection requires a nonempty party");
    cursor_ = cursor_memory && party[remembered_]
                  ? remembered_
                  : static_cast<std::uint8_t>(first - party.begin());
    kind_ = kind;
    phase_ = MenuNavigationPhase::selecting;
}
CharacterNavigationUpdate CharacterMenuNavigation::advance(Buttons pressed, Buttons repeating,
                                                           const Roster& roster,
                                                           const PartyData& data) {
    if (((pressed | repeating) & ~Buttons{4095}) != 0)
        throw std::invalid_argument("Unsupported character menu buttons");
    const auto party = current_menu_party(roster);
    if (phase_ == MenuNavigationPhase::handed_off || !party[cursor_])
        throw std::logic_error("Character selection needs a valid active cursor");
    CharacterNavigationUpdate update;
    auto phase = phase_;
    if (phase == MenuNavigationPhase::selecting) {
        // This state prioritizes B before Left before A, unlike the sidebar.
        if (held(pressed, Button::b)) {
            update.request = CharacterMenuRequest::sidebar;
            update.sounds[update.sound_count++] = MenuSound::cancel;
            phase = MenuNavigationPhase::handed_off;
        } else if (held(pressed, Button::left) &&
                   (kind_ == CharacterMenuKind::equipment || kind_ == CharacterMenuKind::relic)) {
            update.request = CharacterMenuRequest::party_equipment_selection;
            update.sounds[update.sound_count++] = MenuSound::move;
            phase = MenuNavigationPhase::handed_off;
        } else if (held(pressed, Button::a)) {
            if (character_menu_allowed(data, roster.characters[*party[cursor_]], kind_)) {
                update.request = CharacterMenuRequest::character_screen;
                update.selected_slot = cursor_;
                update.sounds[update.sound_count++] = MenuSound::select;
                phase = MenuNavigationPhase::leaving;
            } else {
                update.sounds[update.sound_count++] = MenuSound::invalid;
                update.invalid_mosaic = true;
            }
        }
    }
    phase_ = phase;
    // Main-menu character cursor memory is saved BEFORE movement, even when
    // the handler just disabled the task. This deliberately differs from sidebar.
    remembered_ = cursor_;
    if (phase_ != MenuNavigationPhase::handed_off &&
        (held(repeating, Button::up) || held(repeating, Button::down))) {
        const unsigned direction = held(repeating, Button::up) ? 3U : 1U;
        // Nonempty validated party makes this bounded even with three holes.
        for (unsigned visited = 0; visited < 4; ++visited) {
            cursor_ = static_cast<std::uint8_t>((cursor_ + direction) % 4U);
            if (party[cursor_])
                break;
        }
        // Traversing empty slots (or wrapping to the only member) still emits
        // one move sound: original task coalesces repeated move calls per frame.
        update.sounds[update.sound_count++] = MenuSound::move;
    }
    return update;
}
void CharacterMenuNavigation::finish_transition() {
    if (phase_ != MenuNavigationPhase::leaving)
        throw std::logic_error("No character menu transition to finish");
    phase_ = MenuNavigationPhase::handed_off;
}
} // namespace ff6
