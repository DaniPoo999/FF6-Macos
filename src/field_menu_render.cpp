#include "ff6/field_menu.hpp"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include "rom-materials/item_target_name_column.macros.inc"
#include "rom-materials/item_target_name_row.macros.inc"
#include "rom-materials/item_target_quantity_column.macros.inc"
#include "rom-materials/item_target_quantity_row.macros.inc"
#include "rom-materials/item_count_column.macros.inc"
#include "rom-materials/item_count_row.macros.inc"
#include "rom-materials/menu_item_list_rows.macros.inc"
#include "rom-materials/menu_item_list_cursor_x.macros.inc"
#include "rom-materials/menu_item_list_cursor_y.macros.inc"
#include "rom-materials/menu_item_list_row_step.macros.inc"
#include "rom-materials/menu_item_list_columns.macros.inc"
#include "rom-materials/menu_item_list_symbol_x.macros.inc"
#include "rom-materials/menu_item_list_scrollbar.macros.inc"
#include "rom-materials/menu_item_list_vertical_endpoints.macros.inc"
#include "rom-materials/menu_item_description_column.macros.inc"
#include "rom-materials/menu_item_description_row.macros.inc"
#include "rom-materials/main_menu_target_scroll.macros.inc"
#include "rom-materials/menu_rare_item_cursor_right.macros.inc"
#include "rom-materials/item_option_cursor_y.macros.inc"
#include "rom-materials/item_detail_element_left.macros.inc"
#include "rom-materials/item_detail_element_right.macros.inc"
#include "rom-materials/item_detail_element_top.macros.inc"
#include "rom-materials/item_detail_element_bottom.macros.inc"
#include "rom-materials/item_detail_usage_column.macros.inc"
#include "rom-materials/item_detail_usage_row.macros.inc"
#include "rom-materials/item_detail_user_column.macros.inc"
#include "rom-materials/item_detail_user_row.macros.inc"
#include "rom-materials/item_detail_user_column_step.macros.inc"
#include "rom-materials/item_detail_separator_glyph.macros.inc"
#include "rom-materials/item_detail_learn_multiplier_glyph.macros.inc"
#include "rom-materials/item_detail_window_width.macros.inc"
#include "rom-materials/item_detail_window_height.macros.inc"
#include "rom-materials/item_detail_label_palette.macros.inc"
#include "rom-materials/item_detail_stat_column.macros.inc"
#include "rom-materials/item_detail_separator_column.macros.inc"
#include "rom-materials/item_detail_modifier_column.macros.inc"
#include "rom-materials/item_detail_value_column.macros.inc"
#include "rom-materials/item_detail_stat_first_row.macros.inc"
#include "rom-materials/item_detail_stat_row_step.macros.inc"
#include "rom-materials/item_detail_runic_row.macros.inc"
#include "rom-materials/item_detail_two_hand_row.macros.inc"
#include "rom-materials/item_detail_swdtech_row.macros.inc"
#include "rom-materials/party_equip_left_column.macros.inc"
#include "rom-materials/party_equip_right_column.macros.inc"
#include "rom-materials/party_equip_character_row_step.macros.inc"
#include "rom-materials/equip_text_row_step.macros.inc"
#include "rom-materials/equip_name_row.macros.inc"
#include "rom-materials/equip_value_column.macros.inc"
#include "rom-materials/equip_portrait_x.macros.inc"
#include "rom-materials/equip_portrait_y.macros.inc"
#include "rom-materials/equip_slot_label_column.macros.inc"
#include "rom-materials/equip_slot_item_column.macros.inc"
#include "rom-materials/equip_slot_first_row.macros.inc"
#include "rom-materials/equip_relic_first_row.macros.inc"
#include "rom-materials/equip_stat_label_column.macros.inc"
#include "rom-materials/equip_stat_first_row.macros.inc"
#include "rom-materials/equip_stat_new_column.macros.inc"
#include "rom-materials/equip_stat_arrow_column.macros.inc"
#include "rom-materials/equip_stat_arrow_glyph.macros.inc"
#include "rom-materials/equip_label_palette.macros.inc"
#include "rom-materials/party_equip_item_last_row.macros.inc"
#include "rom-materials/party_equip_name_row_gap.macros.inc"
#include "rom-materials/item_detail_scroll_extent.macros.inc"
#include "rom-materials/equip_option_cursor_y.macros.inc"
#include "rom-materials/relic_option_cursor_right.macros.inc"
#include "rom-materials/equip_list_rows.macros.inc"
#include "rom-materials/equip_list_row_step.macros.inc"
#include "rom-materials/equip_list_text_column.macros.inc"
#include "rom-materials/equip_list_scroll_x.macros.inc"
#include "rom-materials/equip_list_scroll_y.macros.inc"
#include "rom-materials/equip_list_scroll_scale.macros.inc"
#include "rom-materials/equip_comparison_higher_palette.macros.inc"
#include "rom-materials/equip_comparison_lower_palette.macros.inc"
#include "rom-materials/equip_comparison_equal_palette.macros.inc"
#include "rom-materials/item_unknown_power_ids.macros.inc"
#include "rom-materials/item_title_palette.macros.inc"
#include "rom-materials/item_options_palette.macros.inc"
#include "rom-materials/item_target_default_palette.macros.inc"
#include "rom-materials/item_count_palette.macros.inc"
#include "rom-materials/rare_item_name_palette.macros.inc"
#include "rom-materials/item_detail_default_palette.macros.inc"
#include "rom-materials/party_equip_default_palette.macros.inc"
#include "rom-materials/equip_name_palette.macros.inc"
#include "rom-materials/equip_content_default_palette.macros.inc"
#include "rom-materials/equip_options_palette.macros.inc"
#include "rom-materials/equip_title_palette.macros.inc"
#include "rom-materials/item_list_enabled_palette.macros.inc"
#include "rom-materials/item_list_disabled_palette.macros.inc"
#include "rom-materials/equip_list_enabled_palette.macros.inc"
#include "rom-materials/equip_list_disabled_palette.macros.inc"
#include <algorithm>

