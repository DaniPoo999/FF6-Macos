#include "ff6/field_menu.hpp"
#include "rom-materials/config_page_cursor_x.macros.inc"
#include "rom-materials/config_page_cursor_y.macros.inc"
#include "rom-materials/config_page_row_pitch.macros.inc"
#include "rom-materials/config_arrange_cursor_x.macros.inc"
#include "rom-materials/config_arrange_cursor_y.macros.inc"
#include "rom-materials/config_arrange_row_pitch.macros.inc"
#include "rom-materials/config_arrange_reset_y.macros.inc"
#include "rom-materials/config_arrow_x.macros.inc"
#include "rom-materials/config_arrow_up_y.macros.inc"
#include "rom-materials/config_arrow_down_y.macros.inc"
#include "rom-materials/config_arrow_period.macros.inc"
#include "rom-materials/config_scroll_step.macros.inc"
#include "rom-materials/config_clip_span.macros.inc"
#include "rom-materials/config_arrange_all_text_y.macros.inc"
#include "rom-materials/config_clip_left.macros.inc"
#include "rom-materials/config_clip_right.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/config_page_one_numeral_cells.macros.inc"
#include "rom-materials/config_page_two_numeral_cells.macros.inc"
#include "rom-materials/config_detail_text_cells.macros.inc"
#include "rom-materials/config_caption_scroll.macros.inc"
#include "rom-materials/config_heading_palette.macros.inc"
#include "rom-materials/config_arrange_palette.macros.inc"
#include "rom-materials/config_arrange_name_columns.macros.inc"
#include "rom-materials/config_reset_font_color.macros.inc"
#include "rom-materials/config_pair_disabled_palette.macros.inc"
#include "rom-materials/config_pair_selected_palette.macros.inc"
#include "rom-materials/config_pair_caption_offset.macros.inc"
#include "rom-materials/config_page_one_disabled_palette.macros.inc"
#include "rom-materials/config_page_one_selected_palette.macros.inc"
#include "rom-materials/config_page_two_disabled_palette.macros.inc"
#include "rom-materials/config_page_two_selected_palette.macros.inc"
#include <algorithm>
namespace ff6 {
void FieldMenu::advance_configuration(Buttons press, Buttons repeat, FieldMenuUpdate& out) {
    auto& prefs = values_.preferences;
    const bool right = held(repeat, Button::right), left = held(repeat, Button::left);
    if (page_ == FieldMenuPage::controllers) {
        move_character(repeat, out);
        if (left || right) {
            const auto mask = static_cast<std::uint8_t>(1U << character_slot_);
            if (right)
                prefs.second_player_characters |= mask;
            else
                prefs.second_player_characters &= static_cast<std::uint8_t>(~mask);
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::b) || held(press, Button::start)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            transition(FieldMenuPage::configuration);
        }
        return;
    }
    if (page_ == FieldMenuPage::command_arrange) {
        const auto old = cursor_;
        if (held(repeat, Button::up))
            cursor_ = (cursor_ + 4) % 5;
        else if (held(repeat, Button::down))
            cursor_ = (cursor_ + 1) % 5;
        if (old != cursor_)
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        if (held(press, Button::b) || held(press, Button::start)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            transition(FieldMenuPage::configuration);
        } else if (held(press, Button::a)) {
            if (cursor_ == 4) {
                for (auto id : current_menu_party(values_.roster))
                    if (id) {
                        auto& c = values_.roster.characters[*id];
                        if (c.properties && *c.properties != 12 &&
                            *c.properties < party_.characters.size())
                            c.commands = party_.characters[*c.properties].commands;
                    }
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            } else if (current_menu_party(values_.roster)[cursor_]) {
                character_slot_ = cursor_;
                cursor_ = 0;
                page_ = FieldMenuPage::command_select;
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            }
        }
        return;
    }
    if (page_ == FieldMenuPage::command_select || page_ == FieldMenuPage::command_move) {
        const auto old = cursor_;
        if (held(repeat, Button::up))
            cursor_ = 0;
        if (held(repeat, Button::down))
            cursor_ = 3;
        if (left)
            cursor_ = 1;
        if (right)
            cursor_ = 2;
        if (old != cursor_)
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        if (held(press, Button::b)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
            if (page_ == FieldMenuPage::command_move) {
                page_ = FieldMenuPage::command_select;
                cursor_ = selected_;
            } else {
                page_ = FieldMenuPage::command_arrange;
                cursor_ = character_slot_;
            }
        } else if (held(press, Button::a)) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            if (page_ == FieldMenuPage::command_select) {
                selected_ = cursor_;
                page_ = FieldMenuPage::command_move;
            } else {
                auto& commands = values_.roster.characters[selected_character()].commands;
                std::swap(commands[selected_], commands[cursor_]);
                page_ = FieldMenuPage::command_select;
            }
        }
        return;
    }
    if (held(press, Button::b)) {
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        transition(FieldMenuPage::main);
        return;
    }
    const auto old = cursor_;
    if (held(repeat, Button::up)) {
        if (cursor_)
            --cursor_;
        else if (option_) {
            option_ = 0;
            cursor_ = 8;
        }
    } else if (held(repeat, Button::down)) {
        if (cursor_ < (option_ ? 5U : 8U))
            ++cursor_;
        else if (!option_) {
            option_ = 1;
            cursor_ = 0;
        }
    }
    if (held(press, Button::l) || held(press, Button::r)) {
        option_ = 1 - option_;
        config_scroll_ = option_ * 256;
        cursor_ = 0;
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
    } else if (old != cursor_)
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
    const auto adjust = [&](auto& value, unsigned maximum) {
        value = static_cast<std::remove_reference_t<decltype(value)>>(
            right             ? std::min(unsigned{value} + 1, maximum)
            : unsigned{value} ? unsigned{value} - 1
                              : 0);
    };
    if (left || right) {
        if (!option_)
            switch (cursor_) {
            case 0:
                prefs.battle_wait = right;
                break;
            case 1:
                adjust(prefs.battle_speed, 5);
                break;
            case 2:
                adjust(prefs.message_speed, 5);
                break;
            case 3:
                prefs.short_commands = right;
                break;
            case 4:
                prefs.hide_gauge = right;
                break;
            case 5:
                prefs.mono = right;
                break;
            case 6:
                prefs.cursor_memory = right;
                break;
            case 7:
                prefs.remove_equipment = !right;
                break;
            case 8:
                prefs.multiplayer = right;
                break;
            default:
                throw std::logic_error("Invalid config row");
            }
        else
            switch (cursor_) {
            case 0:
                adjust(prefs.magic_order, 5);
                break;
            case 1:
                adjust(values_.window_skin, 7);
                break;
            case 2:
                adjust(color_index_, 7);
                break;
            default: {
                auto& color = color_index_
                                  ? values_.window_colors[values_.window_skin][color_index_ - 1]
                                  : values_.font_color;
                const auto shift = (cursor_ - 3) * 5;
                unsigned component = (color >> shift) & 31U;
                adjust(component, 31);
                color = static_cast<Rgb555>((color & ~(31U << shift)) | (component << shift));
                break;
            }
            }
    }
    if (held(press, Button::a)) {
        if (!option_ && cursor_ == 3 && prefs.short_commands) {
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            transition(FieldMenuPage::command_arrange);
        } else if (!option_ && cursor_ == 8 && prefs.multiplayer) {
            character_slot_ = 0;
            while (!current_menu_party(values_.roster)[character_slot_])
                ++character_slot_;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            transition(FieldMenuPage::controllers);
        } else if (option_ && cursor_ >= 2) {
            if (!color_index_)
                values_.font_color = FF6_ROM_VALUE_CONFIG_RESET_FONT_COLOR_0;
            else
                values_.window_colors[values_.window_skin] =
                    assets_.shared.windows.skins[values_.window_skin].colors;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        }
    }
}
RasterScene FieldMenu::configuration_scene() {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    const auto& prefs = values_.preferences;
    // Decode the authored signed scroll explicitly; caption glyph origins use
    // the shared native one-pixel baseline in MenuCanvas.
    constexpr int encoded_scroll = FF6_ROM_VALUE_CONFIG_CAPTION_SCROLL_0;
    constexpr int caption_offset = -(encoded_scroll < 32768 ? encoded_scroll : encoded_scroll - 65536);
    if (page_ == FieldMenuPage::controllers) {
        canvas.windows({"charctrlwindow"});
        canvas.caption("charctrltitletext", FF6_ROM_VALUE_CONFIG_ARRANGE_PALETTE_0);
        const auto party = current_menu_party(values_.roster);
        for (unsigned i = 0; i < 4; ++i)
            if (party[i]) {
                canvas.text(values_.roster.characters[*party[i]].name, 32, 47 + int(i) * 40);
                canvas.number(1, 1, 136, 47 + int(i) * 40,
                              (prefs.second_player_characters & (1U << i))
                                  ? FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0
                                  : FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0);
                canvas.number(2, 1, 184, 47 + int(i) * 40,
                              (prefs.second_player_characters & (1U << i))
                                  ? FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0
                                  : FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0);
            }
        canvas.cursor(112, 44 + int(character_slot_) * 40);
        return canvas.finish();
    }
    if (page_ != FieldMenuPage::configuration) {
        canvas.windows({"cmdarrangetitlewindow", "cmdarrangechar1leftwindow",
                        "cmdarrangechar1rightwindow", "cmdarrangechar2leftwindow",
                        "cmdarrangechar2rightwindow", "cmdarrangechar3leftwindow",
                        "cmdarrangechar3rightwindow", "cmdarrangechar4leftwindow",
                        "cmdarrangechar4rightwindow"});
        canvas.caption("cmdarrangetitletext", FF6_ROM_VALUE_CONFIG_ARRANGE_PALETTE_0);
        const auto party = current_menu_party(values_.roster);
        constexpr std::array x{
#include "rom-materials/config_command_text_x.inc"
        }, y{
#include "rom-materials/config_command_text_y.inc"
        };
        // All source rows agree on their horizontal address bits. Convert
        // from the original two-byte cell/page format to a native pixel column.
        constexpr int name_column =
            (FF6_ROM_VALUE_CONFIG_ARRANGE_NAME_COLUMNS_0 + 32 - (0x7849 / 2) % 32) % 32;
        constexpr std::array row_text_y{
#include "rom-materials/config_arrange_all_text_y.inc"
        };
        for (unsigned i = 0; i < 4; ++i)
            if (party[i]) {
                const auto& c = values_.roster.characters[*party[i]];
                canvas.text(c.name, name_column * 8, row_text_y[i * 4],
                            FF6_ROM_VALUE_CONFIG_ARRANGE_PALETTE_0);
                for (unsigned j = 0; j < 4; ++j)
                    if (c.commands[j] < 32)
                        canvas.text(assets_.command_names[c.commands[j]], x[j], row_text_y[i * 4 + j]);
            }
        if (page_ == FieldMenuPage::command_arrange)
            canvas.cursor(FF6_ROM_VALUE_CONFIG_ARRANGE_CURSOR_X_0, cursor_ == 4 ? FF6_ROM_VALUE_CONFIG_ARRANGE_RESET_Y_0 : FF6_ROM_VALUE_CONFIG_ARRANGE_CURSOR_Y_0 + int(cursor_) * FF6_ROM_VALUE_CONFIG_ARRANGE_ROW_PITCH_0);
        else {
            canvas.cursor(x[cursor_] - 16, y[cursor_] - 3 + int(character_slot_) * FF6_ROM_VALUE_CONFIG_ARRANGE_ROW_PITCH_0);
            if (page_ == FieldMenuPage::command_move && (ticks_ / 8) % 2)
                canvas.cursor(x[selected_] - 16, y[selected_] - 3 + int(character_slot_) * FF6_ROM_VALUE_CONFIG_ARRANGE_ROW_PITCH_0);
        }
        return canvas.finish();
    }
    canvas.windows({"configmainwindow", "configlabelwindow"});
    canvas.caption("configtitletext", FF6_ROM_VALUE_CONFIG_HEADING_PALETTE_0, 0, caption_offset);
    if ((ticks_ / FF6_ROM_VALUE_CONFIG_ARROW_PERIOD_0) % 2)
        canvas.page_arrow(option_ != 0, FF6_ROM_VALUE_CONFIG_ARROW_X_0, option_ ? FF6_ROM_VALUE_CONFIG_ARROW_UP_Y_0 : FF6_ROM_VALUE_CONFIG_ARROW_DOWN_Y_0);
    auto result = canvas.finish();
    // Both pages travel through the original visible body rectangle. The
    // window/title stay stationary; no generic device scroll is involved.
    for (unsigned page = 0; page < 2; ++page) {
        auto content = configuration_page_scene(page);
        const auto offset = static_cast<int>(config_scroll_) - static_cast<int>(page) * 256;
        for (auto& layer : content.layers) {
            for (unsigned y = 0; y < layer.lines.size(); ++y) {
                layer.lines[y].offset_y += offset;
                layer.lines[y].enabled = y >= FF6_ROM_VALUE_CONFIG_CLIP_SPAN_0 && y < (FF6_ROM_VALUE_CONFIG_CLIP_SPAN_0 + FF6_ROM_VALUE_CONFIG_CLIP_SPAN_1);
                layer.lines[y].left = FF6_ROM_VALUE_CONFIG_CLIP_LEFT_0;
                layer.lines[y].right = FF6_ROM_VALUE_CONFIG_CLIP_RIGHT_0 + 1;
            }
            result.layers.push_back(std::move(layer));
        }
        for (auto& sprite : content.objects->sprites) {
            sprite.y -= offset;
            result.objects->sprites.push_back(std::move(sprite));
        }
    }
    return result;
}
RasterScene FieldMenu::configuration_page_scene(unsigned page) {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    const auto& prefs = values_.preferences;
    // Decode the authored signed scroll explicitly; caption glyph origins use
    // the shared native one-pixel baseline in MenuCanvas.
    constexpr int encoded_scroll = FF6_ROM_VALUE_CONFIG_CAPTION_SCROLL_0;
    constexpr int caption_offset = -(encoded_scroll < 32768 ? encoded_scroll : encoded_scroll - 65536);
    // The complete original tables also supply coordinates folded into
    // unrolled native calls. These rows are affine in the supported ROM.
    constexpr int battle_number_x =
        FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_0;
    constexpr int battle_number_y =
        FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_1;
    constexpr int battle_number_step =
        (FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_2 - FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_0);
    constexpr int message_number_x =
        FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_12;
    constexpr int message_number_y =
        FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_13;
    constexpr int message_number_step =
        (FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_14 - FF6_ROM_VALUE_CONFIG_PAGE_ONE_NUMERAL_CELLS_12);
    constexpr int magic_number_x =
        FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_0;
    constexpr int magic_number_y =
        FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_1;
    constexpr int magic_number_step =
        (FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_2 - FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_0);
    constexpr int window_number_x =
        FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_12;
    constexpr int window_number_y =
        FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_13;
    constexpr int window_number_step =
        (FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_14 - FF6_ROM_VALUE_CONFIG_PAGE_TWO_NUMERAL_CELLS_12);
    constexpr int magic_type_x =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_0;
    constexpr int magic_type_y =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_1;
    constexpr int magic_type_step =
        (FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_3 - FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_1);
    constexpr int color_number_x =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_8;
    constexpr int color_number_y =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_9;
    constexpr int color_number_step =
        (FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_11 - FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_9);
    constexpr int color_bar_x =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_14;
    constexpr int color_bar_y =
        FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_15;
    constexpr int color_bar_step =
        (FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_17 - FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_15);
    const auto caption = [&](const char* name, unsigned color = 0) {
        canvas.caption(name, color, 0, caption_offset);
    };
    if (!page) {
        constexpr std::array labels{
            "configbattlemodetext", "configbattlespeedtext", "configmsgspeedtext",
            "configcmdsettext",     "configgaugetext",       "configsoundtext",
            "configcursortext",     "configreequiptext",     "configcontrollertext"};
        for (auto label : labels)
            caption(label, FF6_ROM_VALUE_CONFIG_HEADING_PALETTE_0);
        const auto pair = [&](const char* a, const char* b, bool second) {
            // Compact font choices share the source's selected/unselected
            // colors; their native conditional arguments are generated too.
            canvas.caption(a, second ? FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0
                                     : FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0,
                           0, FF6_ROM_VALUE_CONFIG_PAIR_CAPTION_OFFSET_0);
            canvas.caption(b, second ? FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0
                                     : FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0,
                           0, FF6_ROM_VALUE_CONFIG_PAIR_CAPTION_OFFSET_0);
        };
        pair("configactivetext", "configwaittext", prefs.battle_wait);
        pair("configcmdwindowtext", "configcmdshorttext", prefs.short_commands);
        pair("configgaugeontext", "configgaugeofftext", prefs.hide_gauge);
        pair("configstereotext", "configmonotext", prefs.mono);
        pair("configresettext", "configmemorytext", prefs.cursor_memory);
        pair("configemptytext", "configoptimumtext", !prefs.remove_equipment);
        pair("configctrlsingletext", "configctrlmultitext", prefs.multiplayer);
        caption("configfasttext");
        caption("configslowtext");
        for (unsigned i = 0; i < 6; ++i) {
            canvas.number(i + 1, 1, battle_number_x + int(i) * battle_number_step, battle_number_y,
                          i == prefs.battle_speed ? FF6_ROM_VALUE_CONFIG_PAGE_ONE_SELECTED_PALETTE_0
                                                 : FF6_ROM_VALUE_CONFIG_PAGE_ONE_DISABLED_PALETTE_0);
            canvas.number(i + 1, 1, message_number_x + int(i) * message_number_step, message_number_y,
                          i == prefs.message_speed ? FF6_ROM_VALUE_CONFIG_PAGE_ONE_SELECTED_PALETTE_0
                                                 : FF6_ROM_VALUE_CONFIG_PAGE_ONE_DISABLED_PALETTE_0);
        }
        if (config_scroll_ == 0 && !option_)
            canvas.cursor(FF6_ROM_VALUE_CONFIG_PAGE_CURSOR_X_0, FF6_ROM_VALUE_CONFIG_PAGE_CURSOR_Y_0 + int(cursor_) * FF6_ROM_VALUE_CONFIG_PAGE_ROW_PITCH_0);
    } else {
        for (auto label : {"configmagicordertext", "configwindowlabeltext", "configcolortext",
                           "configcolorrtext", "configcolorgtext", "configcolorbtext"})
            caption(label, FF6_ROM_VALUE_CONFIG_HEADING_PALETTE_0);
        for (unsigned i = 0; i < 6; ++i)
            canvas.number(i + 1, 1, magic_number_x + int(i) * magic_number_step, magic_number_y,
                          i == prefs.magic_order ? FF6_ROM_VALUE_CONFIG_PAGE_TWO_SELECTED_PALETTE_0
                                                 : FF6_ROM_VALUE_CONFIG_PAGE_TWO_DISABLED_PALETTE_0);
        for (auto label :
             {"configmagicorderatext", "configmagicorderbtext", "configmagicorderctext"})
            caption(label);
        const auto& types = assets_.data.at("configmagictypetext");
        const auto& order = assets_.data.at("configmagictypetextptrs");
        for (unsigned i = 0; i < 3; ++i)
            canvas.text(ByteView{types}.subspan(order[prefs.magic_order * 4 + i], 10),
                        magic_type_x,
                        magic_type_y + int(i) * magic_type_step);
        for (unsigned i = 0; i < 8; ++i)
            canvas.number(i + 1, 1, window_number_x + int(i) * window_number_step, window_number_y,
                          i == values_.window_skin ? FF6_ROM_VALUE_CONFIG_PAGE_TWO_SELECTED_PALETTE_0
                                                 : FF6_ROM_VALUE_CONFIG_PAGE_TWO_DISABLED_PALETTE_0);
        canvas.caption("configfonttext", color_index_ ? FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0
                                                      : FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0,
                       0, FF6_ROM_VALUE_CONFIG_PAIR_CAPTION_OFFSET_0);
        canvas.caption("configwindowtext", color_index_ ? FF6_ROM_VALUE_CONFIG_PAIR_SELECTED_PALETTE_0
                                                        : FF6_ROM_VALUE_CONFIG_PAIR_DISABLED_PALETTE_0,
                       0, FF6_ROM_VALUE_CONFIG_PAIR_CAPTION_OFFSET_0);
        for (unsigned i = 0; i < 7; ++i)
            canvas.swatch(values_.window_colors[values_.window_skin][i],
                          FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_6 + int(i) * 8,
                          FF6_ROM_VALUE_CONFIG_DETAIL_TEXT_CELLS_7);
        const auto color = color_index_
                               ? values_.window_colors[values_.window_skin][color_index_ - 1]
                               : values_.font_color;
        for (unsigned component = 0; component < 3; ++component) {
            const auto value = (color >> (component * 5)) & 31U;
            canvas.number(value, 2, color_number_x, color_number_y + int(component) * color_number_step);
            canvas.color_bar(value, color_bar_x, color_bar_y + int(component) * color_bar_step);
        }
        constexpr std::array y{
#include "rom-materials/config_page_two_cursor_y.inc"
        };
        if (config_scroll_ == 256 && option_)
            canvas.cursor(FF6_ROM_VALUE_CONFIG_PAGE_CURSOR_X_0, y[cursor_]);
    }
    return canvas.finish();
}
} // namespace ff6
