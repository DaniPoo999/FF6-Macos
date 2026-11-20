#include "ff6/colosseum_menu.hpp"
#include "ff6/field_sprites.hpp"
#include "rom-materials/menu_item_list_rows.macros.inc"
#include "rom-materials/menu_item_list_cursor_x.macros.inc"
#include "rom-materials/menu_item_list_cursor_y.macros.inc"
#include "rom-materials/menu_item_list_row_step.macros.inc"
#include "rom-materials/menu_item_list_columns.macros.inc"
#include "rom-materials/menu_item_list_symbol_x.macros.inc"
#include "rom-materials/menu_item_list_scrollbar.macros.inc"
#include "rom-materials/party_label_palette.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include "rom-materials/menu_item_list_vertical_endpoints.macros.inc"
#include "rom-materials/colosseum_text_columns.macros.inc"
#include "rom-materials/colosseum_text_rows.macros.inc"
#include "rom-materials/colosseum_party_step.macros.inc"
#include "rom-materials/colosseum_actor_x.macros.inc"
#include "rom-materials/colosseum_actor_y_components.macros.inc"
#include "rom-materials/colosseum_cursor_origin.macros.inc"
#include "rom-materials/colosseum_versus_x.macros.inc"
#include "rom-materials/colosseum_versus_y.macros.inc"
#include "rom-materials/menu_item_description_column.macros.inc"
#include "rom-materials/menu_item_description_row.macros.inc"
#include "rom-materials/colosseum_background_rows.macros.inc"
#include "rom-materials/colosseum_background_row_origin.macros.inc"
#include "rom-materials/colosseum_small_monster_column.macros.inc"
#include "rom-materials/colosseum_small_monster_row.macros.inc"
#include "rom-materials/colosseum_ceiling_rows.macros.inc"
#include "rom-materials/party_actor_graphics_alias.macros.inc"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include "rom-materials/colosseum_battle_background.macros.inc"
#include <algorithm>

namespace ff6 {
// The native loop folds its row count and pitch into a pixel endpoint. Validate
// that endpoint against the complete original cursor table, without retaining it.
static_assert(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                  FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0 ==
              FF6_ROM_VALUE_MENU_ITEM_LIST_VERTICAL_ENDPOINTS_0 -
                  FF6_ROM_VALUE_MENU_ITEM_LIST_VERTICAL_ENDPOINTS_1);

// The compiler unrolls these slots. Authenticate every original coordinate,
// including the copies that become separate native immediates.
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_5 * 8 + 1 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_6 * 8);
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_5 == FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_6);
static_assert(FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_0 + 1 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_1);
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_5 * 8 + 2 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_7 * 8);
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_5 == FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_7);
static_assert(FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_0 + 2 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_2);
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_5 * 8 + 3 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_8 * 8);
static_assert(FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_5 == FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_8);
static_assert(FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_0 + 3 * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0 ==
              FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_3);

