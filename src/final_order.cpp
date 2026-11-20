#include "ff6/final_order.hpp"
#include "rom-materials/final_order_cursor_anchors.macros.inc"
#include "rom-materials/final_order_row_spacing.macros.inc"
#include "rom-materials/final_order_text_columns.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include <algorithm>

namespace ff6 {
// The source cursor's row-zero baseline and the first-row-minus-spacing
// calculation coalesce in the native compiler. Authenticate that relationship
// rather than retaining a second layout constant in restoration metadata.
static_assert(FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_2 -
                  FF6_ROM_VALUE_FINAL_ORDER_ROW_SPACING_0 ==
              FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_1);
// Select uses native zero-register stores; its identity still comes from the
// shared source sound routine. Move and Cancel share the upper byte.
static_assert((FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U) == 0);
FinalOrderSelection::FinalOrderSelection(const Roster& roster) {
    unsigned count = 0;
    for (unsigned id = 0; id < 16; ++id) {
        const auto& member = roster.membership[id];
        const auto properties = roster.characters[id].properties;
        if (!member.enabled || !member.party || !properties || *properties >= 14) continue;
        if (count == available_.size()) throw std::invalid_argument("Too many final battle participants");
        available_[count++] = static_cast<std::uint8_t>(id);
    }
}
bool FinalOrderSelection::select(unsigned row) {
    if (row >= available_.size()) throw std::invalid_argument("Invalid final battle selection");
    const auto id = available_[row];
    if (!id || std::ranges::find(order_, id) != order_.end()) return false;
    order_[count_++] = id;
    return true;
}
void FinalOrderSelection::undo() { if (count_) order_[--count_].reset(); }
void FinalOrderSelection::reset() { order_.fill({}); count_ = 0; }
void FinalOrderSelection::finish() { for (unsigned row = 0; row < 12; ++row) select(row); }
FinalOrderMenu::FinalOrderMenu(const ContentPack& pack, FieldMenuValues values, Buttons held_buttons)
    : assets_(load_menu_layouts(pack)), values_(std::move(values)), selection_(values_.roster) {
    input_.previous = held_buttons;
}
FieldMenuUpdate FinalOrderMenu::advance(Buttons buttons) {
    sample_input(input_, values_.input, InputContext::menu, buttons, 0);
    FieldMenuUpdate out;
    if (closing_) { if (intensity_) --intensity_; return out; }
    if (intensity_ < 15) { ++intensity_; return out; }
    if (held(input_.repeating, Button::up)) { cursor_ = (cursor_ + 13) % 14; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U); }
    else if (held(input_.repeating, Button::down)) { cursor_ = (cursor_ + 1) % 14; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U); }
    if (held(input_.pressed, Button::b)) { selection_.undo(); out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U); }
    else if (held(input_.pressed, Button::start) || (held(input_.pressed, Button::a) && cursor_ == 13)) {
        selection_.finish(); closing_ = true; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
    } else if (held(input_.pressed, Button::a)) {
        out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        if (!cursor_) { selection_.reset(); cursor_ = 1; }
        else if (selection_.select(cursor_ - 1) && selection_.count() == 12) cursor_ = 13;
    }
    return out;
}
RasterScene FinalOrderMenu::scene() const {
    MenuCanvas canvas(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                      values_.font_color, intensity_);
    canvas.windows({"finalorderwindow"});
    canvas.caption("finalordermsgtext");
    // The original menu compresses sixteen-pixel text rows into twelve-pixel
    // visible rows, shared with party/equipment menus. The source display bands
    // place each glyph top one pixel after its cursor baseline. Cell size is
    // format metadata; selected columns, baselines and spacing come from ROM.
    canvas.text(assets_.captions.at("finalorderresettext").glyphs,
                FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_0 * 8,
                FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_1 + 1);
    canvas.text(assets_.captions.at("finalorderendtext").glyphs,
                FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_1 * 8,
                FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_3 + 1);
    for (unsigned row = 0; row < 12; ++row) {
        const int y = FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_2 + 1 +
                      int(row) * FF6_ROM_VALUE_FINAL_ORDER_ROW_SPACING_0;
        canvas.number(row + 1, 2, FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_4 * 8, y);
        canvas.number(row + 1, 2, FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_5 * 8, y);
        if (const auto id = selection_.available()[row]) {
            const bool selected = std::ranges::find(selection_.order(), id) != selection_.order().end();
            canvas.text(values_.roster.characters[*id].name,
                        FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_2 * 8, y, selected ? 1 : 0);
        }
        if (const auto id = selection_.order()[row])
            canvas.text(values_.roster.characters[*id].name,
                        FF6_ROM_VALUE_FINAL_ORDER_TEXT_COLUMNS_3 * 8, y);
    }
    canvas.cursor(cursor_ == 0 || cursor_ == 13 ? FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_0
                                              : FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_1,
                  cursor_ == 0 ? FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_1
                  : cursor_ == 13 ? FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_3
                  : FF6_ROM_VALUE_FINAL_ORDER_CURSOR_ANCHORS_2 +
                        int(cursor_ - 1) * FF6_ROM_VALUE_FINAL_ORDER_ROW_SPACING_0);
    return canvas.finish();
}
} // namespace ff6
