#include "ff6/field_menu.hpp"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include "rom-materials/status_upper_cells.macros.inc"
#include "rom-materials/status_portrait_y.macros.inc"
#include "rom-materials/status_icon_origin.macros.inc"
#include "rom-materials/status_label_palette.macros.inc"
#include "rom-materials/main_menu_portrait_x.macros.inc"
#include "rom-materials/status_lower_text_layout.macros.inc"
#include "rom-materials/status_command_cursor_x.macros.inc"
#include "rom-materials/status_command_row_pitch.macros.inc"
#include "rom-materials/status_command_text_rows.macros.inc"
#include "rom-materials/gogo_command_cursor_x.macros.inc"
#include "rom-materials/gogo_command_rows.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/gogo_command_visible_rows.macros.inc"
#include "rom-materials/gogo_panel_column.macros.inc"
#include "rom-materials/gogo_panel_row.macros.inc"
#include "rom-materials/gogo_panel_row_width.macros.inc"
#include <algorithm>
namespace ff6 {
void FieldMenu::change_character(Buttons press, FieldMenuUpdate& out) {
    if (inspected_character_) return;
    if (!held(press, Button::l) && !held(press, Button::r))
        return;
    const auto party = current_menu_party(values_.roster);
    const auto old = character_slot_;
    for (unsigned i = 0; i < 4; ++i) {
        character_slot_ = (character_slot_ + (held(press, Button::r) ? 1U : 3U)) % 4;
        if (party[character_slot_] &&
            character_menu_allowed(party_, values_.roster.characters[*party[character_slot_]],
                                   kind_))
            break;
    }
    if (character_slot_ != old) {
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8);
        enter_page();
    }
}
void FieldMenu::advance_status(Buttons press, Buttons repeat, FieldMenuUpdate& out) {
    auto& c = values_.roster.characters[selected_character()];
    if (page_ == FieldMenuPage::gogo_commands) {
        move_list(static_cast<unsigned>(candidates_.size()),
                  FF6_ROM_VALUE_GOGO_COMMAND_VISIBLE_ROWS_0, repeat, out);
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8);
            page_ = FieldMenuPage::status;
            cursor_ = selected_;
        } else if (held(press, Button::a)) {
            c.commands[selected_] = candidates_.at(cursor_);
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255);
            page_ = FieldMenuPage::status;
            cursor_ = selected_;
        }
        return;
    }
    if (held(press, Button::b)) {
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8);
        transition(inspected_character_ ? FieldMenuPage::closed : FieldMenuPage::main);
        return;
    }
    if (c.properties == 12) {
        const auto before = cursor_;
        if (held(repeat, Button::up))
            cursor_ = (cursor_ + 3) % 4;
        else if (held(repeat, Button::down))
            cursor_ = (cursor_ + 1) % 4;
        if (before != cursor_)
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8);
        if (held(press, Button::a) && c.commands[cursor_] != 18) {
            selected_ = cursor_;
            cursor_ = scroll_ = 0;
            candidates_ = menu_gogo_commands(values_.roster, values_.initialized_characters, skills_);
            page_ = FieldMenuPage::gogo_commands;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255);
            return;
        }
    }
    change_character(press, out);
}
RasterScene FieldMenu::status_scene() {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    canvas.windows({"statusmainwindow", "statuscmdwindow", "statustitlewindow"});
    const auto& c = values_.roster.characters[selected_character()];
    const auto stats = calculate_equipment(c, party_, equipment_);
    // Decode final positions from the original tile cells and band schedule.
    // Page address gaps are format metadata; no display-register program runs.
    constexpr std::array<int, 25> lower{
#include "rom-materials/status_lower_text_layout.inc"
    };
    const auto text_y = [](unsigned row) {
        return row < FF6_ROM_VALUE_STATUS_LOWER_TEXT_LAYOUT_0 ? int(row * 8) - 1 : int(row) * FF6_ROM_VALUE_STATUS_LOWER_TEXT_LAYOUT_1 + FF6_ROM_VALUE_STATUS_LOWER_TEXT_LAYOUT_2;
    };
    for (const auto& [id, label] : assets_.captions)
        if (id.starts_with("status")) {
            const unsigned row = label.row + (label.page == 2 ? 32U : 0U);
            canvas.text(label.glyphs, label.column * 8, label.body ? int(row * 8) - 1 : text_y(row),
                        id.ends_with("percenttext") || id.ends_with("slashtext") ? 0 : FF6_ROM_VALUE_STATUS_LABEL_PALETTE_0);
        }
    canvas.text(c.name, FF6_ROM_VALUE_STATUS_UPPER_CELLS_0 * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_1 * 8 - 1);
    canvas.status(c.status, c.persistent_effects, FF6_ROM_VALUE_STATUS_ICON_ORIGIN_0, FF6_ROM_VALUE_STATUS_ICON_ORIGIN_1);
    if (c.esper)
        canvas.text(assets_.esper_names.at(*c.esper), FF6_ROM_VALUE_STATUS_UPPER_CELLS_2 * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_3 * 8 - 1);
    canvas.portrait((c.status & 32U)    ? FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0
                    : c.properties == 1 ? 1
                                        : c.graphics,
                    values_.roster.membership[selected_character()].back_row ? FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_0 : FF6_ROM_VALUE_MAIN_MENU_PORTRAIT_X_1, FF6_ROM_VALUE_STATUS_PORTRAIT_Y_0);
    canvas.number(c.level % 100U, 2, FF6_ROM_VALUE_STATUS_UPPER_CELLS_4 * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_5 * 8 - 1);
    canvas.number(c.hp.current, 4, FF6_ROM_VALUE_STATUS_UPPER_CELLS_6 * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_7 * 8 - 1);
    canvas.number(maximum_vital(c.hp, 9999), 4, FF6_ROM_VALUE_STATUS_UPPER_CELLS_8 * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_9 * 8 - 1);
    canvas.number(c.mp.current, 3, (FF6_ROM_VALUE_STATUS_UPPER_CELLS_10 + 1) * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_11 * 8 - 1);
    canvas.number(maximum_vital(c.mp, 999), 3, (FF6_ROM_VALUE_STATUS_UPPER_CELLS_12 + 1) * 8, FF6_ROM_VALUE_STATUS_UPPER_CELLS_13 * 8 - 1);
    canvas.number(c.experience, 8, lower[21], lower[22]);
    std::uint32_t threshold = 0;
    for (unsigned i = 0; i < std::min(98U, unsigned{c.level}); ++i)
        threshold += party_.experience_growth[i];
    const auto next = c.level >= 99 ? 0U : threshold > c.experience ? threshold - c.experience : 0U;
    canvas.number(next, 8, lower[23], lower[24]);
    for (unsigned i = 0; i < 4; ++i)
        canvas.number(stats.attributes[i], 3, lower[3 + i * 2], lower[4 + i * 2]);
    const std::array<unsigned, 5> right{menu_attack_power(c, items_, stats), stats.defense,
                                        stats.evade, stats.magic_defense, stats.magic_evade};
    for (unsigned i = 0; i < right.size(); ++i)
        canvas.number(right[i], 3, lower[11 + i * 2], lower[12 + i * 2]);
    for (unsigned i = 0; i < 4; ++i) {
        auto cmd = c.commands[i];
        if (cmd >= 32)
            continue;
        for (unsigned j = 0; j < 5; ++j)
            if ((stats.relic_effects[1] & (1U << (6U - j))) &&
                cmd == skills_.commands.relic_changes[j].before)
                cmd = skills_.commands.relic_changes[j].after;
        // Preserve the frozen port's placement bias: original text is cursor
        // +16 X/+4 Y, but this port draws it 16 X/10 Y earlier. Extraction
        // does not establish the accuracy or authorship of that discrepancy.
        canvas.text(assets_.command_names[cmd], FF6_ROM_VALUE_STATUS_COMMAND_CURSOR_X_0 + 16 - 16,
                    FF6_ROM_VALUE_STATUS_COMMAND_TEXT_ROWS_0 + 4 - 10 +
                        int(i) * FF6_ROM_VALUE_STATUS_COMMAND_ROW_PITCH_0);
    }
    if (c.properties == 12 && page_ == FieldMenuPage::status)
        canvas.cursor(FF6_ROM_VALUE_STATUS_COMMAND_CURSOR_X_0 - 16,
                      FF6_ROM_VALUE_STATUS_COMMAND_TEXT_ROWS_0 - 9 +
                          int(cursor_) * FF6_ROM_VALUE_STATUS_COMMAND_ROW_PITCH_0);
    if (page_ == FieldMenuPage::gogo_commands) {
        auto result = canvas.finish();
        MenuCanvas panel(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                         values_.font_color, intensity_);
        const auto height =
            static_cast<std::uint8_t>(std::min(unsigned(FF6_ROM_VALUE_GOGO_COMMAND_VISIBLE_ROWS_0 * 3 / 2),
                                              unsigned(candidates_.size()) * 3 / 2 + 1));
        // A submenu owns the covered rectangle, including its window behind
        // the text. Clip the old page, not just its text's nominal priority.
        // The original uses two window parts. Preserve this port's merged
        // panel, ten-column displacement and extra width column explicitly;
        // ROM-backed coordinates do not establish their visual accuracy.
        for (auto& layer : result.layers)
            for (unsigned y = FF6_ROM_VALUE_GOGO_PANEL_ROW_0 * 8;
                 y < std::min(224U, (unsigned{height} + FF6_ROM_VALUE_GOGO_PANEL_ROW_0 + 2) * 8); ++y)
                layer.lines[y].right = (FF6_ROM_VALUE_GOGO_PANEL_COLUMN_0 - 10) * 8;
        panel.rectangle({0, FF6_ROM_VALUE_GOGO_PANEL_COLUMN_0 - 10,
                         FF6_ROM_VALUE_GOGO_PANEL_ROW_0,
                         (FF6_ROM_VALUE_GOGO_PANEL_ROW_WIDTH_0 >> 8) + 1, height});
        for (unsigned i = 0;
             i < FF6_ROM_VALUE_GOGO_COMMAND_VISIBLE_ROWS_0 && scroll_ + i < candidates_.size(); ++i) {
            const auto cmd = candidates_[scroll_ + i];
            if (cmd < 32)
                // The frozen overlay uses a different horizontal scroll and glyph
                // alignment. Keep those native biases until the accuracy pass.
                panel.text(assets_.command_names[cmd], FF6_ROM_VALUE_GOGO_COMMAND_CURSOR_X_0 - 80 + 16,
                           FF6_ROM_VALUE_GOGO_COMMAND_ROWS_0 + 3 +
                               int(i) * FF6_ROM_VALUE_GOGO_COMMAND_ROWS_1);
        }
        panel.cursor(FF6_ROM_VALUE_GOGO_COMMAND_CURSOR_X_0 - 80,
                     FF6_ROM_VALUE_GOGO_COMMAND_ROWS_0 +
                         int(cursor_ - scroll_) * FF6_ROM_VALUE_GOGO_COMMAND_ROWS_1);
        auto overlay = panel.finish();
        result.layers.insert(result.layers.end(), overlay.layers.begin(), overlay.layers.end());
        result.objects = std::move(overlay.objects);
        return result;
    }
    return canvas.finish();
}
} // namespace ff6
