#include "ff6/save_menu.hpp"
#include "ff6/field_sprites.hpp"
#include "ff6/party_menu.hpp"
#include "rom-materials/save_menu_text_columns.macros.inc"
#include "rom-materials/save_menu_text_rows.macros.inc"
#include "rom-materials/save_menu_name_columns.macros.inc"
#include "rom-materials/save_menu_row_step.macros.inc"
#include "rom-materials/save_menu_actor_x.macros.inc"
#include "rom-materials/save_menu_slot_cursor.macros.inc"
#include "rom-materials/save_menu_confirm_cursor.macros.inc"
#include "rom-materials/save_menu_confirm_step.macros.inc"
#include "rom-materials/save_menu_gray_palette_offset.macros.inc"
#include "rom-materials/party_actor_graphics_alias.macros.inc"
#include "rom-materials/party_label_palette.macros.inc"
#include "rom-materials/menu_navigation_sound_choices.macros.inc"
#include "rom-materials/menu_navigation_sound_bound.macros.inc"
#include "rom-materials/save_menu_actor_y_difference.macros.inc"
#include "rom-materials/save_menu_load_cursor_bias.macros.inc"
#include "rom-materials/save_confirm_clip_column.macros.inc"
#include "rom-materials/save_confirm_clip_row.macros.inc"
#include <algorithm>
namespace ff6 {
// C3/171D..188D and C3/197F provide the primary row, repeated slot
// spacing and actor anchors. The full source proof checks every repeated slot,
// matching name/level rows, matching HP rows and all actor frame offsets.
// Emit only independently compiled values; derive folded relative coordinates.
static_assert(FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_0 == FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_1);
static_assert(FF6_ROM_VALUE_SAVE_MENU_LOAD_CURSOR_BIAS_0 ==
              FF6_ROM_VALUE_SAVE_MENU_SLOT_CURSOR_2 - FF6_ROM_VALUE_SAVE_MENU_ROW_STEP_0);
namespace {
Bytes interface_text(std::string_view text) {
    Bytes out;
    for (char c : text)
        out.push_back(c == ' '               ? 255
                      : c >= 'A' && c <= 'Z' ? static_cast<std::uint8_t>(128 + c - 'A')
                      : c >= 'a' && c <= 'z' ? static_cast<std::uint8_t>(154 + c - 'a')
                                             : 255);
    return out;
}
} // namespace
SaveMenu::SaveMenu(const ContentPack& pack, SaveMenuMode mode, std::array<SaveSlot, 3> slots,
                   MenuWindowAppearance appearance, InputSettings settings, Buttons held)
    : assets_(load_menu_layouts(pack)), mode_(mode), slots_(std::move(slots)),
      appearance_(appearance), settings_(settings) {
    input_.previous = held;
    const auto sprites = load_field_sprites(pack);
    const auto metadata = pack.get("data_cff450_ending_animation");
    const auto animation = pack.get("data_d8e8c8_menu_animation_and_letters");
    const auto palettes = pack.get("interface_palettes_battle_char_pal_pal");
    const auto gray = pack.get("interface_palettes_menu_pal_pal").subspan(FF6_ROM_VALUE_SAVE_MENU_GRAY_PALETTE_OFFSET_0, 32);
    for (unsigned c = 0; c < 16; ++c)
        gray_[c] = read16(gray, c * 2) & 32767U;
    for (unsigned i = 0; i < 23; ++i) {
        const auto anim = read16(animation, 0x4f + i * 2) - 0xe8c8U;
        const auto frame = read16(animation, anim) - 0xe8c8U;
        if (slice(animation, frame, 9)[0] != 2)
            throw std::invalid_argument("Invalid save character drawing");
        const auto palette = ((animation[frame + 4] >> 1) & 7U) - 2;
        if (palette >= 6)
            throw std::invalid_argument("Invalid save character palette");
        for (unsigned c = 0; c < 16; ++c)
            actor_colors_[i][c] = read16(palettes, palette * 32 + c * 2) & 32767U;
        if (i == 22)
            continue; // Brown soldier shares character 14's image.
        for (unsigned row = 0; row < 3; ++row) {
            const unsigned offset = read16(metadata, 0x48f + row * 2);
            if (offset % 32)
                throw std::invalid_argument("Invalid menu character pose");
            for (unsigned col = 0; col < 2; ++col)
                actors_[i][row * 2 + col] =
                    sprites.tiles.at(sprites.graphics[i] + offset / 32 + col);
        }
    }
    if (mode_ == SaveMenuMode::load)
        for (unsigned i = 0; i < 3; ++i)
            if (slots_[i].game) {
                cursor_ = i + 1;
                if (slots_[i].recovered)
                    notice_ = "Recovered previous save after a missing or damaged current file";
                break;
            }
}
std::vector<std::uint8_t> SaveMenu::advance(Buttons buttons) {
    sample_input(input_, settings_, InputContext::menu, buttons, 0);
    std::vector<std::uint8_t> sounds;
    if (invalid_)
        --invalid_;
    if (phase_ == SaveMenuPhase::leaving) {
        intensity_ = static_cast<std::uint8_t>(intensity_ >= 2 ? intensity_ - 2 : 0);
        if (!intensity_)
            phase_ = SaveMenuPhase::complete;
        return sounds;
    }
    if (phase_ == SaveMenuPhase::complete || phase_ == SaveMenuPhase::request)
        return sounds;
    if (intensity_ < 15) {
        intensity_ = static_cast<std::uint8_t>(std::min(15U, unsigned{intensity_} + 2));
        return sounds;
    }
    const auto press = input_.pressed, repeat = input_.repeating;
    if (phase_ == SaveMenuPhase::confirm) {
        if (held(repeat, Button::up) || held(repeat, Button::down)) {
            answer_ = 1 - answer_;
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        }
        if (held(press, Button::b) || (held(press, Button::a) && answer_)) {
            phase_ = SaveMenuPhase::slots;
            choice_.reset();
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        } else if (held(press, Button::a)) {
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
            phase_ = SaveMenuPhase::request;
        }
        return sounds;
    }
    const unsigned count = mode_ == SaveMenuMode::load ? 4 : 3;
    if (held(repeat, Button::up) || held(repeat, Button::down)) {
        cursor_ = (cursor_ + (held(repeat, Button::up) ? count - 1 : 1)) % count;
        sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        notice_.clear();
        if (mode_ == SaveMenuMode::load && cursor_ && slots_[cursor_ - 1].recovered)
            notice_ = "Recovered previous save after a missing or damaged current file";
    }
    if (held(press, Button::b) && mode_ == SaveMenuMode::save) {
        sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 >> 8U);
        phase_ = SaveMenuPhase::leaving;
        return sounds;
    }
    if (held(press, Button::a)) {
        if (mode_ == SaveMenuMode::load && cursor_ == 0) {
            choice_ = SaveMenuChoice{true, 0};
            phase_ = SaveMenuPhase::request;
            sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
        } else {
            const unsigned slot = cursor_ - (mode_ == SaveMenuMode::load ? 1 : 0);
            if (mode_ == SaveMenuMode::load && !slots_[slot].game) {
                sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_BOUND_0);
                invalid_ = 8;
                notice_ = slots_[slot].error;
            } else {
                choice_ = SaveMenuChoice{false, slot};
                answer_ = 0;
                sounds.push_back(FF6_ROM_VALUE_MENU_NAVIGATION_SOUND_CHOICES_0 & 255U);
                phase_ = slots_[slot].game ? SaveMenuPhase::confirm : SaveMenuPhase::request;
            }
        }
    }
    return sounds;
}
void SaveMenu::finish_request(std::string error) {
    if (phase_ != SaveMenuPhase::request)
        throw std::logic_error("Save menu has no pending request");
    notice_ = std::move(error);
    if (notice_.empty())
        phase_ = SaveMenuPhase::leaving;
    else {
        phase_ = SaveMenuPhase::slots;
        choice_.reset();
        invalid_ = 8;
    }
}
RasterScene SaveMenu::scene() const {
    if (phase_ == SaveMenuPhase::confirm) {
        const auto& saved = slots_[choice_->slot].game.value();
        auto roster = saved.roster;
        MainMenuView view;
        populate_party_menu(roster, view);
        view.window = {saved.skin, saved.colors[saved.skin]};
        view.font_color = 32767;
        view.steps = saved.steps;
        view.gil = saved.inventory.gil;
        view.hours = saved.menu.time.hours;
        view.minutes = saved.menu.time.minutes;
        view.intensity = intensity_;
        view.sidebar_cursor_visible = false;
        auto result = make_main_menu_scene(assets_.shared, view);
        const auto& windows = assets_.shared.windows;
        // Resource identities follow the four draw calls at C3/3128..3189.
        // Their window records are extracted, not embedded layout data.
        const std::array placements{windows.placements[3], windows.placements[4],
                                    windows.placements[1], windows.placements[2]};
        result.layers[1] = make_menu_windows(windows, placements, view.window);
        // The native exclusion ends at the time window and starts at the
        // confirmation window. Both boundaries come from their ROM records.
        for (const unsigned layer : {0U, 2U})
            for (unsigned y = 0; y < FF6_ROM_VALUE_SAVE_CONFIRM_CLIP_ROW_0 * 8; ++y)
                result.layers[layer].lines[y].right = FF6_ROM_VALUE_SAVE_CONFIRM_CLIP_COLUMN_0 * 8;
        MenuCanvas overlay(assets_, view.window, 32767, intensity_);
        overlay.caption("gameloadyestext");
        overlay.caption("gameloadnotext");
        if (mode_ == SaveMenuMode::load) {
            overlay.caption("gameloadmsgtext1");
            overlay.caption("gameloadmsgtext2");
        } else {
            overlay.caption("gamesavemsgtext1");
            overlay.caption("gamesavemsgtext2");
            overlay.caption("gamesavemsgtext3");
        }
        overlay.cursor(FF6_ROM_VALUE_SAVE_MENU_CONFIRM_CURSOR_0, FF6_ROM_VALUE_SAVE_MENU_CONFIRM_CURSOR_1 +
                       int(answer_) * FF6_ROM_VALUE_SAVE_MENU_CONFIRM_STEP_0);
        auto image = overlay.finish();
        result.layers.insert(result.layers.end(), image.layers.begin(), image.layers.end());
        result.objects = std::move(image.objects);
        return result;
    }
    MenuCanvas canvas(assets_, appearance_, 32767, intensity_);
    canvas.windows({"savetitlewindow"});
    canvas.caption(mode_ == SaveMenuMode::load ? "newgametext" : "savetitletext");
    auto result = canvas.finish();
    for (unsigned i = 0; i < 3; ++i) {
        const auto& slot = slots_[i];
        const auto appearance =
            slot.game ? MenuWindowAppearance{slot.game->skin, slot.game->colors[slot.game->skin]}
                      : appearance_;
        MenuCanvas row(assets_, appearance, 32767, intensity_);
        const auto prefix = "saveslot" + std::to_string(i + 1);
        row.windows({prefix + "window"});
        if (!slot.game)
            row.caption(prefix + "emptytext");
        else {
            const auto& saved = *slot.game;
            const auto party = current_menu_party(saved.roster);
            const auto who =
                std::find_if(party.begin(), party.end(), [](auto id) { return id.has_value(); });
            const auto& c = saved.roster.characters[**who];
            // C3/171D..188D and C3/197F share the three-slot layout.
            const int dy = int(i) * FF6_ROM_VALUE_SAVE_MENU_ROW_STEP_0;
            row.text(c.name, FF6_ROM_VALUE_SAVE_MENU_NAME_COLUMNS_0 * 8,
                     FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_2 * 8 - 1 + dy);
            row.caption(prefix + "timetext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0);
            row.caption(prefix + "timecolontext");
            row.caption(prefix + "leveltext", FF6_ROM_VALUE_PARTY_LABEL_PALETTE_0);
            row.caption(prefix + "hpslashtext");
            row.number(saved.menu.time.hours, 2, FF6_ROM_VALUE_SAVE_MENU_TEXT_COLUMNS_0 * 8,
                       FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_0 * 8 - 1 + dy);
            row.text(menu_number(saved.menu.time.minutes, 3, 2, true),
                     FF6_ROM_VALUE_SAVE_MENU_TEXT_COLUMNS_1 * 8, FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_1 * 8 - 1 + dy);
            row.number(c.level, 2, FF6_ROM_VALUE_SAVE_MENU_TEXT_COLUMNS_2 * 8,
                       FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_2 * 8 - 1 + dy);
            row.number(c.hp.current, 4, FF6_ROM_VALUE_SAVE_MENU_TEXT_COLUMNS_3 * 8,
                       FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_3 * 8 - 1 + dy);
            row.number(maximum_vital(c.hp, 9999), 4, FF6_ROM_VALUE_SAVE_MENU_TEXT_COLUMNS_4 * 8,
                       FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_3 * 8 - 1 + dy);
            for (unsigned j = 0; j < 4; ++j)
                if (party[j]) {
                    auto gfx = saved.roster.characters[*party[j]].graphics;
                    if (gfx == 14 && saved.flags.story[0x103])
                        gfx = 22;
                    if (gfx >= 23)
                        throw std::invalid_argument("Save menu character image is unavailable");
                    row.actor(actors_[gfx == 22 ? FF6_ROM_VALUE_PARTY_ACTOR_GRAPHICS_ALIAS_0 : gfx],
                              cursor_ == i + (mode_ == SaveMenuMode::load ? 1U : 0U)
                                  ? actor_colors_[gfx]
                                  : gray_,
                              FF6_ROM_VALUE_SAVE_MENU_ACTOR_X_0 + int(j) * (FF6_ROM_VALUE_SAVE_MENU_ACTOR_X_1 - FF6_ROM_VALUE_SAVE_MENU_ACTOR_X_0),
                              (FF6_ROM_VALUE_SAVE_MENU_TEXT_ROWS_0 * 8 - FF6_ROM_VALUE_SAVE_MENU_ACTOR_Y_DIFFERENCE_0) + dy);
                }
        }
        auto image = row.finish();
        result.layers.insert(result.layers.end(), image.layers.begin(), image.layers.end());
        result.objects->sprites.insert(result.objects->sprites.end(),
                                       image.objects->sprites.begin(),
                                       image.objects->sprites.end());
    }
    MenuCanvas overlay(assets_, appearance_, 32767, intensity_);
    overlay.cursor(FF6_ROM_VALUE_SAVE_MENU_SLOT_CURSOR_0,
                   mode_ == SaveMenuMode::load
                       ? (cursor_ ? FF6_ROM_VALUE_SAVE_MENU_SLOT_CURSOR_2 + int(cursor_ - 1) * FF6_ROM_VALUE_SAVE_MENU_ROW_STEP_0
                                  : FF6_ROM_VALUE_SAVE_MENU_SLOT_CURSOR_1)
                       : FF6_ROM_VALUE_SAVE_MENU_SLOT_CURSOR_2 + int(cursor_) * FF6_ROM_VALUE_SAVE_MENU_ROW_STEP_0);
    if (!notice_.empty()) {
        overlay.rectangle({0, 1, 26, 28, 1});
        overlay.text(interface_text(notice_.starts_with("Recovered")
                                        ? "Recovered previous save"
                                        : "Save error   see window title"),
                     16, 215);
    }
    auto image = overlay.finish();
    result.layers.insert(result.layers.end(), image.layers.begin(), image.layers.end());
    result.objects->sprites.insert(result.objects->sprites.begin(), image.objects->sprites.begin(),
                                   image.objects->sprites.end());
    return result;
}
} // namespace ff6
