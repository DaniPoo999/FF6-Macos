#include "ff6/field_menu.hpp"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include "rom-materials/menu_healing_sound.macros.inc"
#include "rom-materials/skills_vital_columns.macros.inc"
#include "rom-materials/skills_equipped_esper_column.macros.inc"
#include "rom-materials/skills_status_x.macros.inc"
#include "rom-materials/skills_vital_rows.macros.inc"
#include "rom-materials/skills_equipped_esper_row.macros.inc"
#include "rom-materials/skills_status_y.macros.inc"
#include "rom-materials/main_menu_portrait_x.macros.inc"
#include "rom-materials/status_portrait_y.macros.inc"
#include "rom-materials/skills_scroll_rates.macros.inc"
#include "rom-materials/skills_scroll_y.macros.inc"
#include "rom-materials/skills_esper_error_hold.macros.inc"
#include "rom-materials/shop_sell_scroll_x.macros.inc"
#include "rom-materials/skills_esper_name_column.macros.inc"
#include "rom-materials/skills_esper_name_row.macros.inc"
#include "rom-materials/skills_esper_spell_column.macros.inc"
#include "rom-materials/skills_esper_progress_column.macros.inc"
#include "rom-materials/skills_esper_spell_first_row.macros.inc"
#include "rom-materials/skills_esper_bonus_column.macros.inc"
#include "rom-materials/skills_esper_bonus_row.macros.inc"
#include "rom-materials/skills_esper_error_column.macros.inc"
#include "rom-materials/skills_esper_error_row.macros.inc"
#include "rom-materials/skills_list_cost_column.macros.inc"
#include "rom-materials/skills_list_cost_row.macros.inc"
#include "rom-materials/skills_target_name_column.macros.inc"
#include "rom-materials/skills_target_name_row.macros.inc"
#include "rom-materials/skills_target_cost_column.macros.inc"
#include "rom-materials/skills_target_cost_row.macros.inc"
#include "rom-materials/skills_target_window_column.macros.inc"
#include "rom-materials/skills_target_window_height.macros.inc"
#include "rom-materials/skills_esper_cursor_name_x.macros.inc"
#include "rom-materials/skills_esper_cursor_detail_x.macros.inc"
#include "rom-materials/skills_esper_cursor_y.macros.inc"
#include "rom-materials/skills_esper_cursor_pitch.macros.inc"
#include "rom-materials/main_menu_target_scroll.macros.inc"
#include "rom-materials/item_detail_learn_multiplier_glyph.macros.inc"
#include "rom-materials/skills_target_window_row_width.macros.inc"
#include "rom-materials/skills_list_magic_column_step.macros.inc"
#include "rom-materials/skills_list_other_column_step.macros.inc"
#include "rom-materials/skills_list_row_steps.macros.inc"
#include "rom-materials/skills_list_cursor_y.macros.inc"
#include "rom-materials/skills_list_cursor_x.macros.inc"
#include "rom-materials/skills_list_name_column.macros.inc"
#include "rom-materials/skills_list_text_scroll.macros.inc"
#include "rom-materials/skills_description_column.macros.inc"
#include "rom-materials/skills_description_scrolled_y.macros.inc"
#include "rom-materials/skills_text_vertical_scroll.macros.inc"
#include "rom-materials/item_detail_label_palette.macros.inc"
#include "rom-materials/skills_character_label_palette.macros.inc"
#include "rom-materials/skills_character_hp_caption_scroll.macros.inc"
#include "rom-materials/skills_character_mp_caption_scroll.macros.inc"
#include "rom-materials/config_select_publication_cue.macros.inc"
#include "rom-materials/config_move_publication_cue.macros.inc"
#include "rom-materials/skills_sparse_rows.macros.inc"
#include "rom-materials/skills_dense_rows.macros.inc"
#include "rom-materials/skills_single_column.macros.inc"
#include "rom-materials/skills_double_columns.macros.inc"
#include "rom-materials/skills_magic_scroll_maximum.macros.inc"
#include "rom-materials/skills_option_enabled_palette.macros.inc"
#include "rom-materials/skills_option_disabled_palette.macros.inc"
#include "rom-materials/skills_list_enabled_palette.macros.inc"
#include "rom-materials/skills_list_disabled_palette.macros.inc"
#include "rom-materials/skills_target_default_palette.macros.inc"
#include "rom-materials/skills_character_default_palette.macros.inc"
#include "rom-materials/skills_equipped_esper_palette.macros.inc"
#include "rom-materials/skills_esper_detail_palette.macros.inc"
#include "rom-materials/skills_esper_name_default_palette.macros.inc"
#include "rom-materials/skills_esper_error_palette.macros.inc"
#include "rom-materials/skills_blitz_input_palette.macros.inc"
#include "rom-materials/skills_lore_cost_palette.macros.inc"
#include <algorithm>
#include <numeric>
namespace ff6 {
void FieldMenu::prepare_skill_list() {
    skill_list_.clear();
    const auto& learned = values_.roster.abilities;
    if (skill_ == 0) {
        for (auto id : skills_.esper_order)
            skill_list_.push_back((values_.roster.espers_owned & (1U << id)) ? id : 255);
        skill_list_.push_back(255);
    } else if (skill_ == 1) {
        skill_list_.assign(54, 255);
        for (unsigned i = 0; i < 54; ++i) {
            const auto progress = menu_magic_progress(values_.roster, selected_character(),
                                                      static_cast<std::uint8_t>(i));
            if (progress && (!magic_costs_ || progress == 255))
                skill_list_[skills_.commands.magic_order.at(values_.preferences.magic_order)[i]] =
                    static_cast<std::uint8_t>(i);
        }
    } else {
        const unsigned count = skill_ == 5 ? 256 : skill_ == 4 ? 24 : 8;
        for (unsigned i = 0; i < count; ++i) {
            const bool known = skill_ == 2   ? (learned.bushido & (1U << i)) != 0
                               : skill_ == 3 ? (learned.blitz & (1U << i)) != 0
                               : skill_ == 4 ? (learned.lores[i / 8] & (1U << (i % 8))) != 0
                               : skill_ == 5 ? (learned.rages[i / 8] & (1U << (i % 8))) != 0
                                             : (learned.dances & (1U << i)) != 0;
            skill_list_.push_back(known ? static_cast<std::uint8_t>(i) : 255);
        }
    }
}
void FieldMenu::advance_skills(Buttons press, Buttons repeat, FieldMenuUpdate& out) {
    if (esper_error_ticks_) {
        --esper_error_ticks_;
        return;
    }
    if (page_ == FieldMenuPage::skills) {
        const auto before = cursor_;
        if (held(repeat, Button::up))
            cursor_ = (cursor_ + 6) % 7;
        else if (held(repeat, Button::down))
            cursor_ = (cursor_ + 1) % 7;
        if (before != cursor_)
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
        values_.preferences.cursors.skills[character_slot_].y = static_cast<std::uint8_t>(cursor_);
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
            transition(FieldMenuPage::main);
        } else if (held(press, Button::a)) {
            if (!available_menu_skills(party_,
                                       values_.roster.characters[selected_character()])[cursor_])
                reject(out);
            else {
                out.sounds.push_back(FF6_ROM_VALUE_CONFIG_SELECT_PUBLICATION_CUE_0);
                skill_ = cursor_;
                cursor_ = scroll_ = description_ticks_ = 0;
                magic_costs_ = false;
                prepare_skill_list();
                page_ = FieldMenuPage::skill_list;
                if (skill_ == 1 && values_.preferences.cursor_memory) {
                    const auto saved = values_.preferences.cursors.magic[character_slot_];
                    scroll_ = saved.scroll * FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0;
                    cursor_ = scroll_ + saved.position.y * FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0 + saved.position.x;
                    if (cursor_ >= 54 || scroll_ > FF6_ROM_VALUE_SKILLS_MAGIC_SCROLL_MAXIMUM_0 * FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0)
                        throw std::invalid_argument("Invalid saved magic cursor");
                }
            }
        } else
            change_character(press, out);
        return;
    }
    if (page_ == FieldMenuPage::esper_details) {
        const auto before = cursor_;
        if (held(repeat, Button::up))
            cursor_ = (cursor_ + 6) % 7;
        else if (held(repeat, Button::down))
            cursor_ = (cursor_ + 1) % 7;
        if (before != cursor_) {
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
            description_ticks_ = 0;
        }
        if (held(press, Button::b) || (held(press, Button::a) && cursor_ == 0)) {
            if (held(press, Button::a)) {
                const auto owner = std::ranges::find_if(
                    values_.roster.characters, [&](const auto& c) { return c.esper == selected_; });
                if (owner != values_.roster.characters.end()) {
                    esper_owner_ =
                        static_cast<std::uint8_t>(owner - values_.roster.characters.begin());
                    esper_error_ticks_ = FF6_ROM_VALUE_SKILLS_ESPER_ERROR_HOLD_0;
                    reject(out);
                    return;
                }
                if (!equip_menu_esper(values_.roster, selected_character(),
                                      static_cast<std::uint8_t>(selected_))) {
                    reject(out);
                    return;
                }
                out.sounds.push_back(FF6_ROM_VALUE_CONFIG_SELECT_PUBLICATION_CUE_0);
            } else
                out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
            page_ = FieldMenuPage::skill_list;
            cursor_ = saved_skill_cursor_;
            scroll_ = saved_skill_scroll_;
        }
        return;
    }
    if (page_ == FieldMenuPage::magic_target) {
        const auto party = current_menu_party(values_.roster);
        const auto caster = *party[caster_slot_];
        const auto stats =
            calculate_equipment(values_.roster.characters[caster], party_, equipment_);
        if (held(press, Button::left) || held(press, Button::right) || held(press, Button::l) ||
            held(press, Button::r)) {
            if (skills_.spells[selected_].targeting & 32U)
                multi_target_ = !multi_target_;
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
        }
        if (!multi_target_)
            move_character(repeat, out);
        if (held(press, Button::a)) {
            std::vector<std::uint8_t> targets;
            if (multi_target_) {
                for (auto id : party)
                    if (id)
                        targets.push_back(*id);
            } else
                targets.push_back(selected_character());
            const auto result = use_menu_spell(skills_, values_.roster, caster, stats,
                                               static_cast<std::uint8_t>(selected_), targets,
                                               multi_target_, values_.permissions);
            if (result == FieldItemResult::invalid)
                reject(out);
            else {
                out.game_sounds.push_back(FF6_ROM_VALUE_MENU_HEALING_SOUND_0);
                if (!menu_spell_available(skills_, values_.roster, caster, stats,
                                          static_cast<std::uint8_t>(selected_),
                                          values_.permissions)) {
                    character_slot_ = caster_slot_;
                    transition(FieldMenuPage::skill_list);
                }
            }
        } else if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
            character_slot_ = caster_slot_;
            transition(FieldMenuPage::skill_list);
        }
        return;
    }
    const auto columns = skill_ == 4 ? unsigned(FF6_ROM_VALUE_SKILLS_SINGLE_COLUMN_0) : unsigned(FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0);
    const auto rows = skill_ == 2 || skill_ == 3 || skill_ == 6 ? unsigned(FF6_ROM_VALUE_SKILLS_SPARSE_ROWS_0) : unsigned(FF6_ROM_VALUE_SKILLS_DENSE_ROWS_0);
    const auto before = cursor_;
    if (held(repeat, Button::up) && cursor_ >= columns)
        cursor_ -= columns;
    else if (held(repeat, Button::down) && cursor_ + columns < skill_list_.size())
        cursor_ += columns;
    else if (held(repeat, Button::left) && cursor_ % columns)
        --cursor_;
    else if (held(repeat, Button::right) && cursor_ % columns + 1 < columns &&
             cursor_ + 1 < skill_list_.size())
        ++cursor_;
    else if (held(repeat, Button::l))
        cursor_ = cursor_ >= rows * columns ? cursor_ - rows * columns : cursor_ % columns;
    else if (held(repeat, Button::r))
        cursor_ = std::min(unsigned(skill_list_.size()) - columns + cursor_ % columns,
                           cursor_ + rows * columns);
    if (cursor_ < scroll_)
        scroll_ = cursor_ / columns * columns;
    if (cursor_ >= scroll_ + rows * columns)
        scroll_ = (cursor_ / columns - rows + 1) * columns;
    if (before != cursor_) {
        out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
        description_ticks_ = 0;
    }
    if (skill_ == 1)
        values_.preferences.cursors.magic[character_slot_] = {
            {static_cast<std::uint8_t>(cursor_ % FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0),
             static_cast<std::uint8_t>((cursor_ - scroll_) / FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0)},
            static_cast<std::uint8_t>(scroll_ / FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0)};
    if (held(press, Button::b)) {
        out.sounds.push_back(FF6_ROM_VALUE_CONFIG_MOVE_PUBLICATION_CUE_0);
        page_ = FieldMenuPage::skills;
        cursor_ = skill_;
        scroll_ = 0;
    } else if (skill_ == 1 && held(press, Button::y)) {
        magic_costs_ = !magic_costs_;
        prepare_skill_list();
        out.sounds.push_back(FF6_ROM_VALUE_CONFIG_SELECT_PUBLICATION_CUE_0);
    } else if (held(press, Button::a)) {
        const auto selected = skill_list_.at(cursor_);
        if (skill_ == 0) {
            out.sounds.push_back(FF6_ROM_VALUE_CONFIG_SELECT_PUBLICATION_CUE_0);
            if (selected == 255)
                equip_menu_esper(values_.roster, selected_character(), {});
            else {
                selected_ = selected;
                saved_skill_cursor_ = cursor_;
                saved_skill_scroll_ = scroll_;
                cursor_ = 0;
                page_ = FieldMenuPage::esper_details;
            }
        } else if (skill_ == 1) {
            const auto stats = calculate_equipment(values_.roster.characters[selected_character()],
                                                   party_, equipment_);
            if (!menu_spell_available(skills_, values_.roster, selected_character(), stats,
                                      selected, values_.permissions))
                reject(out);
            else {
                out.sounds.push_back(FF6_ROM_VALUE_CONFIG_SELECT_PUBLICATION_CUE_0);
                selected_ = selected;
                saved_skill_cursor_ = cursor_;
                saved_skill_scroll_ = scroll_;
                if (selected == 42) {
                    item_handoff_ = use_menu_spell(skills_, values_.roster, selected_character(),
                                                   stats, selected, {}, false, values_.permissions);
                    transition(FieldMenuPage::closed);
                } else {
                    caster_slot_ = character_slot_;
                    character_slot_ = 0;
                    while (!current_menu_party(values_.roster)[character_slot_])
                        ++character_slot_;
                    multi_target_ = false;
                    transition(FieldMenuPage::magic_target);
                }
            }
        }
    }
}
RasterScene FieldMenu::skill_scene() {
    const MenuWindowAppearance appearance{values_.window_skin,
                                          values_.window_colors[values_.window_skin]};
    MenuCanvas canvas(assets_, appearance, values_.font_color, intensity_);
    if (page_ == FieldMenuPage::magic_target) {
        auto scene = make_menu_target_party_scene(assets_.shared, party_view());
        draw_party_status(canvas, 65536 - FF6_ROM_VALUE_MAIN_MENU_TARGET_SCROLL_0);
        // The Magic target caller uses a seven-tile name panel and retains
        // SkillsBG3VScrollHDMATbl's two-pixel text shift (_c35812/_c3584b).
        canvas.windows({"itemtargetcharwindow", "itemtargetqtywindow"});
        canvas.rectangle({0, FF6_ROM_VALUE_SKILLS_TARGET_WINDOW_COLUMN_0,
                          (FF6_ROM_VALUE_SKILLS_TARGET_WINDOW_ROW_WIDTH_0 & 255U), (FF6_ROM_VALUE_SKILLS_TARGET_WINDOW_ROW_WIDTH_0 >> 8U),
                          FF6_ROM_VALUE_SKILLS_TARGET_WINDOW_HEIGHT_0});
        canvas.text(assets_.attack_names[selected_], FF6_ROM_VALUE_SKILLS_TARGET_NAME_COLUMN_0 * 8,
                    FF6_ROM_VALUE_SKILLS_TARGET_NAME_ROW_0 * 8 - 3, FF6_ROM_VALUE_SKILLS_TARGET_DEFAULT_PALETTE_0);
        canvas.caption("skillsmpneededtext", FF6_ROM_VALUE_SKILLS_TARGET_DEFAULT_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_TEXT_VERTICAL_SCROLL_0);
        canvas.caption("skillsmpneededtext2", FF6_ROM_VALUE_SKILLS_TARGET_DEFAULT_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_TEXT_VERTICAL_SCROLL_0);
        const auto caster = *current_menu_party(values_.roster)[caster_slot_];
        const auto stats =
            calculate_equipment(values_.roster.characters[caster], party_, equipment_);
        canvas.number(menu_spell_cost(skills_, static_cast<std::uint8_t>(selected_), stats,
                                      values_.preferences.time),
                      2, FF6_ROM_VALUE_SKILLS_TARGET_COST_COLUMN_0 * 8,
                      FF6_ROM_VALUE_SKILLS_TARGET_COST_ROW_0 * 8 - 3, FF6_ROM_VALUE_SKILLS_TARGET_DEFAULT_PALETTE_0);
        for (unsigned i = 0; i < 4; ++i)
            if (current_menu_party(values_.roster)[i] && (multi_target_ || i == character_slot_)) {
                const auto cursor = assets_.shared.character_cursor_positions[i];
                canvas.cursor(cursor[0] + 65536 - FF6_ROM_VALUE_MAIN_MENU_TARGET_SCROLL_0, cursor[1]);
            }
        auto overlay = canvas.finish();
        scene.layers.insert(scene.layers.end(), overlay.layers.begin(), overlay.layers.end());
        scene.objects = std::move(overlay.objects);
        return scene;
    }
    if (page_ == FieldMenuPage::skills)
        canvas.windows({"skillsmagicwindow1", "skillscharwindow1", "skillsdescwindow1",
                        "skillsoptionswindow2", "skillsoptionswindow1"});
    else
        canvas.windows(
            {"skillsmagicwindow2", "skillscharwindow2", "skillsdescwindow2", "skillsmpwindow"});
    const auto& c = values_.roster.characters[selected_character()];
    const auto stats = calculate_equipment(c, party_, equipment_);
    if (page_ != FieldMenuPage::skills)
        canvas.portrait((c.status & 32U)    ? FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0
                        : c.properties == 1 ? 1
                                            : c.graphics,
                        values_.roster.membership[selected_character()].back_row ? FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_0
                            : FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_1, FF6_ROM_VALUE_STATUS_PORTRAIT_Y_0);
    if (c.esper)
        canvas.text(assets_.esper_names[*c.esper], FF6_ROM_VALUE_SKILLS_EQUIPPED_ESPER_COLUMN_0 * 8,
                    FF6_ROM_VALUE_SKILLS_EQUIPPED_ESPER_ROW_0 * 8 - 1, FF6_ROM_VALUE_SKILLS_EQUIPPED_ESPER_PALETTE_0);
    canvas.status(c.status, c.persistent_effects, FF6_ROM_VALUE_SKILLS_STATUS_X_0,
                  FF6_ROM_VALUE_SKILLS_STATUS_Y_0);
    canvas.caption("skillscharleveltext", FF6_ROM_VALUE_SKILLS_CHARACTER_LABEL_PALETTE_0);
    canvas.caption("skillscharhptext", FF6_ROM_VALUE_SKILLS_CHARACTER_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_CHARACTER_HP_CAPTION_SCROLL_0);
    canvas.caption("skillscharmptext", FF6_ROM_VALUE_SKILLS_CHARACTER_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_CHARACTER_MP_CAPTION_SCROLL_0);
    canvas.number(c.level % 100U, 2, FF6_ROM_VALUE_SKILLS_VITAL_COLUMNS_0 * 8,
                  FF6_ROM_VALUE_SKILLS_VITAL_ROWS_0 * 6 + 13, FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0);
    canvas.number(c.hp.current, 4, FF6_ROM_VALUE_SKILLS_VITAL_COLUMNS_1 * 8,
                  FF6_ROM_VALUE_SKILLS_VITAL_ROWS_1 * 6 + 13, FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0);
    canvas.number(maximum_vital(c.hp, 9999), 4, FF6_ROM_VALUE_SKILLS_VITAL_COLUMNS_2 * 8,
                  FF6_ROM_VALUE_SKILLS_VITAL_ROWS_2 * 6 + 13, FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0);
    canvas.number(c.mp.current, 3, FF6_ROM_VALUE_SKILLS_VITAL_COLUMNS_3 * 8 + 8,
                  FF6_ROM_VALUE_SKILLS_VITAL_ROWS_3 * 6 + 13, FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0);
    canvas.number(maximum_vital(c.mp, 999), 3, FF6_ROM_VALUE_SKILLS_VITAL_COLUMNS_4 * 8 + 8,
                  FF6_ROM_VALUE_SKILLS_VITAL_ROWS_4 * 6 + 13, FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0);
    canvas.caption("skillscharhpslashtext", FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_CHARACTER_HP_CAPTION_SCROLL_0);
    canvas.caption("skillscharmpslashtext", FF6_ROM_VALUE_SKILLS_CHARACTER_DEFAULT_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_CHARACTER_MP_CAPTION_SCROLL_0);
    constexpr std::array option_labels{"skillsgenjutext", "skillsmagictext", "skillsbushidotext",
                                       "skillsblitztext", "skillsloretext",  "skillsragetext",
                                       "skillsdancetext"};
    if (page_ == FieldMenuPage::skills) {
        const auto enabled = available_menu_skills(party_, c);
        for (unsigned i = 0; i < 7; ++i)
            canvas.caption(option_labels[i], enabled[i] ? FF6_ROM_VALUE_SKILLS_OPTION_ENABLED_PALETTE_0 : FF6_ROM_VALUE_SKILLS_OPTION_DISABLED_PALETTE_0, 0, -FF6_ROM_VALUE_SKILLS_TEXT_VERTICAL_SCROLL_0);
        canvas.cursor(0, int(assets_.captions.at(option_labels[cursor_]).row) * 8 - 4);
        return canvas.finish();
    }
    constexpr std::array titles{"skillsgenjutitletext",   "skillsmpcosttext",
                                "skillsbushidotitletext", "skillsblitztitletext",
                                "skillsloretitletext",    "skillsragetitletext",
                                "skillsdancetitletext"};
    canvas.caption(titles[skill_], FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
    if (page_ == FieldMenuPage::esper_details) {
        const auto& esper = skills_.espers.at(selected_);
        canvas.text(assets_.esper_names[selected_], FF6_ROM_VALUE_SKILLS_ESPER_NAME_COLUMN_0 * 8,
                    FF6_ROM_VALUE_SKILLS_ESPER_NAME_ROW_0 * 8 - 5, FF6_ROM_VALUE_SKILLS_ESPER_NAME_DEFAULT_PALETTE_0);
        canvas.caption("genjulearnratetext", FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0, 0, -4);
        canvas.caption("genjulearnpcttext", FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0, 0, -4);
        for (unsigned i = 0; i < 5; ++i)
            if (esper.spells[i] < 54) {
                const auto spell = esper.spells[i];
                canvas.text(assets_.attack_names[spell], FF6_ROM_VALUE_SKILLS_ESPER_SPELL_COLUMN_0 * 8,
                            FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
                canvas.glyph(0xd3, FF6_ROM_VALUE_SKILLS_ESPER_SPELL_COLUMN_0 * 8 + 7 * 8,
                             FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
                canvas.glyph(FF6_ROM_VALUE_ITEM_DETAIL_LEARN_MULTIPLIER_GLYPH_0,
                             FF6_ROM_VALUE_SKILLS_ESPER_SPELL_COLUMN_0 * 8 + 10 * 8,
                             FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
                canvas.number(esper.rates[i], 2, FF6_ROM_VALUE_SKILLS_ESPER_SPELL_COLUMN_0 * 8 + 11 * 8,
                              FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
                const auto progress =
                    menu_magic_progress(values_.roster, selected_character(), spell);
                canvas.number(progress == 255 ? 100 : progress % 100U, 3,
                              FF6_ROM_VALUE_SKILLS_ESPER_PROGRESS_COLUMN_0 * 8,
                              FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
                canvas.glyph(0xcd, FF6_ROM_VALUE_SKILLS_ESPER_PROGRESS_COLUMN_0 * 8 + 3 * 8,
                             FF6_ROM_VALUE_SKILLS_ESPER_SPELL_FIRST_ROW_0 * 6 + 25 + int(i) * 12, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
            }
        if (esper.bonus < 17) {
            auto text = assets_.data.at("genjuatleveluptext");
            text.insert(text.end(), assets_.esper_bonus_names[esper.bonus].begin(),
                        assets_.esper_bonus_names[esper.bonus].end());
            canvas.text(text, FF6_ROM_VALUE_SKILLS_ESPER_BONUS_COLUMN_0 * 8,
                        FF6_ROM_VALUE_SKILLS_ESPER_BONUS_ROW_0 * 6 + 25, FF6_ROM_VALUE_SKILLS_ESPER_DETAIL_PALETTE_0);
        }
        canvas.cursor(cursor_ ? FF6_ROM_VALUE_SKILLS_ESPER_CURSOR_DETAIL_X_0 : FF6_ROM_VALUE_SKILLS_ESPER_CURSOR_NAME_X_0,
                      cursor_ ? FF6_ROM_VALUE_SKILLS_ESPER_CURSOR_Y_0 + int(cursor_) * FF6_ROM_VALUE_SKILLS_ESPER_CURSOR_PITCH_0
                              : FF6_ROM_VALUE_SKILLS_ESPER_CURSOR_Y_0);
        if (esper_error_ticks_ && esper_owner_) {
            Bytes message;
            for (auto glyph : values_.roster.characters[*esper_owner_].name) {
                if (glyph == 255)
                    break;
                message.push_back(glyph);
            }
            const auto& suffix = assets_.data.at("genjuequiperrormsgtext");
            message.insert(message.end(), suffix.begin(), suffix.end());
            canvas.text(message, FF6_ROM_VALUE_SKILLS_ESPER_ERROR_COLUMN_0 * 8,
                        FF6_ROM_VALUE_SKILLS_ESPER_ERROR_ROW_0 * 8 - 1, FF6_ROM_VALUE_SKILLS_ESPER_ERROR_PALETTE_0);
        } else if (cursor_ == 0)
            canvas.description(assets_.esper_descriptions[selected_], FF6_ROM_VALUE_SKILLS_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_SKILLS_DESCRIPTION_SCROLLED_Y_0 - 3, description_ticks_);
        else if (cursor_ == 6 && esper.bonus < 17)
            canvas.description(assets_.esper_bonus_descriptions[esper.bonus], FF6_ROM_VALUE_SKILLS_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_SKILLS_DESCRIPTION_SCROLLED_Y_0 - 3,
                               description_ticks_);
        else if (cursor_ < 6 && esper.spells[cursor_ - 1] < 54)
            canvas.description(assets_.magic_descriptions[esper.spells[cursor_ - 1]], FF6_ROM_VALUE_SKILLS_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_SKILLS_DESCRIPTION_SCROLLED_Y_0 - 3,
                               description_ticks_);
        return canvas.finish();
    }
    const unsigned columns = skill_ == 4 ? FF6_ROM_VALUE_SKILLS_SINGLE_COLUMN_0 : FF6_ROM_VALUE_SKILLS_DOUBLE_COLUMNS_0,
                   rows = (skill_ == 2 || skill_ == 3 || skill_ == 6) ? FF6_ROM_VALUE_SKILLS_SPARSE_ROWS_0 : FF6_ROM_VALUE_SKILLS_DENSE_ROWS_0;
    const unsigned pitch = rows == FF6_ROM_VALUE_SKILLS_SPARSE_ROWS_0 ? FF6_ROM_VALUE_SKILLS_LIST_ROW_STEPS_0 : FF6_ROM_VALUE_SKILLS_LIST_ROW_STEPS_2;
    if (skill_ == 0 || skill_ == 1 || skill_ == 4 || skill_ == 5) {
        const auto maximum = static_cast<unsigned>(skill_list_.size()) / columns - rows;
        const unsigned rate = skill_ == 0 ? FF6_ROM_VALUE_SKILLS_SCROLL_RATES_0
                            : skill_ == 1 ? FF6_ROM_VALUE_SKILLS_SCROLL_RATES_1
                            : skill_ == 4 ? FF6_ROM_VALUE_SKILLS_SCROLL_RATES_2
                                          : FF6_ROM_VALUE_SKILLS_SCROLL_RATES_3;
        canvas.scroll_indicator(scroll_ / columns, maximum, rate, FF6_ROM_VALUE_SHOP_SELL_SCROLL_X_0,
                                FF6_ROM_VALUE_SKILLS_SCROLL_Y_0, ticks_);
    }
    for (unsigned i = 0; i < rows * columns && scroll_ + i < skill_list_.size(); ++i) {
        const auto id = skill_list_[scroll_ + i];
        if (id == 255)
            continue;
        const int x = FF6_ROM_VALUE_SKILLS_LIST_NAME_COLUMN_0 * 8 + int(i % columns) *
                      (skill_ == 1 ? FF6_ROM_VALUE_SKILLS_LIST_MAGIC_COLUMN_STEP_0 : FF6_ROM_VALUE_SKILLS_LIST_OTHER_COLUMN_STEP_0),
                  y = 65535 - FF6_ROM_VALUE_SKILLS_LIST_TEXT_SCROLL_0 + 8 + int(i / columns) * int(pitch);
        unsigned color = FF6_ROM_VALUE_SKILLS_LIST_ENABLED_PALETTE_0;
        if (skill_ == 0 && std::ranges::any_of(values_.roster.characters, [&](const auto& actor) {
                return actor.esper == id;
            }))
            color = FF6_ROM_VALUE_SKILLS_LIST_DISABLED_PALETTE_0;
        if (skill_ == 1 && !menu_spell_available(skills_, values_.roster, selected_character(),
                                                 stats, id, values_.permissions))
            color = FF6_ROM_VALUE_SKILLS_LIST_DISABLED_PALETTE_0;
        const auto& name = skill_ == 0   ? assets_.esper_names[id]
                           : skill_ == 1 ? assets_.attack_names[id]
                           : skill_ == 2 ? assets_.bushido_names[id]
                           : skill_ == 3 ? assets_.attack_names[93 + id]
                           : skill_ == 4 ? assets_.attack_names[139 + id]
                           : skill_ == 5 ? assets_.monster_names[id]
                                         : assets_.dance_names[id];
        // C3/55D4–56DE draws only learned input sequences in the Blitz list.
        // The supported USA Rev 1 submenu intentionally has no attack-name labels.
        if (skill_ != 3)
            canvas.text(name, x, y, color);
        if (skill_ == 0) {
            canvas.glyph(0xc7, x + 64, y, color);
            canvas.number(menu_spell_cost(skills_, static_cast<std::uint8_t>(54 + id), stats,
                                          values_.preferences.time),
                          3, x + 72, y, color);
        }
        if (skill_ == 3) {
            const auto& table = assets_.data.at("blitzinputtiletbl");
            for (unsigned input = 0; input < 10; ++input) {
                const auto command = assets_.blitz_inputs[id][input];
                if (command >= 15)
                    throw std::invalid_argument("Invalid Blitz button");
                const auto glyph = read16(table, command * 2);
                if (!glyph)
                    break;
                canvas.glyph(static_cast<std::uint8_t>(glyph), x + 8 + int(input) * 8, y, FF6_ROM_VALUE_SKILLS_BLITZ_INPUT_PALETTE_0,
                             (glyph & 0x4000U) != 0, (glyph & 0x8000U) != 0);
            }
        }
        if (skill_ == 1) {
            const auto progress = menu_magic_progress(values_.roster, selected_character(), id);
            if (magic_costs_)
                canvas.number(menu_spell_cost(skills_, id, stats, values_.preferences.time) % 100U,
                              2, x + 64, y, color);
            else if (progress != 255) {
                canvas.number(progress % 100U, 2, x + 64, y, FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
                const std::array<std::uint8_t, 1> percent{0xcd};
                canvas.text(percent, x + 80, y, FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
            }
        }
        if (skill_ == 4)
            canvas.number(menu_spell_cost(skills_, static_cast<std::uint8_t>(139 + id), stats,
                                          values_.preferences.time),
                          3, x + 88, y, FF6_ROM_VALUE_SKILLS_LORE_COST_PALETTE_0);
    }
    canvas.cursor(FF6_ROM_VALUE_SKILLS_LIST_CURSOR_X_0 + int(cursor_ % columns) *
                      (skill_ == 1 ? FF6_ROM_VALUE_SKILLS_LIST_MAGIC_COLUMN_STEP_0 : FF6_ROM_VALUE_SKILLS_LIST_OTHER_COLUMN_STEP_0),
                  FF6_ROM_VALUE_SKILLS_LIST_CURSOR_Y_0 - 4 + int((cursor_ - scroll_) / columns) * int(pitch));
    const auto id = skill_list_.at(cursor_);
    if (id != 255) {
        const Bytes* description = skill_ == 0   ? &assets_.esper_descriptions[id]
                                   : skill_ == 1 ? &assets_.magic_descriptions[id]
                                   : skill_ == 2 ? &assets_.bushido_descriptions[id]
                                   : skill_ == 3 ? &assets_.blitz_descriptions[id]
                                   : skill_ == 4 ? &assets_.lore_descriptions[id]
                                                 : nullptr;
        if (description)
            canvas.description(*description, FF6_ROM_VALUE_SKILLS_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_SKILLS_DESCRIPTION_SCROLLED_Y_0 - 3, description_ticks_);
        if (skill_ == 1)
            canvas.number(menu_spell_cost(skills_, id, stats, values_.preferences.time), 3, (FF6_ROM_VALUE_SKILLS_LIST_COST_COLUMN_0 - 1) * 8,
                          FF6_ROM_VALUE_SKILLS_LIST_COST_ROW_0 * 8 - 1, FF6_ROM_VALUE_SKILLS_TARGET_DEFAULT_PALETTE_0);
    }
    return canvas.finish();
}
} // namespace ff6