namespace ff6 {
// The native row loop folds its visible extent into one comparison. Bind that
// extent to the original cursor table as well as the extracted count/pitch.
static_assert(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                  FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0 ==
              FF6_ROM_VALUE_MENU_ITEM_LIST_VERTICAL_ENDPOINTS_0 -
                  FF6_ROM_VALUE_MENU_ITEM_LIST_VERTICAL_ENDPOINTS_1);

// Rare-item text and cursor columns use the same original separation.
static_assert((FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_1 -
               FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_0) * 8 ==
              FF6_ROM_VALUE_MENU_RARE_ITEM_CURSOR_RIGHT_0 - FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0);

RasterScene FieldMenu::item_scene() {
    const MenuWindowAppearance appearance{values_.window_skin,
                                          values_.window_colors[values_.window_skin]};
    MenuCanvas canvas(assets_, appearance, values_.font_color, intensity_);
    if (page_ == FieldMenuPage::item_target) {
        auto scene = make_menu_target_party_scene(assets_.shared, party_view());
        draw_party_status(canvas, 65536 - FF6_ROM_VALUE_MAIN_MENU_TARGET_SCROLL_0);
        canvas.windows({"itemtargetcharwindow", "itemtargetitemnamewindow", "itemtargetqtywindow"});
        // ItemBG3VScrollHDMATbl keeps 12-pixel rows in this target panel too.
        canvas.caption("itemownedtext", FF6_ROM_VALUE_ITEM_TARGET_DEFAULT_PALETTE_0, 0, -4);
        const auto slot = values_.inventory.slots[selected_];
        canvas.text(assets_.item_names[slot.item], FF6_ROM_VALUE_ITEM_TARGET_NAME_COLUMN_0 * 8,
                    FF6_ROM_VALUE_ITEM_TARGET_NAME_ROW_0 * 8 - 1, FF6_ROM_VALUE_ITEM_TARGET_DEFAULT_PALETTE_0);
        // ItemTargetDrawQty uses cell (5,9), shifted up eight pixels by that
        // row's retained text spacing. It belongs below the Owned caption.
        canvas.number(slot.quantity, 2, FF6_ROM_VALUE_ITEM_TARGET_QUANTITY_COLUMN_0 * 8,
                      FF6_ROM_VALUE_ITEM_TARGET_QUANTITY_ROW_0 * 8 - 9, FF6_ROM_VALUE_ITEM_TARGET_DEFAULT_PALETTE_0);
        const auto cursor = assets_.shared.character_cursor_positions[character_slot_];
        canvas.cursor(cursor[0] + (65536 - FF6_ROM_VALUE_MAIN_MENU_TARGET_SCROLL_0), cursor[1]);
        auto overlay = canvas.finish();
        scene.layers.insert(scene.layers.end(), overlay.layers.begin(), overlay.layers.end());
        scene.objects = std::move(overlay.objects);
        return scene;
    }
    canvas.windows({"itemoptionswindow", "itemtitlewindow", "itemdescwindow", "itemlistwindow"});
    canvas.caption("itemtitletext", FF6_ROM_VALUE_ITEM_TITLE_PALETTE_0);
    for (auto name : {"itemoptionusetext", "itemoptionarrangetext", "itemoptionraretext"})
        canvas.caption(name, FF6_ROM_VALUE_ITEM_OPTIONS_PALETTE_0);
    if (page_ == FieldMenuPage::rare_items) {
        std::vector<unsigned> rare;
        for (unsigned i = 0; i < 20; ++i)
            if (values_.rare_items[i])
                rare.push_back(i);
        canvas.number(static_cast<std::uint32_t>(rare.size()), 3,
                      FF6_ROM_VALUE_ITEM_COUNT_COLUMN_0 * 8,
                      FF6_ROM_VALUE_ITEM_COUNT_ROW_0 * 8 - 1, FF6_ROM_VALUE_ITEM_COUNT_PALETTE_0);
        for (unsigned i = 0; i < rare.size(); ++i)
            canvas.text(assets_.rare_names[rare[i]],
                        FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_0 * 8 +
                            static_cast<int>(i % 2) *
                                (FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_1 -
                                 FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_0) * 8,
                        FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 + 3 +
                            static_cast<int>(i / 2) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0, FF6_ROM_VALUE_RARE_ITEM_NAME_PALETTE_0);
        canvas.cursor(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0 +
                          static_cast<int>(cursor_ % 2) *
                              (FF6_ROM_VALUE_MENU_RARE_ITEM_CURSOR_RIGHT_0 -
                               FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0),
                      FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                          static_cast<int>(cursor_ / 2) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0);
        if (cursor_ < rare.size())
            canvas.description(assets_.rare_descriptions[rare[cursor_]],
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_ROW_0 * 8 - 5,
                               description_ticks_);
        return canvas.finish();
    }
    if (page_ == FieldMenuPage::item_details) {
        const auto id = values_.inventory.slots[selected_].item;
        const auto& item = items_.items[id];
        const auto& record = equipment_.items[id];
        // The original drawer slides the item page right by 120 pixels. Its
        // second panel contains statistics; the first keeps elemental affinity
        // and the initialized characters whose properties match the equip mask.
        if (item.type == 1) {
            canvas.text(assets_.captions.at("itemattackelementtext").glyphs,
                        FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_LEFT_0 * 8,
                        FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_TOP_0 * 6 + 9,
                        FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
            canvas.elements(record.attack_elements_or_halved,
                            FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_LEFT_0 * 8,
                            FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_TOP_0 * 6 + 17);
        } else {
            constexpr std::array labels{"itemelementhalftext", "itemelementabsorbtext",
                                        "itemelementnulltext", "itemelementweaktext"};
            const std::array masks{record.attack_elements_or_halved, record.absorbed,
                                   record.nullified, record.weak};
            for (unsigned i = 0; i < 4; ++i) {
                // Keep the native six-pixel half-row and glyph baseline while
                // obtaining the authored grid cells from the original captions.
                const int x = (FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_LEFT_0 + int(i % 2) *
                    (FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_RIGHT_0 -
                     FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_LEFT_0)) * 8;
                const int y = (FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_TOP_0 + int(i / 2) *
                    (FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_BOTTOM_0 -
                     FF6_ROM_VALUE_ITEM_DETAIL_ELEMENT_TOP_0)) * 6 + 9;
                canvas.text(assets_.captions.at(labels[i]).glyphs, x, y,
                            FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
                canvas.elements(masks[i], x, y + 8);
            }
        }
        Bytes usage = assets_.item_names[id];
        const auto padding = std::find(usage.begin() + 1, usage.end(), 255);
        usage.erase(padding, usage.end());
        const auto& suffix = assets_.data.at("itemusagetext");
        usage.insert(usage.end(), suffix.begin(), suffix.end());
        canvas.text(usage, FF6_ROM_VALUE_ITEM_DETAIL_USAGE_COLUMN_0 * 8,
                    FF6_ROM_VALUE_ITEM_DETAIL_USAGE_ROW_0 * 6 + 9,
                    FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
        unsigned printed = 0;
        for (const auto who : menu_item_users(values_.roster, values_.initialized_characters, item)) {
            canvas.text(values_.roster.characters[who].name,
                        FF6_ROM_VALUE_ITEM_DETAIL_USER_COLUMN_0 * 8 +
                            int(printed % 3) * FF6_ROM_VALUE_ITEM_DETAIL_USER_COLUMN_STEP_0 * 4,
                        FF6_ROM_VALUE_ITEM_DETAIL_USER_ROW_0 * 6 + 9 + int(printed / 3) * 12, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            ++printed;
        }
        auto scene = canvas.finish();
        if (!detail_scroll_)
            return scene;
        for (auto& layer : scene.layers)
            for (auto& line : layer.lines) {
                line.offset_x -= static_cast<int>(detail_scroll_);
                line.left = static_cast<std::uint16_t>(detail_scroll_ + 8);
            }
        scene.objects.reset();
        MenuCanvas stats(assets_, appearance, values_.font_color, intensity_);
        stats.rectangle({0, 1, 1, FF6_ROM_VALUE_ITEM_DETAIL_WINDOW_WIDTH_0,
                         FF6_ROM_VALUE_ITEM_DETAIL_WINDOW_HEIGHT_0});
        constexpr std::array names{"itemstrengthtext", "itemspeedtext",     "itemstaminatext",
                                   "itemmagpwrtext",   "itemattackpwrtext", "itemdefensetext",
                                   "itemevadetext",    "itemmagdeftext",    "itemmblocktext"};
        // Local panel placement and native text baselines stay unchanged. The
        // authored columns and row progression come from the original cells.
        const auto& modifiers = assets_.data.at("statmodifiertexttable");
        const auto& evasion = assets_.data.at("evademodifiertexttbl");
        for (unsigned i = 0; i < names.size(); ++i) {
            const int y = FF6_ROM_VALUE_ITEM_DETAIL_STAT_FIRST_ROW_0 * 6 + 13 +
                          int(i) * FF6_ROM_VALUE_ITEM_DETAIL_STAT_ROW_STEP_0 * 6;
            stats.text(assets_.captions.at(names[i]).glyphs,
                       (FF6_ROM_VALUE_ITEM_DETAIL_STAT_COLUMN_0 + 1) * 8, y,
                       FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
            stats.glyph(FF6_ROM_VALUE_ITEM_DETAIL_SEPARATOR_GLYPH_0,
                        (FF6_ROM_VALUE_ITEM_DETAIL_SEPARATOR_COLUMN_0 + 1) * 8, y,
                        FF6_ROM_VALUE_ITEM_DETAIL_LABEL_PALETTE_0);
            if (i < 4) {
                const int bonus = record.attribute_bonuses[i];
                const unsigned index = static_cast<unsigned>(bonus < 0 ? 8 - bonus : bonus);
                stats.text(ByteView{modifiers}.subspan(index * 2, 2),
                           (FF6_ROM_VALUE_ITEM_DETAIL_MODIFIER_COLUMN_0 + 1) * 8, y, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            } else if (i == 4 && item.type == 1) {
                // The identities are ROM data; preserve the native comparison shape.
                if (id == FF6_ROM_VALUE_ITEM_UNKNOWN_POWER_IDS_3 ||
                    id == FF6_ROM_VALUE_ITEM_UNKNOWN_POWER_IDS_2 ||
                    id == FF6_ROM_VALUE_ITEM_UNKNOWN_POWER_IDS_1 ||
                    id == FF6_ROM_VALUE_ITEM_UNKNOWN_POWER_IDS_0)
                    stats.text(assets_.captions.at("itemunknownattacktext").glyphs,
                               (FF6_ROM_VALUE_ITEM_DETAIL_VALUE_COLUMN_0 + 1) * 8, y, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
                else
                    stats.number(record.power, 3, (FF6_ROM_VALUE_ITEM_DETAIL_VALUE_COLUMN_0 + 1) * 8, y, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            } else if (i == 5 || i == 7)
                stats.number(item.type == 1 ? 0
                             : i == 5       ? record.power
                                            : record.hit_rate_or_magic_defense,
                             3, (FF6_ROM_VALUE_ITEM_DETAIL_VALUE_COLUMN_0 + 1) * 8, y, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            else if (i == 6 || i == 8) {
                // Equipment uses the signed evade table internally. Its menu
                // display is the authored modifier text, including plus/minus.
                const int bonus = i == 6 ? record.evade_bonus : record.magic_evade_bonus;
                const auto index =
                    static_cast<unsigned>(bonus <= 5 ? bonus >= 0 ? bonus : 5 - bonus : 0);
                stats.text(ByteView{evasion}.subspan(index * 4, 3),
                           (FF6_ROM_VALUE_ITEM_DETAIL_VALUE_COLUMN_0 + 1) * 8, y, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            }
        }
        if (item.type == 1) {
            if (item.effects & 128U)
                stats.text(assets_.captions.at("itemrunictext").glyphs,
                           (FF6_ROM_VALUE_ITEM_DETAIL_STAT_COLUMN_0 + 1) * 8,
                           FF6_ROM_VALUE_ITEM_DETAIL_RUNIC_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            if (item.two_handed)
                stats.text(assets_.captions.at("item2handtext").glyphs,
                           (FF6_ROM_VALUE_ITEM_DETAIL_STAT_COLUMN_0 + 1) * 8,
                           FF6_ROM_VALUE_ITEM_DETAIL_TWO_HAND_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            if (item.effects & 2U)
                stats.text(assets_.captions.at("itembushidotext").glyphs,
                           (FF6_ROM_VALUE_ITEM_DETAIL_STAT_COLUMN_0 + 1) * 8,
                           FF6_ROM_VALUE_ITEM_DETAIL_SWDTECH_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
        } else if (item.learn_rate && item.learned_spell < 54) {
            stats.text(assets_.attack_names[item.learned_spell],
                       (FF6_ROM_VALUE_ITEM_DETAIL_STAT_COLUMN_0 + 1) * 8,
                       FF6_ROM_VALUE_ITEM_DETAIL_TWO_HAND_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            stats.glyph(FF6_ROM_VALUE_ITEM_DETAIL_LEARN_MULTIPLIER_GLYPH_0, 80,
                        FF6_ROM_VALUE_ITEM_DETAIL_TWO_HAND_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
            stats.number(item.learn_rate, 2, 88, FF6_ROM_VALUE_ITEM_DETAIL_TWO_HAND_ROW_0 * 6 + 25, FF6_ROM_VALUE_ITEM_DETAIL_DEFAULT_PALETTE_0);
        }
        auto left = stats.finish();
        for (auto& layer : left.layers)
            for (auto& line : layer.lines) {
                line.offset_x += static_cast<int>(FF6_ROM_VALUE_ITEM_DETAIL_SCROLL_EXTENT_0 - detail_scroll_);
                line.right = static_cast<std::uint16_t>(detail_scroll_ + 8);
            }
        scene.layers.insert(scene.layers.end(), left.layers.begin(), left.layers.end());
        return scene;
    }
    canvas.scroll_indicator(scroll_, 255 - FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0,
                            FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_0,
                            FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_1,
                            FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_2, ticks_);
    for (unsigned row = 0; row < FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 && scroll_ + row < 255; ++row) {
        const auto slot = values_.inventory.slots[scroll_ + row];
        if (slot.item == 255)
            continue;
        const unsigned color = field_item_available(items_, slot.item, values_.permissions)
                                   ? FF6_ROM_VALUE_ITEM_LIST_ENABLED_PALETTE_0
                                   : FF6_ROM_VALUE_ITEM_LIST_DISABLED_PALETTE_0;
        canvas.inventory_name(slot.item, FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_0 * 8,
                              FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 + 3 +
                                  static_cast<int>(row) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0, color);
        canvas.number(slot.quantity % 100U, 2, FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_1 * 8,
                      FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 + 3 +
                          static_cast<int>(row) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0, color);
        const auto icon = items_.items[slot.item].icon;
        if (icon >= 0xd8 && icon <= 0xe7)
            canvas.text(assets_.item_symbols[icon - 0xd8], FF6_ROM_VALUE_MENU_ITEM_LIST_SYMBOL_X_0,
                        FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 + 3 +
                            static_cast<int>(row) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0,
                        color);
    }
    if (page_ == FieldMenuPage::item_options) {
        constexpr std::array x{
#include "rom-materials/item_option_cursor_x.inc"
            };
        canvas.cursor(x[option_], FF6_ROM_VALUE_ITEM_OPTION_CURSOR_Y_0);
    } else {
        canvas.cursor(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0,
                      FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                          static_cast<int>(cursor_ - scroll_) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0);
        if (page_ == FieldMenuPage::item_move && selected_ >= scroll_ &&
            selected_ < scroll_ + FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 &&
            (ticks_ / 8) % 2)
            canvas.cursor(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0,
                          FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                              static_cast<int>(selected_ - scroll_) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0);
        const auto id = values_.inventory.slots[cursor_].item;
        if (id != 255)
            canvas.description(assets_.item_descriptions[id],
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_ROW_0 * 8 - 5, description_ticks_);
        canvas.number(static_cast<std::uint32_t>(std::ranges::count_if(
                          values_.inventory.slots, [](auto slot) { return slot.item != 255; })),
                      3, FF6_ROM_VALUE_ITEM_COUNT_COLUMN_0 * 8,
                      FF6_ROM_VALUE_ITEM_COUNT_ROW_0 * 8 - 1, FF6_ROM_VALUE_ITEM_COUNT_PALETTE_0);
    }
    return canvas.finish();
}
RasterScene FieldMenu::equipment_scene() {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    // The compiler shares the last equipment row between the six item draws
    // and character name. Bind those folded offsets to their original cells.
    static_assert(FF6_ROM_VALUE_PARTY_EQUIP_ITEM_LAST_ROW_0 >=
                  2 * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0);
    static_assert(FF6_ROM_VALUE_PARTY_EQUIP_ITEM_LAST_ROW_0 >=
                  FF6_ROM_VALUE_PARTY_EQUIP_NAME_ROW_GAP_0);
    if (page_ == FieldMenuPage::party_equipment) {
        canvas.windows({"partyequipwindow"});
        const auto party = current_menu_party(values_.roster);
        for (unsigned slot = 0; slot < 4; ++slot)
            if (party[slot]) {
                const auto& c = values_.roster.characters[*party[slot]];
                canvas.text(c.name, FF6_ROM_VALUE_PARTY_EQUIP_LEFT_COLUMN_0 * 8,
                            (FF6_ROM_VALUE_PARTY_EQUIP_ITEM_LAST_ROW_0 -
                             FF6_ROM_VALUE_PARTY_EQUIP_NAME_ROW_GAP_0) * 6 + 1 +
                                static_cast<int>(slot) * FF6_ROM_VALUE_PARTY_EQUIP_CHARACTER_ROW_STEP_0 * 6, FF6_ROM_VALUE_PARTY_EQUIP_DEFAULT_PALETTE_0);
                for (unsigned i = 0; i < 6; ++i)
                    canvas.text(assets_.item_names[c.equipment[i]],
                                i % 2 ? FF6_ROM_VALUE_PARTY_EQUIP_RIGHT_COLUMN_0 * 8
                                      : FF6_ROM_VALUE_PARTY_EQUIP_LEFT_COLUMN_0 * 8,
                                (FF6_ROM_VALUE_PARTY_EQUIP_ITEM_LAST_ROW_0 -
                                 2 * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0) * 6 + 1 +
                                    static_cast<int>(slot) * FF6_ROM_VALUE_PARTY_EQUIP_CHARACTER_ROW_STEP_0 * 6 +
                                    static_cast<int>(i / 2) * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0 * 6, FF6_ROM_VALUE_PARTY_EQUIP_DEFAULT_PALETTE_0);
            }
        return canvas.finish();
    }
    if (page_ == FieldMenuPage::equipment)
        canvas.windows({"equipbtmwindow1", "equiptopwindow1", "equipoptionswindow"});
    else
        canvas.windows({"equipbtmwindow2", "equiptopwindow2", "equiptitlewindow"});
    const auto& c = values_.roster.characters[selected_character()];
    canvas.text(c.name, FF6_ROM_VALUE_EQUIP_VALUE_COLUMN_0 * 8,
                FF6_ROM_VALUE_EQUIP_NAME_ROW_0 * 6 + 1, FF6_ROM_VALUE_EQUIP_NAME_PALETTE_0);
    canvas.portrait((c.status & 32U) ? FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0 : c.properties == 1 ? 1 : c.graphics,
                    FF6_ROM_VALUE_EQUIP_PORTRAIT_X_0, FF6_ROM_VALUE_EQUIP_PORTRAIT_Y_0);
    constexpr std::array labels{"equiprhandtext", "equiplhandtext",  "equipheadtext",
                                "equipbodytext",  "equiprelic1text", "equiprelic2text"};
    const unsigned first = relics_ ? 4 : 0, count = relics_ ? 2 : 4;
    const int top = relics_ ? FF6_ROM_VALUE_EQUIP_RELIC_FIRST_ROW_0 * 6 + 1
                            : FF6_ROM_VALUE_EQUIP_SLOT_FIRST_ROW_0 * 6 + 1;
    for (unsigned i = 0; i < count; ++i) {
        canvas.text(assets_.captions.at(labels[first + i]).glyphs,
                    FF6_ROM_VALUE_EQUIP_SLOT_LABEL_COLUMN_0 * 8,
                    top + static_cast<int>(i) * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0 * 6,
                    FF6_ROM_VALUE_EQUIP_LABEL_PALETTE_0);
        canvas.text(assets_.item_names[c.equipment[first + i]],
                    FF6_ROM_VALUE_EQUIP_SLOT_ITEM_COLUMN_0 * 8,
                    top + static_cast<int>(i) * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0 * 6, FF6_ROM_VALUE_EQUIP_CONTENT_DEFAULT_PALETTE_0);
    }
    if (reequip_ticks_) {
        canvas.caption("equipchangedmsgtext", FF6_ROM_VALUE_EQUIP_OPTIONS_PALETTE_0, 0, -4);
        canvas.caption(values_.preferences.remove_equipment ? "equipemptymsgtext"
                                                            : "equipoptimummsgtext", FF6_ROM_VALUE_EQUIP_OPTIONS_PALETTE_0, 0, -4);
    } else if (page_ == FieldMenuPage::equipment) {
        if (relics_) {
            canvas.caption("relicoptionequiptext", FF6_ROM_VALUE_EQUIP_OPTIONS_PALETTE_0, 0, -4);
            canvas.caption("relicoptionremovetext", FF6_ROM_VALUE_EQUIP_OPTIONS_PALETTE_0, 0, -4);
            canvas.cursor(option_ ? FF6_ROM_VALUE_RELIC_OPTION_CURSOR_RIGHT_0
                                  : FF6_ROM_VALUE_EQUIP_OPTION_CURSOR_Y_0,
                          FF6_ROM_VALUE_EQUIP_OPTION_CURSOR_Y_0);
        } else {
            for (auto label : {"equipoptionequiptext", "equipoptionoptimumtext",
                               "equipoptionremovetext", "equipoptionemptytext"})
                canvas.caption(label, FF6_ROM_VALUE_EQUIP_OPTIONS_PALETTE_0, 0, -4);
            constexpr std::array x{
#include "rom-materials/equipment_option_cursor_x.inc"
            };
            canvas.cursor(x[option_], FF6_ROM_VALUE_EQUIP_OPTION_CURSOR_Y_0);
        }
    } else {
        canvas.caption(removing_ ? "equiptitleremovetext" : "equiptitleequiptext", FF6_ROM_VALUE_EQUIP_TITLE_PALETTE_0, 0, -4);
        canvas.cursor(0, top + 1 + static_cast<int>(equip_slot_ - first) * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0 * 6);
    }
    const auto stats = calculate_equipment(c, party_, equipment_);
    if (relics_ && !reequip_ticks_ && page_ != FieldMenuPage::equipment) {
        const auto id = page_ == FieldMenuPage::equip_list
                            ? values_.inventory.slots[candidates_.at(cursor_)].item
                            : c.equipment[equip_slot_];
        if (id != 255)
            canvas.description(assets_.item_descriptions[id],
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_COLUMN_0 * 8,
                               FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_ROW_0 * 8 - 9, description_ticks_);
    }
    auto preview = c;
    bool comparison = false;
    if (page_ == FieldMenuPage::equip_list) {
        // The source cursor grid is also the visible text-row grid. Its fixed
        // format extent agrees with the list-height property used by navigation.
        constexpr std::array cursor_y{
#include "rom-materials/equip_list_cursor_rows.inc"
        };
        static_assert(cursor_y.size() == FF6_ROM_VALUE_EQUIP_LIST_ROWS_0);
        if (candidates_.size() > FF6_ROM_VALUE_EQUIP_LIST_ROWS_0) {
            const auto maximum = static_cast<unsigned>(candidates_.size()) - FF6_ROM_VALUE_EQUIP_LIST_ROWS_0;
            canvas.scroll_indicator(scroll_, maximum, FF6_ROM_VALUE_EQUIP_LIST_SCROLL_SCALE_0 / maximum,
                                    FF6_ROM_VALUE_EQUIP_LIST_SCROLL_X_0,
                                    FF6_ROM_VALUE_EQUIP_LIST_SCROLL_Y_0, ticks_);
        }
        const auto item = values_.inventory.slots[candidates_.at(cursor_)].item;
        comparison = menu_can_equip(items_, c, stats, equip_slot_, item);
        if (comparison)
            preview.equipment[equip_slot_] = item;
        for (unsigned i = 0; i < cursor_y.size() && scroll_ + i < candidates_.size(); ++i) {
            const auto slot = values_.inventory.slots[candidates_[scroll_ + i]];
            const unsigned color = menu_can_equip(items_, c, stats, equip_slot_, slot.item)
                                       ? FF6_ROM_VALUE_EQUIP_LIST_ENABLED_PALETTE_0
                                       : FF6_ROM_VALUE_EQUIP_LIST_DISABLED_PALETTE_0;
            canvas.text(assets_.item_names[slot.item], FF6_ROM_VALUE_EQUIP_LIST_TEXT_COLUMN_0 * 8,
                        cursor_y[i] - 1, color);
        }
        canvas.cursor(0, cursor_y.front() +
                             static_cast<int>(cursor_ - scroll_) * FF6_ROM_VALUE_EQUIP_LIST_ROW_STEP_0);
    }
    const auto display = [&](const Character& character, const EquipmentStats& attributes) {
        return std::array<unsigned, 9>{attributes.attributes[0],
                                       attributes.attributes[1],
                                       attributes.attributes[2],
                                       attributes.attributes[3],
                                       menu_attack_power(character, items_, attributes),
                                       attributes.defense,
                                       attributes.evade,
                                       attributes.magic_defense,
                                       attributes.magic_evade};
    };
    const auto old_values = display(c, stats),
               new_values = display(preview, calculate_equipment(preview, party_, equipment_));
    constexpr std::array names{"equipstrengthtext", "equipspeedtext",     "equipstaminatext",
                               "equipmagpwrtext",   "equipattackpwrtext", "equipdefensetext",
                               "equipevadetext",    "equipmagdeftext",    "equipmagevadetext"};
    for (unsigned i = 0; i < names.size(); ++i) {
        const int y = FF6_ROM_VALUE_EQUIP_STAT_FIRST_ROW_0 * 6 + 1 +
                      static_cast<int>(i) * FF6_ROM_VALUE_EQUIP_TEXT_ROW_STEP_0 * 6;
        canvas.text(assets_.captions.at(names[i]).glyphs,
                    FF6_ROM_VALUE_EQUIP_STAT_LABEL_COLUMN_0 * 8, y,
                    FF6_ROM_VALUE_EQUIP_LABEL_PALETTE_0);
        canvas.number(old_values[i], 3, FF6_ROM_VALUE_EQUIP_VALUE_COLUMN_0 * 8, y, FF6_ROM_VALUE_EQUIP_CONTENT_DEFAULT_PALETTE_0);
        canvas.glyph(FF6_ROM_VALUE_EQUIP_STAT_ARROW_GLYPH_0,
                     FF6_ROM_VALUE_EQUIP_STAT_ARROW_COLUMN_0 * 8, y,
                     FF6_ROM_VALUE_EQUIP_LABEL_PALETTE_0);
        if (comparison) {
            canvas.number(new_values[i], 3, FF6_ROM_VALUE_EQUIP_STAT_NEW_COLUMN_0 * 8, y,
                          new_values[i] > old_values[i] ? FF6_ROM_VALUE_EQUIP_COMPARISON_HIGHER_PALETTE_0
                          : new_values[i] < old_values[i] ? FF6_ROM_VALUE_EQUIP_COMPARISON_LOWER_PALETTE_0
                                                         : FF6_ROM_VALUE_EQUIP_COMPARISON_EQUAL_PALETTE_0);
        }
    }
    return canvas.finish();
}
} // namespace ff6