ColosseumMenu::ColosseumMenu(const ContentPack& pack, FieldMenuValues values,
                             bool shadow_waiting, Buttons held)
    : assets_(load_menu_layouts(pack)), items_(load_menu_items(pack)),
      data_(decode_colosseum_data(pack.get("data_dfb600_colosseum_prop"))), values_(std::move(values)),
      background_(load_battle_background(pack, FF6_ROM_VALUE_COLOSSEUM_BATTLE_BACKGROUND_0)), monsters_(load_battle_monster_graphics_data(pack)),
      shadow_waiting_(shadow_waiting) {
    input_.previous = held;
    auto windows = assets_.shared.windows;
    set_menu_window_gradient(windows, pack.get("menu_colosseum_window_shade_bands"),
                             pack.get("menu_colosseum_window_blend_bands"));
    challenger_rows_ = windows.rows;
    const auto align = pack.get("data_ece800_monster_align");
    if (align.size() != alignment_.size()) throw std::invalid_argument("Invalid Coliseum alignment table");
    std::copy(align.begin(), align.end(), alignment_.begin());
    const auto sprites = load_field_sprites(pack);
    const auto metadata = pack.get("data_cff450_ending_animation");
    const auto animation = pack.get("data_d8e8c8_menu_animation_and_letters");
    const auto vs = slice(animation, 0xec8a - 0xe8c8, 9);
    if (vs[0] != 2) throw std::invalid_argument("Invalid Coliseum versus layout");
    for (unsigned piece = 0; piece < 2; ++piece) {
        const auto tile = vs[3 + piece * 4];
        for (unsigned row = 0; row < 2; ++row) for (unsigned col = 0; col < 2; ++col)
            versus_[piece * 4 + row * 2 + col] = assets_.sprites.at(tile + row * 16 + col);
    }
    const auto palettes = pack.get("interface_palettes_battle_char_pal_pal");
    for (unsigned id = 0; id < actors_.size(); ++id) {
        const auto anim = read16(animation, 0x4f + id * 2) - 0xe8c8U;
        const auto frame = read16(animation, anim) - 0xe8c8U;
        const unsigned palette = ((slice(animation, frame, 9)[4] >> 1) & 7U) - 2;
        if (palette >= 6) throw std::invalid_argument("Invalid Coliseum character palette");
        for (unsigned c = 0; c < 16; ++c) colors_[id][c] = read16(palettes, palette * 32 + c * 2) & 32767U;
        for (unsigned row = 0; row < 3; ++row) {
            const auto offset = read16(metadata, 0x48f + row * 2);
            if (offset % 32) throw std::invalid_argument("Invalid Coliseum sprite tile offset");
            for (unsigned col = 0; col < 2; ++col)
                actors_[id][row * 2 + col] = sprites.tiles.at(sprites.graphics[id == 22 ? FF6_ROM_VALUE_PARTY_ACTOR_GRAPHICS_ALIAS_0 : id] + offset / 32 + col);
        }
    }
}
const Character* ColosseumMenu::character(unsigned position) const {
    for (unsigned id = 0; id < values_.roster.characters.size(); ++id) {
        const auto& m = values_.roster.membership[id];
        if (m.party == values_.roster.active_party && m.position == position && values_.roster.characters[id].properties)
            return &values_.roster.characters[id];
    }
    return nullptr;
}
FieldMenuUpdate ColosseumMenu::advance(Buttons buttons) {
    ++ticks_;
    sample_input(input_, values_.input, InputContext::menu, buttons, 0);
    FieldMenuUpdate out;
    if (page_ == ColosseumPage::closed) { if (intensity_) --intensity_; return out; }
    if (intensity_ < 15) { ++intensity_; return out; }
    const auto before = cursor_;
    if (page_ == ColosseumPage::wager) {
        if (held(input_.repeating, Button::up) && cursor_) --cursor_;
        else if (held(input_.repeating, Button::down) && cursor_ < 255) ++cursor_;
        else if (held(input_.repeating, Button::l))
            cursor_ = cursor_ < FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 ? 0 :
                      cursor_ - FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0;
        else if (held(input_.repeating, Button::r)) cursor_ = std::min(255U, cursor_ + FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0);
        if (cursor_ < scroll_) scroll_ = cursor_;
        if (cursor_ >= scroll_ + FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0)
            scroll_ = cursor_ - (FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 - 1);
        if (held(input_.pressed, Button::b)) {
            selection_.reset(); page_ = ColosseumPage::closed;
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        } else if (held(input_.pressed, Button::a)) {
            const auto& stock = values_.inventory.slots[cursor_];
            if (stock.item == 255 || !stock.quantity) out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
            else {
                selection_ = select_colosseum_wager(data_, values_.inventory, cursor_, shadow_waiting_);
                page_ = ColosseumPage::challenger; cursor_ = 0; intensity_ = 0;
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            }
        }
    } else {
        if (held(input_.repeating, Button::left)) cursor_ = (cursor_ + 3) % 4;
        else if (held(input_.repeating, Button::right)) cursor_ = (cursor_ + 1) % 4;
        // C3/AE0F admits only A after committing the wager selection.
        if (held(input_.pressed, Button::a)) {
            if (!character(cursor_)) out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
            else {
                select_colosseum_challenger(*selection_, values_.roster, cursor_);
                page_ = ColosseumPage::closed;
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            }
        }
    }
    if (cursor_ != before) out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
    return out;
}
RasterScene ColosseumMenu::scene() const {
    MenuCanvas c(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]}, values_.font_color, intensity_);
    if (!selection_) {
        c.windows({"colosseumitemtitlewindow", "colosseumitemmsgwindow", "colosseumitemdescwindow", "colosseumitemlistwindow"});
        c.caption("colosseumitemtitletext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0);
        c.caption("colosseumitemmsgtext");
        const auto selected = values_.inventory.slots[cursor_].item;
        if (selected != 255) c.description(assets_.item_descriptions[selected],
            FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_COLUMN_0 * 8, FF6_ROM_VALUE_MENU_ITEM_DESCRIPTION_ROW_0 * 8 - 1);
        // C3/7D2B and C3/7F88 share the item-list layout with this wager screen.
        for (unsigned row = 0; row < FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0 && scroll_ + row < 256; ++row) {
            const auto stock = values_.inventory.slots[scroll_ + row];
            if (stock.item == 255) continue;
            const int y = FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 - 1 +
                          int(row) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0;
            c.inventory_name(stock.item, FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_0 * 8, y);
            c.number(stock.quantity, 2, FF6_ROM_VALUE_MENU_ITEM_LIST_COLUMNS_1 * 8, y);
            const auto icon = items_.items[stock.item].icon;
            if (icon >= 0xd8 && icon <= 0xe7)
                c.text(assets_.item_symbols[icon - 0xd8], FF6_ROM_VALUE_MENU_ITEM_LIST_SYMBOL_X_0, y);
        }
        c.cursor(FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_X_0, FF6_ROM_VALUE_MENU_ITEM_LIST_CURSOR_Y_0 +
                 int(cursor_ - scroll_) * FF6_ROM_VALUE_MENU_ITEM_LIST_ROW_STEP_0);
        c.scroll_indicator(scroll_, 256 - FF6_ROM_VALUE_MENU_ITEM_LIST_ROWS_0,
                           FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_0, FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_1,
                           FF6_ROM_VALUE_MENU_ITEM_LIST_SCROLLBAR_2, ticks_);
        return c.finish();
    }
    const auto& selected = *selection_;
    c.windows({"colosseumprizewindow", "colosseumwagerwindow", "colosseumcharwindow"});
    c.caption("colosseumcharmsgtext");
    c.text(assets_.item_names[selected.wager], FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_0 * 8,
           FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_0 * 8 - 1);
    if (data_[selected.wager].hidden_prize) c.caption("colosseumunknownprizetext");
    else if (selected.prize != 255) c.text(assets_.item_names[selected.prize],
        FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_1 * 8, FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_1 * 8 - 1);
    const auto actor = [&](const Character& who, int x, int y) {
        const auto gfx = (who.status & 32U) ? unsigned{FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0} : unsigned{who.graphics};
        if (gfx >= actors_.size()) throw std::invalid_argument("Unsupported Coliseum character graphic");
        c.actor(actors_[gfx], colors_[gfx], x, y + FF6_ROM_VALUE_COLOSSEUM_ACTOR_Y_COMPONENTS_3);
    };
    for (unsigned pos = 0; pos < 4; ++pos) if (const auto* who = character(pos)) {
        c.text(who->name, FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_5 * 8 + int(pos) * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0,
               FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_5 * 8 - 1);
        actor(*who, FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_0 + int(pos) * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0,
              FF6_ROM_VALUE_COLOSSEUM_ACTOR_Y_COMPONENTS_0);
    }
    if (const auto* who = character(cursor_)) {
        c.text(who->name, FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_3 * 8, FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_3 * 8 - 1);
        actor(*who, FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_4, FF6_ROM_VALUE_COLOSSEUM_ACTOR_Y_COMPONENTS_1);
    }
    c.text(assets_.monster_names[selected.monster], FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_2 * 8,
           FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_2 * 8 - 1);
    if (selected.shadow) {
        const auto it = std::ranges::find(values_.roster.characters, std::optional<std::uint8_t>{3}, &Character::properties);
        if (it == values_.roster.characters.end()) throw std::invalid_argument("Coliseum Shadow lacks roster identity");
        c.text(it->name, FF6_ROM_VALUE_COLOSSEUM_TEXT_COLUMNS_4 * 8, FF6_ROM_VALUE_COLOSSEUM_TEXT_ROWS_4 * 8 - 1);
        // DrawColosseumShadow flips each sixteen-pixel-wide actor piece.
        auto facing = actors_[3];
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 2; ++col)
                for (unsigned y = 0; y < 8; ++y)
                    for (unsigned x = 0; x < 8; ++x)
                        facing[row * 2 + col][y * 8 + x] = actors_[3][row * 2 + 1 - col][y * 8 + 7 - x];
        c.actor(facing, colors_[3], FF6_ROM_VALUE_COLOSSEUM_ACTOR_X_5,
                FF6_ROM_VALUE_COLOSSEUM_ACTOR_Y_COMPONENTS_2 + FF6_ROM_VALUE_COLOSSEUM_ACTOR_Y_COMPONENTS_3);
    }
    c.actor(std::span{versus_}.first(4), assets_.shared.cursor_colors,
            FF6_ROM_VALUE_COLOSSEUM_VERSUS_X_0 + FF6_ROM_VALUE_COLOSSEUM_VERSUS_X_1, FF6_ROM_VALUE_COLOSSEUM_VERSUS_Y_0);
    c.actor(std::span{versus_}.subspan(4), assets_.shared.cursor_colors,
            FF6_ROM_VALUE_COLOSSEUM_VERSUS_X_0 + FF6_ROM_VALUE_COLOSSEUM_VERSUS_X_2, FF6_ROM_VALUE_COLOSSEUM_VERSUS_Y_1);
    c.cursor(FF6_ROM_VALUE_COLOSSEUM_CURSOR_ORIGIN_0 + int(cursor_) * FF6_ROM_VALUE_COLOSSEUM_PARTY_STEP_0,
             FF6_ROM_VALUE_COLOSSEUM_CURSOR_ORIGIN_1);
    auto result = c.finish();
    result.layers.front().lines.assign(challenger_rows_.begin(), challenger_rows_.end());
    // FixColosseumBGTiles supplies the copied rows and their destination.
    // Windows share the same layer and extracted gradient as the arena.
    auto background = make_battle_background_layer(background_, {0, 0, 256, FF6_ROM_VALUE_COLOSSEUM_BACKGROUND_ROWS_0 * 8});
    background.lines.assign(challenger_rows_.begin(), challenger_rows_.end());
    for (auto& line : background.lines) line.offset_y = -(FF6_ROM_VALUE_COLOSSEUM_BACKGROUND_ROW_ORIGIN_0 * 8 - 1);
    result.layers.insert(result.layers.begin(), std::move(background));
    if (!selected.shadow) {
        const auto& graphic = monsters_.records[selected.monster];
        const unsigned extent = graphic.stencil_extent;
        // C3/B033 aligns the full stencil, retaining trailing blank rows and
        // pairs of blank right-hand columns before the alignment adjustment.
        unsigned blank_rows = extent - graphic.dimensions.height, right = 0; std::uint16_t occupied = 0;
        for (unsigned row = 0; row < extent; ++row) occupied |= graphic.stencil[row];
        const unsigned byte = extent == 16 ? occupied & 255U : occupied >> 8U;
        if (byte) while (right < 8 && !(byte & (1U << right))) ++right;
        int y = int(blank_rows);
        switch (alignment_[selected.monster]) {
        case 0: y = -FF6_ROM_VALUE_COLOSSEUM_CEILING_ROWS_0; break;
        case 1: break;
        case 2: y = std::max(0, y - 1); break;
        case 3: ++y; break;
        case 4: y = std::max(0, y - 3); break;
        default: throw std::invalid_argument("Invalid Coliseum monster alignment");
        }
        // The menu loads monsters into a tile layer, including 128-pixel
        // stencils. They are not restricted to an actor sprite's dimensions.
        TileLayer layer;
        layer.columns = layer.rows = static_cast<std::uint16_t>(extent);
        layer.wrap_x = layer.wrap_y = false;
        layer.palettes.push_back(graphic.colors);
        layer.tiles.emplace_back();
        layer.cells.resize(extent * extent, {0, 0, 8 * 256});
        layer.lines.resize(result.height);
        for (auto& line : layer.lines) {
            line.offset_x = -((extent == 8 ? FF6_ROM_VALUE_COLOSSEUM_SMALL_MONSTER_COLUMN_0 * 8 : 0) + int(right / 2) * 8);
            line.offset_y = 1 - ((extent == 8 ? FF6_ROM_VALUE_COLOSSEUM_SMALL_MONSTER_ROW_0 * 8 : 0) + y * 8);
        }
        unsigned tile = 0;
        for (unsigned row = 0; row < graphic.dimensions.height; ++row) for (unsigned col = 0; col < extent; ++col)
            if (graphic.stencil[row] & (0x8000U >> col)) {
                layer.cells[row * extent + col].tile = static_cast<std::uint32_t>(layer.tiles.size());
                layer.tiles.push_back(graphic.packed_tiles.at(tile++));
            }
        result.layers.insert(result.layers.begin() + 1, std::move(layer));
    }
    return result;
}
} // namespace ff6
