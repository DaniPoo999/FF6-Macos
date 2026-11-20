#include "ff6/shop.hpp"
#include "rom-materials/shop_party_arrow_tile.macros.inc"
#include "rom-materials/shop_equipped_icon_tile.macros.inc"
#include "rom-materials/shop_equal_icon_tile.macros.inc"
#include "rom-materials/shop_comparison_first_tile.macros.inc"
#include "rom-materials/shop_actor_grid.macros.inc"
#include "rom-materials/shop_comparison_origin.macros.inc"
#include "rom-materials/shop_party_marker_origin.macros.inc"
#include "rom-materials/shop_label_palette.macros.inc"
#include "rom-materials/shop_fixed_text_positions.macros.inc"
#include "rom-materials/shop_caption_scroll.macros.inc"
#include "rom-materials/shop_row_pitch.macros.inc"
#include "rom-materials/shop_buy_price_x.macros.inc"
#include "rom-materials/shop_cursor_y.macros.inc"
#include "rom-materials/shop_quantity_cursor_x.macros.inc"
#include "rom-materials/shop_actor_pose_interval.macros.inc"
#include "rom-materials/shop_comparison_interval.macros.inc"
#include "rom-materials/shop_comparison_tile_span.macros.inc"
#include "rom-materials/shop_sell_name_x.macros.inc"
#include "rom-materials/shop_sell_quantity_x.macros.inc"
#include "rom-materials/shop_sell_symbol_x.macros.inc"
#include "rom-materials/shop_sell_scroll_maximum.macros.inc"
#include "rom-materials/shop_sell_scroll_rate.macros.inc"
#include "rom-materials/shop_sell_scroll_x.macros.inc"
#include "rom-materials/shop_sell_scroll_y.macros.inc"
#include "rom-materials/shop_sell_cursor_x.macros.inc"
#include "rom-materials/shop_sell_rows.macros.inc"
#include "rom-materials/shop_description_x.macros.inc"
#include "rom-materials/shop_actor_normal_y.macros.inc"
#include "rom-materials/shop_actor_alternate_y.macros.inc"
#include "rom-materials/shop_description_window_y.macros.inc"
#include <algorithm>

