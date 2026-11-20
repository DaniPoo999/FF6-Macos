#include "ff6/party_selection.hpp"
#include "rom-materials/menu_imp_portrait.macros.inc"
#include "rom-materials/party_cursor_x_origin.macros.inc"
#include "rom-materials/party_reserve_y_origin.macros.inc"
#include "rom-materials/party_group_y_origin.macros.inc"
#include "rom-materials/party_reserve_step.macros.inc"
#include "rom-materials/party_group_column_step.macros.inc"
#include "rom-materials/party_group_step.macros.inc"
#include "rom-materials/party_actor_x_offset.macros.inc"
#include "rom-materials/party_actor_graphics_alias.macros.inc"
#include "rom-materials/party_error_hold.macros.inc"
#include "rom-materials/party_portrait_x.macros.inc"
#include "rom-materials/party_portrait_y.macros.inc"
#include "rom-materials/party_status_x.macros.inc"
#include "rom-materials/party_status_y.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include "rom-materials/party_group_bottom_y.macros.inc"
#include "rom-materials/party_actor_vertical_endpoints.macros.inc"
#include "rom-materials/party_arrow_vertical_endpoints.macros.inc"
#include "rom-materials/party_text_positions.macros.inc"
#include "rom-materials/party_caption_scroll.macros.inc"
#include "rom-materials/party_label_palette.macros.inc"
#include "rom-materials/battle_character_vital_caps.macros.inc"
#include <algorithm>

