#include "ff6/field_menu.hpp"
#include "rom-materials/config_scroll_step.macros.inc"
#include "rom-materials/menu_healing_sound.macros.inc"
#include "rom-materials/menu_mosaic_envelope.macros.inc"
#include "rom-materials/menu_item_list_rows.macros.inc"
#include "rom-materials/item_detail_scroll_step.macros.inc"
#include "rom-materials/item_detail_scroll_extent.macros.inc"
#include "rom-materials/equip_list_rows.macros.inc"
#include "rom-materials/field_order_scroll_step.macros.inc"
#include "rom-materials/field_order_scroll_extent.macros.inc"
#include "rom-materials/field_row_slide_wait.macros.inc"
#include "rom-materials/field_party_swap_wait.macros.inc"
#include "rom-materials/field_party_swap_phase.macros.inc"
#include "rom-materials/field_party_status_x.macros.inc"
#include "rom-materials/field_reequip_wait.macros.inc"
#include "rom-materials/main_menu_portrait_pitch.macros.inc"
#include "rom-materials/field_swap_clip_layout.macros.inc"
#include "rom-materials/field_party_status_rows.macros.inc"
#include "rom-materials/field_party_portrait_y.macros.inc"
#include "rom-materials/field_menu_fade_start.macros.inc"
#include "rom-materials/field_menu_full_intensity.macros.inc"
#include "rom-materials/skills_initial_cursor_row.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include "rom-materials/menu_select_sound.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
template <class Update> void sounds(FieldMenuUpdate& out, const Update& update) {
    for (unsigned i = 0; i < update.sound_count; ++i)
        out.sounds.push_back(menu_sound_effect(update.sounds[i]));
}
} // namespace
FieldMenu::FieldMenu(const ContentPack& pack, FieldMenuValues values, Buttons buttons,
                    std::optional<std::uint8_t> inspected_character)
    : assets_(load_menu_layouts(pack)), items_(load_menu_items(pack)),
      party_(load_party_data(pack)), equipment_(load_equipment_data(pack)),
      skills_(load_menu_skills(pack)), name_assets_(load_name_menu(pack)),
      values_(std::move(values)), inspected_character_(inspected_character),
      main_(values_.preferences.cursors.main.y),
      characters_(values_.preferences.cursors.character.y) {
    input_.previous = buttons;
    main_.enter(values_.preferences.cursor_memory);
    if (inspected_character_) {
        if (*inspected_character_ >= 16 || !values_.roster.characters[*inspected_character_].properties)
            throw std::invalid_argument("Invalid inspected character");
        kind_ = CharacterMenuKind::status;
        page_ = FieldMenuPage::status;
        enter_page();
    } else if (std::ranges::none_of(current_menu_party(values_.roster),
                             [](auto id) { return id.has_value(); }))
        throw std::invalid_argument("Field menu needs a party");
}
std::uint8_t FieldMenu::selected_character() const {
    if (inspected_character_) return *inspected_character_;
    return current_menu_party(values_.roster).at(character_slot_).value();
}
void FieldMenu::transition(FieldMenuPage target) {
    previous_page_ = page_;
    next_page_ = target;
    transitioning_ = true;
    fade_in_ = false;
}
void FieldMenu::sidebar() {
    main_ = MainMenuNavigation(values_.preferences.cursors.main.y);
    main_.enter(values_.preferences.cursor_memory);
    page_ = FieldMenuPage::main;
}
void FieldMenu::enter_page() {
    cursor_ = scroll_ = option_ = description_ticks_ = 0;
    if (page_ == FieldMenuPage::main)
        sidebar();
    if (page_ == FieldMenuPage::items && values_.preferences.cursor_memory) {
        scroll_ = values_.preferences.cursors.items.scroll;
        cursor_ = scroll_ + values_.preferences.cursors.items.position.y;
        if (cursor_ >= 255 || scroll_ > 255 - FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0)
            throw std::invalid_argument("Invalid saved item cursor");
    }
    if (page_ == FieldMenuPage::items &&
        (previous_page_ == FieldMenuPage::item_target || previous_page_ == FieldMenuPage::naming)) {
        cursor_ = saved_item_cursor_;
        scroll_ = saved_item_scroll_;
    }
    if (page_ == FieldMenuPage::skill_list && previous_page_ == FieldMenuPage::magic_target) {
        cursor_ = saved_skill_cursor_;
        scroll_ = saved_skill_scroll_;
    }
    if (page_ == FieldMenuPage::equipment) {
        relics_ = kind_ == CharacterMenuKind::relic;
        const auto& c = values_.roster.characters[selected_character()];
        old_relics_ = {c.equipment[4], c.equipment[5]};
    }
    if (page_ == FieldMenuPage::skills)
        cursor_ = values_.preferences.cursor_memory
                      ? values_.preferences.cursors.skills[character_slot_].y
                      : FF6_ROM_VALUE_SKILLS_INITIAL_CURSOR_ROW_0;
    if (page_ == FieldMenuPage::configuration) {
        config_scroll_ = 0;
        if (previous_page_ == FieldMenuPage::controllers)
            cursor_ = 8;
        if (previous_page_ == FieldMenuPage::command_arrange)
            cursor_ = 3;
    }
}
void FieldMenu::reject(FieldMenuUpdate& out) {
    out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
    invalid_ticks_ = FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0;
}
void FieldMenu::move_list(unsigned size, unsigned height, Buttons repeat, FieldMenuUpdate& out) {
    if (!size) {
        cursor_ = scroll_ = 0;
        return;
    }
    const auto before = cursor_;
    if (held(repeat, Button::up) && cursor_)
        --cursor_;
    else if (held(repeat, Button::down) && cursor_ + 1 < size)
        ++cursor_;
    else if (held(repeat, Button::l))
        cursor_ = cursor_ > height ? cursor_ - height : 0;
    else if (held(repeat, Button::r))
        cursor_ = std::min(cursor_ + height, size - 1);
    if (cursor_ < scroll_)
        scroll_ = cursor_;
    if (cursor_ >= scroll_ + height)
        scroll_ = cursor_ - height + 1;
    if (cursor_ != before) {
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        description_ticks_ = 0;
    }
}
void FieldMenu::move_character(Buttons repeat, FieldMenuUpdate& out) {
    if (!held(repeat, Button::up) && !held(repeat, Button::down))
        return;
    const auto party = current_menu_party(values_.roster);
    const auto direction = held(repeat, Button::up) ? 3U : 1U;
    for (unsigned i = 0; i < 4; ++i) {
        character_slot_ = (character_slot_ + direction) % 4;
        if (party[character_slot_])
            break;
    }
    out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
}
FieldMenuUpdate FieldMenu::advance(Buttons buttons) {
    FieldMenuUpdate out;
    if (complete())
        throw std::logic_error("Closed field menu cannot accept input");
    sample_input(input_, values_.input, InputContext::menu, buttons, 0);
    ++ticks_;
    ++description_ticks_;
    advance_game_time(values_.preferences.time, values_.preferences.time_frames);
    if (invalid_ticks_)
        --invalid_ticks_;
    if (fade_in_) {
        intensity_ =
            static_cast<std::uint8_t>(std::min(unsigned{FF6_ROM_VALUE_FIELD_MENU_FULL_INTENSITY_0}, unsigned{intensity_} +
                (intensity_ ? 2U : unsigned{FF6_ROM_VALUE_FIELD_MENU_FADE_START_0})));
        if (intensity_ == FF6_ROM_VALUE_FIELD_MENU_FULL_INTENSITY_0)
            fade_in_ = false;
        return out;
    }
    if (transitioning_) {
        intensity_ = static_cast<std::uint8_t>(intensity_ >= 2 ? intensity_ - 2 : 0);
        if (!intensity_) {
            page_ = next_page_;
            transitioning_ = false;
            if (page_ != FieldMenuPage::closed) {
                enter_page();
                fade_in_ = true;
            }
        }
        return out;
    }
    if (detail_scroll_ != detail_page_ * FF6_ROM_VALUE_ITEM_DETAIL_SCROLL_EXTENT_0) {
        detail_scroll_ = detail_page_ ? detail_scroll_ + FF6_ROM_VALUE_ITEM_DETAIL_SCROLL_STEP_0
                                      : detail_scroll_ - FF6_ROM_VALUE_ITEM_DETAIL_SCROLL_STEP_0;
        return out;
    }
    if (order_scroll_ != order_target_) {
        order_scroll_ = order_scroll_ < order_target_ ? order_scroll_ + FF6_ROM_VALUE_FIELD_ORDER_SCROLL_STEP_0 : order_scroll_ - FF6_ROM_VALUE_FIELD_ORDER_SCROLL_STEP_0;
        if (!order_scroll_)
            sidebar();
        return out;
    }
    if (row_slide_) {
        --row_slide_;
        return out;
    }
    if (swap_ticks_) {
        --swap_ticks_;
        if (swap_ticks_ == FF6_ROM_VALUE_FIELD_PARTY_SWAP_PHASE_0)
            swap_party_order();
        return out;
    }
    if (page_ == FieldMenuPage::configuration && config_scroll_ != option_ * 256) {
        config_scroll_ = option_ ? config_scroll_ + FF6_ROM_VALUE_CONFIG_SCROLL_STEP_0 : config_scroll_ - FF6_ROM_VALUE_CONFIG_SCROLL_STEP_0;
        return out;
    }
    const auto press = input_.pressed, repeat = input_.repeating;
    if (page_ == FieldMenuPage::main) {
        const auto update = main_.advance(press, repeat, values_.permissions.rest);
        sounds(out, update);
        values_.preferences.cursors.main.y = main_.remembered_cursor();
        if (update.invalid_mosaic)
            invalid_ticks_ = FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0;
        if (update.request)
            switch (*update.request) {
            case MainMenuRequest::close_and_refresh_equipment:
                transition(FieldMenuPage::closed);
                break;
            case MainMenuRequest::items:
                transition(FieldMenuPage::items);
                break;
            case MainMenuRequest::configuration:
                transition(FieldMenuPage::configuration);
                break;
            case MainMenuRequest::save:
                transition(FieldMenuPage::save);
                break;
            case MainMenuRequest::party_order:
                character_slot_ = 0;
                while (!current_menu_party(values_.roster)[character_slot_])
                    ++character_slot_;
                page_ = FieldMenuPage::order;
                order_target_ = FF6_ROM_VALUE_FIELD_ORDER_SCROLL_EXTENT_0;
                break;
            default:
                kind_ = *update.request == MainMenuRequest::skill_character
                            ? CharacterMenuKind::skills
                        : *update.request == MainMenuRequest::equipment_character
                            ? CharacterMenuKind::equipment
                        : *update.request == MainMenuRequest::relic_character
                            ? CharacterMenuKind::relic
                            : CharacterMenuKind::status;
                characters_.enter(kind_, current_menu_party(values_.roster),
                                  values_.preferences.cursor_memory);
                page_ = FieldMenuPage::character;
                character_slot_ = characters_.cursor();
                break;
            }
    } else if (page_ == FieldMenuPage::character) {
        const auto update = characters_.advance(press, repeat, values_.roster, party_);
        sounds(out, update);
        values_.preferences.cursors.character.y = characters_.remembered_cursor();
        character_slot_ = characters_.cursor();
        if (update.invalid_mosaic)
            invalid_ticks_ = FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0;
        if (update.request == CharacterMenuRequest::sidebar)
            sidebar();
        else if (update.request == CharacterMenuRequest::party_equipment_selection)
            transition(FieldMenuPage::party_equipment);
        else if (update.request == CharacterMenuRequest::character_screen) {
            character_slot_ = *update.selected_slot;
            transition(kind_ == CharacterMenuKind::skills   ? FieldMenuPage::skills
                       : kind_ == CharacterMenuKind::status ? FieldMenuPage::status
                                                            : FieldMenuPage::equipment);
        }
    } else if (page_ == FieldMenuPage::order || page_ == FieldMenuPage::order_move) {
        move_character(repeat, out);
        if (held(press, Button::b) ||
            (page_ == FieldMenuPage::order && held(press, Button::right))) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            if (page_ == FieldMenuPage::order_move) {
                character_slot_ = selected_;
                page_ = FieldMenuPage::order;
            } else
                order_target_ = 0;
        } else if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            if (page_ == FieldMenuPage::order) {
                selected_ = character_slot_;
                page_ = FieldMenuPage::order_move;
            } else {
                const auto party = current_menu_party(values_.roster);
                auto& a = values_.roster.membership[*party[selected_]];
                if (selected_ == character_slot_) {
                    a.back_row = !a.back_row;
                    row_slide_ = FF6_ROM_VALUE_FIELD_ROW_SLIDE_WAIT_0;
                } else {
                    swap_ticks_ = FF6_ROM_VALUE_FIELD_PARTY_SWAP_WAIT_0;
                }
                page_ = FieldMenuPage::order;
            }
        }
    } else if (page_ >= FieldMenuPage::items && page_ <= FieldMenuPage::rare_items)
        advance_items(press, repeat, out);
    else if (page_ >= FieldMenuPage::equipment && page_ <= FieldMenuPage::party_equipment)
        advance_equipment(press, repeat, out);
    else if (page_ == FieldMenuPage::status || page_ == FieldMenuPage::gogo_commands)
        advance_status(press, repeat, out);
    else if (page_ >= FieldMenuPage::skills && page_ <= FieldMenuPage::magic_target)
        advance_skills(press, repeat, out);
    else if (page_ >= FieldMenuPage::configuration && page_ <= FieldMenuPage::controllers)
        advance_configuration(press, repeat, out);
    else if (page_ == FieldMenuPage::naming) {
        for (auto sound : naming_->advance(name_assets_, press, repeat))
            out.sounds.push_back(sound);
        if (naming_->phase() == NameMenuPhase::complete) {
            values_.roster.characters[selected_character()].name = naming_->name();
            naming_.reset();
            transition(FieldMenuPage::items);
        }
    } else if (page_ != FieldMenuPage::save)
        throw std::logic_error("Field submenu owner has not been connected");
    return out;
}
void FieldMenu::swap_party_order() {
    const auto party = current_menu_party(values_.roster);
    std::swap(values_.roster.membership[*party[selected_]].position,
              values_.roster.membership[*party[character_slot_]].position);
    const auto mask = (1U << selected_) | (1U << character_slot_);
    const auto flags = values_.preferences.second_player_characters & mask;
    if (flags && flags != mask)
        values_.preferences.second_player_characters ^= static_cast<std::uint8_t>(mask);
    std::swap(values_.preferences.cursors.skills[selected_],
              values_.preferences.cursors.skills[character_slot_]);
    std::swap(values_.preferences.cursors.magic[selected_],
              values_.preferences.cursors.magic[character_slot_]);
}
void FieldMenu::finish_save(std::optional<GameMenuState> saved) {
    if (page_ != FieldMenuPage::save)
        throw std::logic_error("No save menu to finish");
    if (saved) {
        values_.preferences.save_count = saved->save_count;
        values_.preferences.saved_time = saved->saved_time;
    }
    intensity_ = 0;
    page_ = FieldMenuPage::main;
    transitioning_ = false;
    fade_in_ = true;
    enter_page();
}
void FieldMenu::begin_item(FieldMenuUpdate& out) {
    const auto id = values_.inventory.slots[selected_].item;
    if (id == 255) {
        page_ = FieldMenuPage::items;
        return;
    }
    if (items_.items[id].type != 6) {
        if (items_.items[id].type) {
            page_ = FieldMenuPage::item_details;
            detail_page_ = 0;
        } else
            page_ = FieldMenuPage::items;
        return;
    }
    if (!field_item_available(items_, id, values_.permissions)) {
        page_ = FieldMenuPage::items;
        return;
    }
    saved_item_cursor_ = cursor_;
    saved_item_scroll_ = scroll_;
    character_slot_ = 0;
    while (!current_menu_party(values_.roster)[character_slot_])
        ++character_slot_;
    if (id == 247 || id == 253) {
        item_handoff_ = use_field_item(items_, values_.inventory,
                                       values_.roster.characters[selected_character()],
                                       static_cast<std::uint8_t>(selected_), values_.permissions);
        if (item_handoff_ == FieldItemResult::invalid) {
            item_handoff_.reset();
            reject(out);
            page_ = FieldMenuPage::items;
        } else
            transition(FieldMenuPage::closed);
    } else
        transition(FieldMenuPage::item_target);
}
void FieldMenu::advance_items(Buttons press, Buttons repeat, FieldMenuUpdate& out) {
    if (page_ == FieldMenuPage::items || page_ == FieldMenuPage::item_move) {
        move_list(255, FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0, repeat, out);
        values_.preferences.cursors.items = {{0, static_cast<std::uint8_t>(cursor_ - scroll_)},
                                             static_cast<std::uint8_t>(scroll_)};
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            page_ = page_ == FieldMenuPage::item_move ? FieldMenuPage::items
                                                      : FieldMenuPage::item_options;
        } else if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            if (page_ == FieldMenuPage::items) {
                selected_ = cursor_;
                page_ = FieldMenuPage::item_move;
            } else if (selected_ == cursor_)
                begin_item(out);
            else {
                swap_menu_items(values_.inventory, static_cast<std::uint8_t>(selected_),
                                static_cast<std::uint8_t>(cursor_));
                page_ = FieldMenuPage::items;
            }
        }
    } else if (page_ == FieldMenuPage::item_options) {
        if (held(repeat, Button::left) || held(repeat, Button::right)) {
            option_ = (option_ + (held(repeat, Button::left) ? 2U : 1U)) % 3;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            transition(FieldMenuPage::main);
        } else if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            if (option_ == 0)
                page_ = FieldMenuPage::items;
            else if (option_ == 1) {
                arrange_menu_inventory(items_, values_.inventory);
                option_ = 0;
            } else {
                saved_item_cursor_ = cursor_;
                saved_item_scroll_ = scroll_;
                page_ = FieldMenuPage::rare_items;
                scroll_ = 0;
                const auto memory = values_.preferences.cursors.rare_items;
                cursor_ = values_.preferences.cursor_memory ? memory.y * 2U + memory.x : 0;
                if (cursor_ >= 20)
                    throw std::invalid_argument("Invalid rare-item cursor");
            }
        }
    } else if (page_ == FieldMenuPage::item_target) {
        move_character(repeat, out);
        if (held(press, Button::a)) {
            const auto result = use_field_item(
                items_, values_.inventory, values_.roster.characters[selected_character()],
                static_cast<std::uint8_t>(selected_), values_.permissions);
            if (result == FieldItemResult::invalid)
                reject(out);
            else {
                if (result == FieldItemResult::naming)
                    out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
                else
                    out.game_sounds.push_back(FF6_ROM_VALUE_MENU_HEALING_SOUND_0);
                if (result == FieldItemResult::naming) {
                    const auto& c = values_.roster.characters[selected_character()];
                    naming_.emplace(c.name, c.graphics);
                    transition(FieldMenuPage::naming);
                } else if (result != FieldItemResult::applied) {
                    item_handoff_ = result;
                    transition(FieldMenuPage::closed);
                } else if (!values_.inventory.slots[selected_].quantity)
                    transition(FieldMenuPage::items);
            }
        } else if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            transition(FieldMenuPage::items);
        }
    } else if (page_ == FieldMenuPage::item_details) {
        if (!detail_page_ && (held(press, Button::a) || held(press, Button::left))) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            detail_page_ = 1;
        } else if (held(press, Button::b) || (detail_page_ && held(press, Button::right))) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            if (detail_page_)
                detail_page_ = 0;
            else
                page_ = FieldMenuPage::items;
        }
    } else if (page_ == FieldMenuPage::rare_items) {
        const auto before = cursor_;
        if (held(repeat, Button::up))
            cursor_ = (cursor_ + 18) % 20;
        else if (held(repeat, Button::down))
            cursor_ = (cursor_ + 2) % 20;
        else if (held(repeat, Button::left) || held(repeat, Button::right))
            cursor_ ^= 1;
        if (before != cursor_) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            description_ticks_ = 0;
        }
        values_.preferences.cursors.rare_items = {static_cast<std::uint8_t>(cursor_ % 2),
                                                  static_cast<std::uint8_t>(cursor_ / 2)};
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            page_ = FieldMenuPage::item_options;
            cursor_ = saved_item_cursor_;
            scroll_ = saved_item_scroll_;
        }
    }
}
void FieldMenu::advance_equipment(Buttons press, Buttons repeat, FieldMenuUpdate& out) {
    if (page_ == FieldMenuPage::party_equipment) {
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            transition(FieldMenuPage::main);
        }
        return;
    }
    auto& character = values_.roster.characters[selected_character()];
    if (reequip_ticks_) {
        --reequip_ticks_;
        if (held(press, Button::b) || held(press, Button::l) || held(press, Button::r))
            reequip_ticks_ = 0;
        if (!reequip_ticks_) {
            if (values_.preferences.remove_equipment)
                empty_menu_equipment(values_.inventory, character);
            else
                optimize_menu_equipment(items_, party_, equipment_, values_.inventory, character);
            const auto stats = calculate_equipment(character, party_, equipment_);
            character.hp.boost = stats.hp_boost;
            character.mp.boost = stats.mp_boost;
            kind_ = CharacterMenuKind::equipment;
            transition(FieldMenuPage::equipment);
        }
        return;
    }
    if (page_ == FieldMenuPage::equipment) {
        if (held(press, Button::l) || held(press, Button::r)) {
            if (relics_ && check_reequip())
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            else
                change_character(press, out);
            return;
        }
        const unsigned options = relics_ ? 2 : 4;
        if (held(repeat, Button::left) || held(repeat, Button::right)) {
            option_ = (option_ + (held(repeat, Button::left) ? options - 1 : 1)) % options;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            if (!relics_ && option_ == 1) {
                optimize_menu_equipment(items_, party_, equipment_, values_.inventory, character);
                option_ = 0;
            } else if (!relics_ && option_ == 3) {
                empty_menu_equipment(values_.inventory, character);
                const auto stats = calculate_equipment(character, party_, equipment_);
                character.hp.boost = stats.hp_boost;
                character.mp.boost = stats.mp_boost;
                option_ = 0;
            } else {
                removing_ = option_ != 0;
                equip_slot_ = relics_ ? 4 : 0;
                page_ = FieldMenuPage::equip_slot;
            }
        } else if (held(press, Button::b)) {
            // The three loadout-changing relics trigger the source's configured
            // re-equip policy, even when a replacement grants the same flags.
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            if (!relics_ || !check_reequip())
                transition(FieldMenuPage::main);
        }
    } else if (page_ == FieldMenuPage::equip_slot) {
        if (held(press, Button::l) || held(press, Button::r)) {
            if (relics_ && check_reequip())
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            else {
                const auto slot = equip_slot_;
                change_character(press, out);
                equip_slot_ = slot;
            }
            return;
        }
        const unsigned first = relics_ ? 4 : 0, count = relics_ ? 2 : 4;
        if (held(repeat, Button::up) || held(repeat, Button::down)) {
            equip_slot_ =
                first + (equip_slot_ - first + (held(repeat, Button::up) ? count - 1 : 1)) % count;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            description_ticks_ = 0;
        }
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            page_ = FieldMenuPage::equipment;
            option_ = 0;
        } else if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
            if (removing_)
                change_menu_equipment(items_, party_, equipment_, values_.inventory, character,
                                      equip_slot_, {});
            else {
                candidates_ = menu_equipment_candidates(
                    items_, values_.inventory, character,
                    calculate_equipment(character, party_, equipment_), equip_slot_);
                if (candidates_.empty())
                    reject(out);
                else {
                    cursor_ = scroll_ = 0;
                    page_ = FieldMenuPage::equip_list;
                }
            }
        }
    } else if (page_ == FieldMenuPage::equip_list) {
        move_list(static_cast<unsigned>(candidates_.size()), FF6_ROM_VALUE_EQUIP_LIST_ROWS_0, repeat, out);
        if (held(press, Button::a)) {
            if (change_menu_equipment(items_, party_, equipment_, values_.inventory, character,
                                      equip_slot_, candidates_.at(cursor_))) {
                out.sounds.push_back(FF6_ROM_VALUE_MENU_SELECT_SOUND_0);
                page_ = FieldMenuPage::equip_slot;
            } else
                reject(out);
        } else if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            page_ = FieldMenuPage::equip_slot;
        }
    }
}
bool FieldMenu::check_reequip() {
    const auto& character = values_.roster.characters[selected_character()];
    const std::array current{character.equipment[4], character.equipment[5]};
    const auto triggers = [](auto ids) {
        return std::ranges::any_of(ids,
                                   [](auto id) { return id == 208 || id == 209 || id == 218; });
    };
    if (character.properties != 13 && current != old_relics_ &&
        (triggers(current) || triggers(old_relics_))) {
        reequip_ticks_ = FF6_ROM_VALUE_FIELD_REEQUIP_WAIT_0;
        return true;
    }
    return false;
}
void FieldMenu::event_timer(std::optional<std::uint16_t> frames, bool expired) {
    timer_frames_ = frames;
    if (expired && page_ != FieldMenuPage::closed && next_page_ != FieldMenuPage::closed)
        transition(FieldMenuPage::closed);
}
MainMenuView FieldMenu::party_view() {
    MainMenuView view;
    populate_party_menu(values_.roster, view);
    view.window = {values_.window_skin, values_.window_colors[values_.window_skin]};
    view.font_color = values_.font_color;
    view.gil = values_.inventory.gil;
    view.steps = values_.steps;
    view.hours = values_.preferences.time.hours;
    view.minutes = values_.preferences.time.minutes;
    if (timer_frames_) {
        view.hours = static_cast<std::uint8_t>(*timer_frames_ / 3600U);
        view.minutes = static_cast<std::uint8_t>((*timer_frames_ / 60U) % 60U);
    }
    view.save_enabled = values_.permissions.rest;
    view.intensity = intensity_;
    view.cursor = main_.cursor();
    view.sidebar_cursor_visible = page_ == FieldMenuPage::main;
    if (page_ == FieldMenuPage::character || page_ == FieldMenuPage::order ||
        page_ == FieldMenuPage::order_move)
        view.character_cursor = static_cast<std::uint8_t>(character_slot_);
    if (swap_ticks_)
        view.character_cursor.reset();
    return view;
}
void FieldMenu::draw_party_status(MenuCanvas& canvas, int shift_x) {
    // DrawCharBlock1..4 create the same icons on the main and target pages.
    // The target-page horizontal scroll also shifts these sprite tasks.
    const auto party = current_menu_party(values_.roster);
    for (unsigned slot = 0; slot < party.size(); ++slot)
        if (party[slot]) {
            const auto& c = values_.roster.characters[*party[slot]];
            // The shared character text layer already owns "wounded".
            if (!(c.status & 128U))
                canvas.status(c.status, c.persistent_effects, FF6_ROM_VALUE_FIELD_PARTY_STATUS_X_0 + shift_x,
                              std::array<int, 4>{FF6_ROM_VALUE_FIELD_PARTY_STATUS_ROWS_0,
                                  FF6_ROM_VALUE_FIELD_PARTY_STATUS_ROWS_1,
                                  FF6_ROM_VALUE_FIELD_PARTY_STATUS_ROWS_2,
                                  FF6_ROM_VALUE_FIELD_PARTY_STATUS_ROWS_3}[slot]);
        }
}
RasterScene FieldMenu::scene() {
    RasterScene result;
    if (page_ == FieldMenuPage::closed) {
        result.intensity.assign(224, 0);
        return result;
    }
    if (page_ <= FieldMenuPage::order_move) {
        result = make_main_menu_scene(assets_.shared, party_view());
        MenuCanvas icons(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                         values_.font_color, intensity_);
        draw_party_status(icons, 0);
        result.objects = icons.finish().objects;
        if (order_scroll_) {
            for (auto& layer : result.layers)
                for (auto& line : layer.lines)
                    line.offset_x -= static_cast<int>(order_scroll_);
            for (auto& sprite : result.objects->sprites)
                sprite.x += static_cast<int>(order_scroll_);
        }
        if (row_slide_) {
            const auto party = current_menu_party(values_.roster);
            for (auto& layer : result.layers)
                if (layer.columns == 5 && layer.rows == 5) {
                    if (party[character_slot_] &&
                        layer.lines[0].offset_y == -FF6_ROM_VALUE_FIELD_PARTY_PORTRAIT_Y_0 - static_cast<int>(character_slot_) * FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_PITCH_0)
                        for (auto& line : layer.lines)
                            line.offset_x +=
                                values_.roster.membership[*party[character_slot_]].back_row
                                    ? int(row_slide_)
                                    : -int(row_slide_);
                }
        }
        if (swap_ticks_) {
            const auto hidden = 4U * (swap_ticks_ > FF6_ROM_VALUE_FIELD_PARTY_SWAP_PHASE_0 ? FF6_ROM_VALUE_FIELD_PARTY_SWAP_WAIT_0 - swap_ticks_ : swap_ticks_);
            const auto top = std::min(selected_, character_slot_),
                       bottom = std::max(selected_, character_slot_);
            for (unsigned slot : {top, bottom})
                for (unsigned y = FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_0 + slot * FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_1; y < FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_0 + FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_2 + slot * FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_1; ++y) {
                    const auto row = y - (FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_0 + slot * FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_1);
                    if (slot == top ? row < hidden : row >= FF6_ROM_VALUE_FIELD_SWAP_CLIP_LAYOUT_2 - hidden) {
                        result.layers[0].lines[y].enabled = false;
                        for (auto& layer : result.layers)
                            if (layer.columns == 5 && layer.rows == 5)
                                layer.lines[y].enabled = false;
                        result.objects->lines[y].enabled = false;
                    }
                }
        }
    } else if (page_ <= FieldMenuPage::rare_items)
        result = item_scene();
    else if (page_ <= FieldMenuPage::party_equipment)
        result = equipment_scene();
    else if (page_ == FieldMenuPage::status || page_ == FieldMenuPage::gogo_commands)
        result = status_scene();
    else if (page_ >= FieldMenuPage::skills && page_ <= FieldMenuPage::magic_target)
        result = skill_scene();
    else if (page_ >= FieldMenuPage::configuration && page_ <= FieldMenuPage::controllers)
        result = configuration_scene();
    else if (page_ == FieldMenuPage::naming)
        result = naming_->scene(name_assets_,
                                {values_.window_skin, values_.window_colors[values_.window_skin]},
                                values_.font_color);
    else if (page_ == FieldMenuPage::save)
        result.intensity.assign(224, 0);
    else
        throw std::logic_error("Field submenu display is not connected");
    if (invalid_ticks_) {
        const auto phase = unsigned{FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0} - invalid_ticks_;
        const auto size = static_cast<std::uint8_t>(phase < FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_1
            ? phase + FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_2
            : FF6_ROM_VALUE_MENU_MOSAIC_ENVELOPE_0 - phase);
        for (auto& layer : result.layers)
            for (unsigned y = 0; y < layer.lines.size(); ++y) {
                layer.lines[y].sample_width = size;
                layer.lines[y].sample_row = static_cast<int>(y - y % size);
            }
    }
    return result;
}
} // namespace ff6