namespace ff6 {
RasterScene ShopMenu::scene() const {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    const bool quantity = page_ == ShopPage::buy_quantity || page_ == ShopPage::sell_quantity;
    const bool buying = page_ == ShopPage::buy || page_ == ShopPage::buy_quantity;
    if (quantity)
        canvas.windows({"shopqtytitlewindow", "shopqtymsgwindow", "shopqtyitemwindow",
                        "shopqtydescwindow", "shopqtycharwindow", "shopqtygilwindow"});
    else if (buying)
        canvas.windows(
            {"shopbuytitlewindow", "shopbuymsgwindow", "shopbuylistwindow", "shopbuycharwindow"});
    else
        canvas.windows({"shopselltitlewindow", "shopsellmsgwindow", "shopselloptionswindow",
                        "shopsellgilwindow", "shopselllistwindow"});
    // The ROM display bands determine caption offsets and stock-row spacing.
    // Fixed text pairs are gil, owned, equipped, transaction name/quantity/total,
    // item power and the first stock name, in that order.
    const auto caption = [&](std::string_view name, unsigned color = 0) {
        canvas.caption(name, color, 0, -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_0);
    };
    caption("shoptypetext_0" + std::to_string(definition().type), FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0);
    caption(message_);
    if (!buying && !quantity) {
        caption("shopoptionstext");
        caption("shopgiltext1", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0);
        canvas.number(values_.inventory.gil, 7, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_0,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_1);
        if (page_ == ShopPage::sell) {
            canvas.scroll_indicator(scroll_, FF6_ROM_VALUE_SHOP_SELL_SCROLL_MAXIMUM_0,
                FF6_ROM_VALUE_SHOP_SELL_SCROLL_RATE_0, FF6_ROM_VALUE_SHOP_SELL_SCROLL_X_0,
                FF6_ROM_VALUE_SHOP_SELL_SCROLL_Y_0, ticks_);
            for (unsigned row = 0;
                 row < (FF6_ROM_VALUE_SHOP_SELL_ROWS_2 - FF6_ROM_VALUE_SHOP_SELL_ROWS_0) /
                           FF6_ROM_VALUE_SHOP_SELL_ROWS_1 && scroll_ + row < 255; ++row) {
                const auto stock = values_.inventory.slots[scroll_ + row];
                if (stock.item == 255)
                    continue;
                // The frozen port places sell cursors four pixels below the source
                // table, with text one pixel above the cursor. Preserve this
                // native discrepancy for binary identity; do not call it accurate.
                const int y = FF6_ROM_VALUE_SHOP_SELL_ROWS_0 + 4 - 1 +
                              int(row) * FF6_ROM_VALUE_SHOP_SELL_ROWS_1;
                canvas.inventory_name(stock.item, FF6_ROM_VALUE_SHOP_SELL_NAME_X_0, y);
                canvas.number(stock.quantity, 2, FF6_ROM_VALUE_SHOP_SELL_QUANTITY_X_0, y);
                const auto icon = data_.items.items[stock.item].icon;
                if (icon >= 0xd8 && icon <= 0xe7)
                    canvas.text(assets_.item_symbols[icon - 0xd8], FF6_ROM_VALUE_SHOP_SELL_SYMBOL_X_0, y);
            }
            canvas.cursor(FF6_ROM_VALUE_SHOP_SELL_CURSOR_X_0, FF6_ROM_VALUE_SHOP_SELL_ROWS_0 + 4 +
                          int(cursor_ - scroll_) * FF6_ROM_VALUE_SHOP_SELL_ROWS_1);
        } else {
            constexpr std::array<int, 3> x{
#include "rom-materials/shop_option_cursor_x.inc"
            };
            canvas.cursor(x[option_], FF6_ROM_VALUE_SHOP_CURSOR_Y_0);
        }
    }
    const auto selected = quantity ? transaction_item_
                          : buying ? definition().items[cursor_]
                                   : values_.inventory.slots[cursor_].item;
    if (buying || quantity) {
        caption("shopgiltext1", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0);
        canvas.number(values_.inventory.gil, 7, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_0,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_1);
        canvas.caption("shopownedtext", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0, 0,
            -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_1);
        canvas.caption("shopequippedtext", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0, 0,
            -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_3);
        canvas.number(shop_owned(values_.inventory, selected), 2, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_2,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_3);
        canvas.number(shop_equipped(values_.roster, values_.initialized_characters, selected),
                      2, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_4,
                          FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_5);
    }
    if (buying && !quantity) {
        for (unsigned row = 0; row < definition().items.size(); ++row) {
            const auto item = definition().items[row];
            canvas.text(assets_.item_names[item], FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_14,
                FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_15 + int(row) * FF6_ROM_VALUE_SHOP_ROW_PITCH_0);
            canvas.number(shop_price(data_.prices[item], definition().price_rule, graphics_), 5,
                          FF6_ROM_VALUE_SHOP_BUY_PRICE_X_0,
                              FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_15 + int(row) * FF6_ROM_VALUE_SHOP_ROW_PITCH_0);
        }
        canvas.cursor(0, FF6_ROM_VALUE_SHOP_CURSOR_Y_0 + int(cursor_) * FF6_ROM_VALUE_SHOP_ROW_PITCH_0);
    }
    if (quantity) {
        canvas.caption("shopgiltext2", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0, 0,
            -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_2);
        canvas.text(assets_.item_names[selected], FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_6,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_7);
        canvas.number(quantity_, 2, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_8,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_9);
        const auto unit =
            buying ? shop_price(data_.prices[selected], definition().price_rule, graphics_)
                   : data_.prices[selected] / 2;
        canvas.number(static_cast<std::uint32_t>(unit) * quantity_, 7,
            FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_10, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_11);
        const auto& item = data_.items.items[selected];
        if (item.type > 0 && item.type < 6) {
            canvas.caption(item.type == 1 ? "shoppowertext" : "shopdefensetext",
                FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_3);
            canvas.caption("shopdotstext", FF6_ROM_VALUE_SHOP_LABEL_PALETTE_0, 0,
                -FF6_ROM_VALUE_SHOP_CAPTION_SCROLL_3);
            canvas.number(item.power, 3, FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_12,
                FF6_ROM_VALUE_SHOP_FIXED_TEXT_POSITIONS_13);
        }
        // Anchor to the authored description panel. Keep the frozen native
        // three-pixel glyph inset explicit; BG3 raster/glyph alignment remains
        // a reconstruction discrepancy, not an extracted original coordinate.
        canvas.description(assets_.item_descriptions[selected], FF6_ROM_VALUE_SHOP_DESCRIPTION_X_0,
                           FF6_ROM_VALUE_SHOP_DESCRIPTION_WINDOW_Y_0 + 3);
        canvas.cursor(FF6_ROM_VALUE_SHOP_QUANTITY_CURSOR_X_0, FF6_ROM_VALUE_SHOP_CURSOR_Y_0);
    }
    if (buying || quantity) {
        unsigned position = 0;
        const auto characters = shop_character_mask(values_.roster, values_.initialized_characters);
        for (unsigned i = 0; i < 16; ++i) {
            const auto& c = values_.roster.characters[i];
            if (!(characters & (1U << i)))
                continue;
            // Native geometry comes from the complete source coordinate grid.
            const int x = FF6_ROM_VALUE_SHOP_ACTOR_GRID_1 +
                          int(position % FF6_ROM_VALUE_SHOP_ACTOR_GRID_0) * FF6_ROM_VALUE_SHOP_ACTOR_GRID_3;
            const int y = FF6_ROM_VALUE_SHOP_ACTOR_GRID_2 +
                          int(position / FF6_ROM_VALUE_SHOP_ACTOR_GRID_0) * FF6_ROM_VALUE_SHOP_ACTOR_GRID_4;
            const bool can_equip =
                buying && (data_.items.items[selected].characters & (1U << *c.properties));
            const unsigned pose = can_equip && ticks_ % (2 * FF6_ROM_VALUE_SHOP_ACTOR_POSE_INTERVAL_0) >=
                                 FF6_ROM_VALUE_SHOP_ACTOR_POSE_INTERVAL_0 ? 1 : 0;
            canvas.actor(actors_[*c.properties][pose], actor_colors_[*c.properties], x,
                         y + (pose ? FF6_ROM_VALUE_SHOP_ACTOR_ALTERNATE_Y_0
                                   : FF6_ROM_VALUE_SHOP_ACTOR_NORMAL_Y_0));
            if (buying) {
                if (values_.roster.membership[i].party == values_.roster.active_party) {
                    // PartyArrowSprite selects a 16x16 image. The decoded sheet has
                    // sixteen 8x8 tiles per row; a vertical flip swaps its tile rows.
                    std::array<Tile, 4> arrow{
                        assets_.sprites.at(FF6_ROM_VALUE_SHOP_PARTY_ARROW_TILE_0 + 16),
                        assets_.sprites.at(FF6_ROM_VALUE_SHOP_PARTY_ARROW_TILE_0 + 17),
                        assets_.sprites.at(FF6_ROM_VALUE_SHOP_PARTY_ARROW_TILE_0),
                        assets_.sprites.at(FF6_ROM_VALUE_SHOP_PARTY_ARROW_TILE_0 + 1)};
                    for (auto& tile : arrow)
                        for (unsigned row = 0; row < 4; ++row)
                            for (unsigned col = 0; col < 8; ++col)
                                std::swap(tile[row * 8 + col], tile[(7 - row) * 8 + col]);
                    canvas.actor(arrow, assets_.shared.cursor_colors, x,
                                 y - FF6_ROM_VALUE_SHOP_ACTOR_GRID_2 +
                                     FF6_ROM_VALUE_SHOP_PARTY_MARKER_ORIGIN_0 - 2);
                }
                const auto comparison = shop_comparison(data_.items, c, selected);
                if (comparison) {
                    // Preserve the existing native four-slot loop for binary identity.
                    // The original fourth record repeats immediately; that cadence
                    // discrepancy remains open for the later accuracy pass.
                    const unsigned step = std::min(unsigned(FF6_ROM_VALUE_SHOP_COMPARISON_TILE_SPAN_0),
                        (ticks_ % (4 * FF6_ROM_VALUE_SHOP_COMPARISON_INTERVAL_0)) /
                            FF6_ROM_VALUE_SHOP_COMPARISON_INTERVAL_0);
                    const unsigned tile =
                        comparison == 1 ? FF6_ROM_VALUE_SHOP_EQUIPPED_ICON_TILE_0
                        : comparison == 4 ? FF6_ROM_VALUE_SHOP_EQUAL_ICON_TILE_0
                                          : FF6_ROM_VALUE_SHOP_COMPARISON_FIRST_TILE_0 - step;
                    std::array<Tile, 2> icon{assets_.sprites.at(tile), Tile{}};
                    if (comparison == 3)
                        for (unsigned row = 0; row < 4; ++row)
                            for (unsigned col = 0; col < 8; ++col)
                                std::swap(icon[0][row * 8 + col], icon[0][(7 - row) * 8 + col]);
                    canvas.actor(icon, assets_.shared.cursor_colors,
                                 x - FF6_ROM_VALUE_SHOP_ACTOR_GRID_1 + FF6_ROM_VALUE_SHOP_COMPARISON_ORIGIN_0,
                                 y - FF6_ROM_VALUE_SHOP_ACTOR_GRID_2 + FF6_ROM_VALUE_SHOP_COMPARISON_ORIGIN_1);
                }
            }
            ++position;
        }
    }
    return canvas.finish();
}
} // namespace ff6