namespace ff6 {
// Compiler-folded lower-row coordinates have their own authenticated ROM selection.
static_assert(FF6_ROM_VALUE_PARTY_GROUP_Y_ORIGIN_0 + FF6_ROM_VALUE_PARTY_RESERVE_STEP_0 ==
              FF6_ROM_VALUE_PARTY_GROUP_BOTTOM_Y_0);
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
}
PartySelection::PartySelection(Roster roster, unsigned parties, bool clear, std::uint16_t forced)
    : roster_(std::move(roster)), parties_(parties), forced_(forced) {
    require(parties >= 1 && parties <= 3, "Party selection requires one to three groups");
    unsigned reserve = 0;
    for (unsigned id = 0; id < 16; ++id) {
        auto& member = roster_.membership[id];
        if (clear) member.party = 0;
        require(member.position < 4 && member.party < 8, "Invalid party membership");
        if (member.enabled && member.party == 0) reserve_[reserve++] = static_cast<std::uint8_t>(id);
    }
}
std::optional<std::uint8_t> PartySelection::character(unsigned cell) const {
    require(cell < 16 + parties_ * 4, "Invalid party selection cell");
    if (cell < 16) return reserve_[cell];
    const unsigned group = (cell - 16) / 4 + 1, position = (cell - 16) % 4;
    for (unsigned id = 0; id < 16; ++id) {
        const auto& member = roster_.membership[id];
        if (member.enabled && member.party == group && member.position == position)
            return static_cast<std::uint8_t>(id);
    }
    return {};
}
bool PartySelection::locked(unsigned cell) const {
    const auto id = character(cell);
    return id && (forced_ & (1U << *id));
}
bool PartySelection::swap(unsigned first, unsigned second) {
    if (locked(first) || locked(second)) return false;
    const auto a = character(first), b = character(second);
    const auto place = [&](std::optional<std::uint8_t> id, unsigned cell) {
        if (cell < 16) reserve_[cell] = id;
        if (id) {
            auto& member = roster_.membership[*id];
            member.party = cell < 16 ? 0 : static_cast<std::uint8_t>((cell - 16) / 4 + 1);
            member.position = cell < 16 ? 0 : static_cast<std::uint8_t>((cell - 16) % 4);
            // Back-row and enabled/visible state belong to the character,
            // not the destination cell (C3/72F8).
        }
    };
    place(a, second);
    place(b, first);
    return true;
}
bool PartySelection::ready() const {
    for (unsigned group = 0; group < parties_; ++group) {
        bool occupied = false;
        for (unsigned pos = 0; pos < 4; ++pos) occupied |= character(16 + group * 4 + pos).has_value();
        if (!occupied) return false;
    }
    return true;
}
void PartySelection::update_characters(const Roster& edited) {
    roster_.characters = edited.characters;
    roster_.abilities = edited.abilities;
}
PartySelectionMenu::PartySelectionMenu(const ContentPack& pack, FieldMenuValues values,
                                     unsigned parties, bool clear, std::uint16_t forced,
                                     Buttons buttons)
    : assets_(load_menu_layouts(pack)), sprites_(load_field_sprites(pack)),
      values_(std::move(values)), selection_(values_.roster, parties, clear, forced) {
    input_.previous = buttons;
    const auto metadata = pack.get("data_cff450_ending_animation");
    const auto animation = pack.get("data_d8e8c8_menu_animation_and_letters");
    const auto palettes = pack.get("interface_palettes_battle_char_pal_pal");
    for (unsigned id = 0; id < 23; ++id) {
        const auto anim = read16(animation, 0x4f + id * 2) - 0xe8c8U;
        const auto frame = read16(animation, anim) - 0xe8c8U;
        const unsigned palette = ((slice(animation, frame, 9)[4] >> 1) & 7U) - 2;
        require(palette < 6, "Invalid party character palette");
        for (unsigned c = 0; c < 16; ++c)
            colors_[id][c] = read16(palettes, palette * 32 + c * 2) & 32767U;
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = read16(metadata, 0x48f + row * 2);
            require(offset % 32 == 0, "Invalid party sprite tile offset");
            for (unsigned col = 0; col < 2; ++col)
                actors_[id][row * 2 + col] =
                    sprites_.tiles.at(sprites_.graphics[id == 22 ? FF6_ROM_VALUE_PARTY_ACTOR_GRAPHICS_ALIAS_0 : id] + offset / 32 + col);
        }
    }
}
std::array<int, 2> PartySelectionMenu::cursor_position(unsigned cell) const {
    if (cell < 16) return {FF6_ROM_VALUE_PARTY_CURSOR_X_ORIGIN_0 + int(cell % 8) * FF6_ROM_VALUE_PARTY_RESERVE_STEP_0,
                           FF6_ROM_VALUE_PARTY_RESERVE_Y_ORIGIN_0 + int(cell / 8) * FF6_ROM_VALUE_PARTY_RESERVE_STEP_0};
    const unsigned p = cell - 16;
    return {FF6_ROM_VALUE_PARTY_CURSOR_X_ORIGIN_0 + int(p / 4) * FF6_ROM_VALUE_PARTY_GROUP_STEP_0 +
                int(p % 4 / 2) * FF6_ROM_VALUE_PARTY_GROUP_COLUMN_STEP_0,
            FF6_ROM_VALUE_PARTY_GROUP_Y_ORIGIN_0 + int(p % 2) * FF6_ROM_VALUE_PARTY_RESERVE_STEP_0};
}
void PartySelectionMenu::move(Buttons repeat) {
    if (cursor_ < 16) {
        unsigned x = cursor_ % 8, y = cursor_ / 8;
        if (held(repeat, Button::down) && y == 1) {
            // Original upper/lower cursor maps preserve horizontal proximity.
            const unsigned column = selection_.parties() == 3
                ? std::array<unsigned, 8>{
#include "rom-materials/party_cursor_down_columns.inc"
}[x]
                : std::min(x, selection_.parties() * 2 - 1);
            cursor_ = 16 + column / 2 * 4 + column % 2 * 2;
            return;
        }
        if (held(repeat, Button::up)) y = 0;
        else if (held(repeat, Button::down)) y = 1;
        else if (held(repeat, Button::left) && x) --x;
        else if (held(repeat, Button::right) && x < 7) ++x;
        cursor_ = x + y * 8;
    } else {
        const unsigned cell = cursor_ - 16;
        unsigned x = cell / 4 * 2 + cell % 4 / 2, y = cell % 2;
        if (held(repeat, Button::up) && y == 0) {
            const unsigned column = std::array<unsigned, 6>{
#include "rom-materials/party_cursor_up_columns.inc"
}[x];
            cursor_ = 8 + column;
            return;
        }
        if (held(repeat, Button::up)) y = 0;
        else if (held(repeat, Button::down)) y = 1;
        else if (held(repeat, Button::left) && x) --x;
        else if (held(repeat, Button::right) && x + 1 < selection_.parties() * 2) ++x;
        cursor_ = 16 + x / 2 * 4 + x % 2 * 2 + y;
    }
}
FieldMenuUpdate PartySelectionMenu::advance(const ContentPack& pack, Buttons buttons) {
    ++clock_;
    if (status_) {
        auto out = status_->advance(buttons);
        if (status_->complete()) {
            selection_.update_characters(status_->values().roster);
            status_.reset();
            picked_.reset();
            intensity_ = 0;
            input_.previous = buttons;
        }
        return out;
    }
    sample_input(input_, values_.input, InputContext::menu, buttons, 0);
    FieldMenuUpdate out;
    if (closing_) { if (intensity_) --intensity_; return out; }
    if (intensity_ < 15) { ++intensity_; return out; }
    if (error_) { --error_; return out; }
    const auto old = cursor_;
    move(input_.repeating);
    if (cursor_ != old) { out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8); return out; }
    if (held(input_.pressed, Button::a)) {
        if (selection_.locked(cursor_)) out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
        else if (!picked_) { picked_ = cursor_; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U); }
        else if (*picked_ == cursor_) {
            if (const auto id = selection_.character(cursor_)) {
                auto values = values_; values.roster = selection_.roster();
                status_.emplace(pack, std::move(values), buttons, *id);
                out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            } else out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
        } else {
            selection_.swap(*picked_, cursor_);
            picked_.reset();
            out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        }
    } else if (held(input_.pressed, Button::b) && picked_) {
        picked_.reset(); out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8);
    } else if (!picked_ && (held(input_.pressed, Button::b) || held(input_.pressed, Button::start))) {
        if (selection_.ready()) { closing_ = true; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8); }
        else { error_ = FF6_ROM_VALUE_PARTY_ERROR_HOLD_0; out.sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0); }
    }
    return out;
}
RasterScene PartySelectionMenu::scene() {
    if (status_) return status_->scene();
    MenuCanvas c(assets_, {values_.window_skin, values_.window_colors[values_.window_skin]},
                 values_.font_color, intensity_);
    // DrawWindow overwrites cells on one surface. Separate equal-depth layers
    // would leave the outer lower window in front of the individual groups.
    constexpr std::array<std::string_view, 8> windows{
        "partytopwindow", "partybtmwindow", "partymidwindow", "partymsgwindow",
        "partytitlewindow", "party1window", "party2window", "party3window"};
    c.windows(std::span{windows}.first(5 + selection_.parties()));
    c.caption("partytitletext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_0);
    c.caption(error_ ? (selection_.parties() == 1 ? "partyerrormsg2text" : "partyerrormsg1text")
                     : "partymsgtext", 0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_0);
    if (!error_ || selection_.parties() > 1)
        c.number(selection_.parties(), 1, error_ ? FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_2 :
                     FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_0, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_1);
    static_assert(FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_1 == FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_3);
    const auto id = selection_.character(cursor_);
    if (id) {
        const auto& ch = selection_.roster().characters[*id];
        const unsigned portrait = ch.status & 32U ? FF6_ROM_VALUE_MENU_IMP_PORTRAIT_0 : ch.properties == 1 ? 1 : ch.graphics;
        c.portrait(static_cast<std::uint8_t>(portrait),
                   FF6_ROM_VALUE_PARTY_PORTRAIT_X_0, FF6_ROM_VALUE_PARTY_PORTRAIT_Y_0);
        c.status(ch.status, ch.persistent_effects,
                 FF6_ROM_VALUE_PARTY_STATUS_X_0, FF6_ROM_VALUE_PARTY_STATUS_Y_0);
        c.text(ch.name, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_4, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_5);
        if (ch.esper) c.text(assets_.esper_names.at(*ch.esper),
                             FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_6, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_7);
        c.caption("partyleveltext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_1);
        c.caption("partyhptext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_2);
        c.caption("partymptext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_3);
        c.caption("partyhpslashtext", 0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_2);
        c.caption("partympslashtext", 0, 0, -FF6_ROM_VALUE_PARTY_CAPTION_SCROLL_3);
        c.number(ch.level, 2, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_8, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_9);
        c.number(ch.hp.current, 4, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_10, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_11);
        c.number(maximum_vital(ch.hp, FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_0), 4, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_12, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_13);
        c.number(ch.mp.current, 4, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_14, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_15);
        c.number(maximum_vital(ch.mp, FF6_ROM_VALUE_BATTLE_CHARACTER_VITAL_CAPS_1), 4, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_16, FF6_ROM_VALUE_PARTY_TEXT_POSITIONS_17);
    }
    for (unsigned cell = 0; cell < 16 + selection_.parties() * 4; ++cell)
        if (const auto character = selection_.character(cell)) {
            const auto& ch = selection_.roster().characters[*character];
            require(ch.properties && *ch.properties < actors_.size(), "Invalid party character sprite");
            const auto pos = cursor_position(cell);
            c.actor(actors_[*ch.properties], colors_[*ch.properties], pos[0] + FF6_ROM_VALUE_PARTY_ACTOR_X_OFFSET_0,
                    pos[1] - (FF6_ROM_VALUE_PARTY_ACTOR_VERTICAL_ENDPOINTS_0 -
                              FF6_ROM_VALUE_PARTY_ACTOR_VERTICAL_ENDPOINTS_1));
            if (selection_.locked(cell))
                c.page_arrow(true, pos[0] + FF6_ROM_VALUE_PARTY_ACTOR_X_OFFSET_0,
                             pos[1] + (FF6_ROM_VALUE_PARTY_ARROW_VERTICAL_ENDPOINTS_0 -
                                       FF6_ROM_VALUE_PARTY_ARROW_VERTICAL_ENDPOINTS_1));
        }
    c.cursor(cursor_position(cursor_)[0], cursor_position(cursor_)[1]);
    if (picked_ && (clock_ & 16U))
        c.cursor(cursor_position(*picked_)[0], cursor_position(*picked_)[1]);
    return c.finish();
}
} // namespace ff6
