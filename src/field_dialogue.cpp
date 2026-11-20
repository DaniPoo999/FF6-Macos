#include "rom-materials/field_dialogue_glyph_alignment.macros.inc"
#include "rom-materials/field_dialogue_clear_visits.macros.inc"
#include "rom-materials/field_dialogue_fixed_pause.macros.inc"
#include "rom-materials/field_dialogue_pause_unit.macros.inc"
#include "rom-materials/field_dialogue_wallpaper_palette.macros.inc"
#include "rom-materials/field_dialogue_title_bands.macros.inc"
#include "rom-materials/field_dialogue_title_blend_span.macros.inc"
#include "rom-materials/field_dialogue_title_shade_origin.macros.inc"
#include "rom-materials/field_dialogue_clear_width.macros.inc"
#include "rom-materials/field_dialogue_glyph_top.macros.inc"
#include "rom-materials/field_dialogue_text_left.macros.inc"
#include "rom-materials/field_dialogue_text_right.macros.inc"
#include "ff6/field_dialogue.hpp"
#include "rom-materials/field_dialogue_text_width.macros.inc"
#include "rom-materials/field_dialogue_title_hold.macros.inc"
#include "rom-materials/field_dialogue_window_phases.macros.inc"
#include "rom-materials/field_dialogue_title_window_scroll.macros.inc"
#include "rom-materials/field_dialogue_title_text_scroll.macros.inc"
#include "rom-materials/field_dialogue_top_band.macros.inc"
#include "rom-materials/field_dialogue_bottom_band.macros.inc"
#include "rom-materials/field_dialogue_ending_bands.macros.inc"
#include "rom-materials/field_viewport_bounds.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
template <class T> void copy_bytes(ByteView bytes, T& out) {
    require(bytes.size() == out.size(), "Invalid dialogue resource extent");
    std::copy(bytes.begin(), bytes.end(), out.begin());
}
Bytes name_glyphs(ByteView bytes) {
    Bytes result;
    for (auto value : bytes) {
        if (value == 255)
            break;
        result.push_back(wrap8(unsigned{value} + 256U - 96U));
    }
    return result;
}
ByteView item_name(const FieldDialogueAssets& a, std::uint8_t item) {
    return slice(a.item_names, std::size_t{item} * 13 + 1, 12);
}
} // namespace

FieldDialogueAssets load_field_dialogue(const ContentPack& pack) {
    FieldDialogueAssets a;
    const auto own = [&](const char* id) {
        const auto bytes = pack.get(id);
        return Bytes(bytes.begin(), bytes.end());
    };
    a.script = own("text_dlg_bin");
    a.item_names = own("text_item_name_bin");
    a.spell_names = own("text_magic_name_bin");
    const auto titles = pack.get("text_map_title_bin");
    const auto title_pointers = pack.get("text_map_title_ptr_bin");
    require(title_pointers.size() == 73 * 2, "Invalid map-title pointers");
    for (std::size_t i = 0; i < 73; ++i) {
        auto at = std::size_t{read16(title_pointers, i * 2)};
        Bytes title;
        while (at < titles.size() && titles[at] != 0) {
            require(titles[at] >= 32 && titles[at] < 128, "Invalid map-title glyph");
            title.push_back(titles[at++]);
        }
        require(at < titles.size(), "Unterminated map title");
        a.titles.push_back(std::move(title));
    }
    const auto pointers = pack.get("text_dlg_ptr_bin"), bank = pack.get("text_dlg_bank_bin");
    require(bank.size() == 2 && pointers.size() == 3084 * 2 && read16(bank, 0) <= 3084 &&
                a.item_names.size() == 256 * 13 && a.spell_names.size() == 54 * 7,
            "Invalid dialogue tables");
    for (std::size_t i = 0; i < pointers.size() / 2; ++i) {
        const auto offset =
            std::uint32_t{read16(pointers, i * 2)} + (i >= read16(bank, 0) ? 65536U : 0U);
        require(offset < a.script.size(), "Dialogue pointer outside text resource");
        a.messages.push_back(offset);
    }
    copy_bytes(pack.get("font_tables_widths"), a.widths);
    copy_bytes(pack.get("data_c0dfa0_dte_character_pairs"), a.pairs);
    const auto glyphs = decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
    require(glyphs.size() == a.glyphs.size(), "Invalid dialogue font");
    std::copy(glyphs.begin(), glyphs.end(), a.glyphs.begin());
    const auto tiles = decode_tiles(pack.get("menu_graphics_window_gfx_4bpp"), 4);
    const auto colors = pack.get("menu_graphics_window_pal_pal");
    require(tiles.size() == 8 * 28 && colors.size() == 8 * 32, "Invalid dialogue window skins");
    for (std::size_t s = 0; s < 8; ++s) {
        for (std::size_t t = 0; t < 28; ++t) {
            a.skins[s][t] = tiles[s * 28 + t];
            for (auto& pixel : a.skins[s][t])
                pixel |= 8U; // Original makes every wallpaper pixel opaque.
        }
        for (std::size_t c = 0; c < 7; ++c)
            a.colors[s][c] = read16(colors, s * 32 + (c + 1) * 2) & 0x7fffU;
    }
    const auto formation = pack.get("data_c036c1_vram_tilemap_pattern_tables");
    require(formation.size() == 352, "Invalid dialogue formation extent");
    for (std::size_t i = 0; i < a.window_tiles.size(); ++i) {
        const auto tile = formation[64 + i];
        require(tile >= 0xe0 && tile <= 0xfb, "Invalid dialogue wallpaper tile");
        a.window_tiles[i] = static_cast<std::uint8_t>(tile - 0xe0U);
    }
    copy_bytes(pack.get("data_c03cd7_wmdata_wave_fill_table"), a.shades);
    for (auto& shade : a.shades) {
        require((shade & 0xe0U) == 0xe0U, "Invalid dialogue shade");
        shade &= 31U;
    }
    copy_bytes(pack.get("data_c03cc7_wmdata_reverse_ramp_table"), a.ending_shades);
    for (auto& shade : a.ending_shades) {
        require((shade & 0xe0U) == 0xe0U, "Invalid ending fade shade");
        shade &= 31U;
    }
    copy_bytes(pack.get("field_dialogue_window_scroll"), a.window_scroll);
    copy_bytes(pack.get("field_dialogue_cursor"), a.cursor);
    const auto text_scroll = pack.get("field_dialogue_text_scroll");
    require(text_scroll.size() == 144, "Invalid dialogue text scroll extent");
    // Initialization keeps the low byte of each signed source word, then clears
    // its high byte. Text coordinates therefore wrap in a 256-pixel canvas.
    for (std::size_t i = 0; i < 72; ++i)
        a.text_scroll[i] = text_scroll[i * 2];
    return a;
}

void compose_field_ending_bars(RasterScene& scene, const FieldDialogueAssets& a) {
    require(scene.width == 256 && scene.height == 224, "Invalid ending field viewport");
    scene.color_effect_rows.resize(scene.height);
    static_assert(FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_3 + 1U == 8U);
    // C0/39A8: the first strip has seven lines; strips 1/2 and 17/18
    // subtract the authored gradient in opposite directions. This replaces
    // map lighting, including fixed-color flashes, for these rows only.
    for (unsigned i = 0; i < 16; ++i) {
        require(a.ending_shades[i] < 32, "Invalid ending fade shade");
        const RasterColorEffect effect{static_cast<Rgb555>(a.ending_shades[i] * 1057U),
                                       ColorBlend::subtract, false};
        scene.color_effect_rows[FF6_ROM_VALUE_FIELD_DIALOGUE_ENDING_BANDS_0 * 8U - 1U + i] = effect;
        scene.color_effect_rows[FF6_ROM_VALUE_FIELD_DIALOGUE_ENDING_BANDS_1 * 8U + 14U - i] = effect;
    }
    // Strips 19..26 select only the blank middle of the dialogue tilemap.
    // Opening a caption replaces these rows using its ordinary text canvas.
    for (unsigned y = FF6_ROM_VALUE_FIELD_DIALOGUE_ENDING_BANDS_2 * 8U - 1U;
         y < (FF6_ROM_VALUE_FIELD_DIALOGUE_ENDING_BANDS_2 + FF6_ROM_VALUE_FIELD_DIALOGUE_ENDING_BANDS_3) * 8U - 1U; ++y) {
        for (auto& layer : scene.layers)
            layer.lines.at(y).enabled = false;
        if (scene.objects)
            scene.objects->lines.at(y).enabled = false;
    }
}

void FieldDialogue::show(const FieldDialogueAssets& a, EventDialogue message, bool ending_bars) {
    require(title_remaining_ == 0,
            "Dialogue overlapping a map title requires retained overlapping window ownership");
    require(message.message < a.messages.size() && a.messages[message.message] < a.script.size(),
            "Dialogue message outside resource");
    require(
        !open() || state_.bottom == message.bottom,
        "Moving an already open dialogue window requires retained overlapping window ownership");
    require(!open() || state_.text_only == message.text_only,
            "Changing an open dialogue presentation requires retained mixed window ownership");
    // GetDlgPtr and event 48/4B do not reset the text cursor, page, buffer, waits
    // or choice result. Those are cleared by their actual consumers.
    state_.position = a.messages[message.message];
    state_.text_active = true;
    state_.end_pending = false;
    state_.bottom = message.bottom;
    state_.text_only = message.text_only;
    state_.ending_bars = ending_bars;
    state_.window = DialogueWindow::opening;
}
void FieldDialogue::show_map_title(const FieldDialogueAssets& a, std::uint8_t title) {
    require(title < a.titles.size(), "Map title outside resource");
    require(state_ == FieldDialogueStatus{} && title_remaining_ == 0,
            "Map-title initialization requires fresh map dialogue state");
    unsigned width = 0;
    for (const auto glyph : a.titles[title]) {
        require(glyph >= 32 && glyph < 128, "Invalid map-title glyph");
        width += a.widths[glyph];
        require(width < FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_WIDTH_0, "Map title exceeds supported single-line width");
    }
    auto next = *this;
    next.state_.x = static_cast<std::uint8_t>((unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_WIDTH_0} - width) / 2U);
    next.state_.block = next.state_.x / 16U;
    // ShowMapTitle publishes each glyph synchronously using the ordinary text
    // canvas, including the blank-glyph overhang flush at the terminator.
    for (const auto glyph : a.titles[title]) {
        next.draw(a, glyph);
        next.publish();
        ++next.state_.position;
    }
    next.newline(a, true);
    next.publish();
    next.state_.key_wait = next.state_.clear_region = 0;
    next.state_.text_active = next.state_.end_pending = true;
    next.title_remaining_ = FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_HOLD_0;
    *this = std::move(next);
}
void FieldDialogue::close_map_title() {
    if (title_remaining_ == 0)
        return;
    title_remaining_ = 0;
    state_.text_active = state_.end_pending = false;
    state_.clear_region = FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_VISITS_0;
}
std::uint8_t FieldDialogue::word_width(const FieldDialogueAssets& a,
                                       const FieldDialogueValues& values) const {
    auto at = std::size_t{state_.position};
    bool buffered = !buffer_.empty();
    std::uint8_t width = 0;
    const auto add = [&](std::uint8_t glyph) { width = wrap8(unsigned{width} + a.widths[glyph]); };
    while (at < a.script.size()) {
        const auto token = a.script[at++];
        if (token >= 128) {
            const auto pair = std::size_t{token & 127U} * 2;
            if (!buffered) {
                if (a.pairs[pair] == 127)
                    return width;
                add(a.pairs[pair]);
            } else
                buffered = false;
            if (a.pairs[pair + 1] == 127)
                return width;
            add(a.pairs[pair + 1]);
        } else if (token >= 32) {
            if (token == 127)
                return width;
            add(token);
        } else {
            if (!buffered) {
                if (token >= 2 && token < 16) {
                    for (auto glyph : name_glyphs(values.names[token - 2U]))
                        add(glyph);
                } else if (token == 26) {
                    for (auto glyph : name_glyphs(item_name(a, values.item)))
                        add(glyph);
                }
            }
            return width;
        }
    }
    throw std::invalid_argument("Unterminated dialogue word");
}
void FieldDialogue::paint(const FieldDialogueAssets& a, std::uint8_t glyph) {
    require(glyph >= 32, "Dialogue substitution selected an unavailable font glyph");
    const unsigned shift = state_.x & 7U;
    for (std::size_t row = 0; row < 11; ++row) {
        const std::uint32_t raw = a.glyphs[glyph - 32U][row];
        std::uint32_t ink = raw << 16U, shadow = 0;
        if (shift < FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_ALIGNMENT_0) {
            ink = std::uint32_t{wrap16(raw << (FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_ALIGNMENT_0 - shift))} << 16U;
            shadow = (ink >> 1U) & 0xffff0000U;
        } else if (shift == FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_ALIGNMENT_0)
            shadow = (ink >> 1U) & 0xffff0000U;
        else {
            ink >>= shift - FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_ALIGNMENT_0;
            shadow = ink >> 1U;
        }
        // DrawLetter copies only the high byte of the overflow word. With the
        // supported 16-pixel source glyphs the omitted low byte is always zero.
        if ((state_.x & 8U) != 0) {
            ink >>= 8U;
            shadow >>= 8U;
        }
        ink_[row] |= ink;
        shadow_[row + 1] |= shadow;
    }
}
void FieldDialogue::stage() {
    require(state_.line < 4 && state_.block < 16, "Dialogue text exceeds native page");
    pending_.fill(0);
    for (std::size_t y = 0; y < 12; ++y)
        for (unsigned x = 0; x < 16; ++x)
            pending_[(y + FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_TOP_0) * 16 + x] = static_cast<std::uint8_t>(
                ((ink_[y] >> (31U - x)) & 1U) | (((shadow_[y] >> (31U - x)) & 1U) << 1U));
    destination_ = {state_.line, state_.block};
    text_pending_ = true;
}
void FieldDialogue::draw(const FieldDialogueAssets& a, std::uint8_t glyph) {
    const auto next_x = wrap8(unsigned{state_.x} + a.widths[glyph]);
    if (next_x >= FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_WIDTH_0) {
        newline(a, false);
        return;
    }
    paint(a, glyph);
    stage();
    if (((state_.x & 15U) + a.widths[glyph]) >= 16) {
        for (auto& row : ink_)
            row <<= 16U;
        for (auto& row : shadow_)
            row <<= 16U;
        ++state_.block;
    }
    state_.x = next_x;
}
void FieldDialogue::newline(const FieldDialogueAssets& a, bool page) {
    paint(a, 255); // Flush the retained overhang with the original blank glyph.
    stage();
    state_.x = FF6_ROM_VALUE_FIELD_DIALOGUE_INITIAL_X_0;
    state_.block = 0;
    state_.line = page ? 0 : static_cast<std::uint8_t>((state_.line + 1U) & 3U);
    ink_.fill(0);
    shadow_.fill(0);
    if (state_.line == 0) {
        state_.clear_region = FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_VISITS_0;
        state_.key_wait = 2;
    }
}
void FieldDialogue::draw_buffer(const FieldDialogueAssets& a) {
    require(buffer_at_ < buffer_.size(), "Empty dialogue substitution");
    const auto glyph = buffer_[buffer_at_++];
    if (buffer_at_ == buffer_.size()) {
        buffer_.clear();
        buffer_at_ = 0;
        ++state_.position;
    }
    draw(a, glyph);
}
void FieldDialogue::draw_cursor(const FieldDialogueAssets& a, Buttons buttons) {
    require(state_.selection < state_.choices, "Dialogue selection outside choices");
    destination_ = choices_[state_.selection];
    const bool previous = held(buttons, Button::up) || held(buttons, Button::left);
    const bool next = held(buttons, Button::down) || held(buttons, Button::right);
    if (!previous && !next)
        choice_latched_ = false;
    else {
        if (choice_latched_)
            return;
        if (previous && state_.selection > 0) {
            --state_.selection;
            choice_latched_ = true;
        } else if (!previous && state_.selection + 1U < state_.choices) {
            ++state_.selection;
            choice_latched_ = true;
        }
    }
    pending_.fill(0);
    for (std::size_t row = 0; row < 12; ++row)
        for (unsigned x = 0; x < 8; ++x)
            pending_[(row + FF6_ROM_VALUE_FIELD_DIALOGUE_GLYPH_TOP_0) * 16 + x] =
                static_cast<std::uint8_t>(((a.cursor[row] >> (7U - x)) & 1U) |
                                          ((((a.cursor[row] >> 1U) >> (7U - x)) & 1U) << 1U));
    cursor_destination_ = choices_[state_.selection];
    cursor_pending_ = true;
}
void FieldDialogue::update_text(const FieldDialogueAssets& a, const FieldDialogueValues& values,
                                Buttons buttons) {
    if (!state_.text_active)
        return;
    if (state_.pause) {
        --state_.pause;
        return;
    }
    if (state_.key_timer) {
        if ((state_.key_timer & 0x7fffU) == 0) {
            state_.key_timer = 0;
            state_.key_wait = 0;
            state_.choices = 0;
        } else
            --state_.key_timer;
    }
    if (state_.key_wait) {
        if (state_.choices >= 2)
            draw_cursor(a, buttons);
        const bool confirm = held(buttons, Button::a);
        if (state_.key_wait == 1 && confirm) {
            state_.key_wait = 0;
            state_.choices = 0;
            state_.key_timer &= 255U; // The original clears only the timer's high byte.
        } else if (state_.key_wait != 1 && !confirm)
            --state_.key_wait;
        return;
    }
    if (state_.clear_region) {
        --state_.clear_region;
        return;
    }
    if (state_.end_pending) {
        state_.window = DialogueWindow::closing;
        state_.text_active = false;
        return;
    }
    if (unsigned{state_.x} + word_width(a, values) >= FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_WIDTH_0) {
        newline(a, false);
        return;
    }
    if (!buffer_.empty()) {
        draw_buffer(a);
        return;
    }
    const auto token = slice(a.script, state_.position, 1)[0];
    if (token >= 128) {
        const auto at = std::size_t{token & 127U} * 2;
        buffer_ = {a.pairs[at], a.pairs[at + 1]};
        draw_buffer(a);
        return;
    }
    if (token >= 32) {
        draw(a, token);
        ++state_.position;
        return;
    }
    if (token >= 2 && token < 16) {
        buffer_ = name_glyphs(values.names[token - 2U]);
        draw_buffer(a);
        return;
    }
    switch (token) {
    case 0:
        newline(a, true);
        state_.end_pending = true;
        break;
    case 1:
        newline(a, false);
        ++state_.position;
        break;
    case 16:
        state_.pause = FF6_ROM_VALUE_FIELD_DIALOGUE_FIXED_PAUSE_0;
        ++state_.position;
        break;
    case 17:
        state_.pause =
            static_cast<std::uint16_t>(unsigned{slice(a.script, state_.position + 1U, 1)[0]} * FF6_ROM_VALUE_FIELD_DIALOGUE_PAUSE_UNIT_0);
        state_.position += 2;
        break;
    case 18:
        state_.key_timer = 0x8001;
        ++state_.position;
        break;
    case 19:
        newline(a, true);
        ++state_.position;
        break;
    case 20: {
        const auto count = slice(a.script, state_.position + 1U, 1)[0];
        require(count > 0 && count < 128, "Dialogue tab exceeds supported buffer");
        buffer_.assign(count, 127);
        ++state_.position;
        break;
    }
    case 21:
        require(state_.choices < choices_.size(), "Too many dialogue choices");
        choices_[state_.choices++] = {state_.line, state_.block};
        draw(a, 255);
        ++state_.position;
        break;
    case 22:
        state_.key_timer = static_cast<std::uint16_t>(
            0x8000U | (unsigned{slice(a.script, state_.position + 1U, 1)[0]} * FF6_ROM_VALUE_FIELD_DIALOGUE_PAUSE_UNIT_0));
        state_.key_wait = 1;
        state_.position += 2;
        break;
    case 25: {
        require(values.gil > 0 && values.gil <= 9999999,
                "Dialogue quantity has no supported decimal representation");
        auto number = values.gil;
        do {
            buffer_.push_back(static_cast<std::uint8_t>(number % 10U + 0x54U));
            number /= 10U;
        } while (number);
        std::reverse(buffer_.begin(), buffer_.end());
        draw_buffer(a);
        break;
    }
    case 26:
        buffer_ = name_glyphs(item_name(a, values.item));
        draw_buffer(a);
        break;
    case 27:
        // USA code retains the Japanese four-byte stride and length. Do not
        // silently replace it with the nominal seven-byte English name record.
        buffer_ = name_glyphs(slice(a.spell_names, std::size_t{values.spell} * 4 + 1, 4));
        draw_buffer(a);
        break;
    default:
        throw std::invalid_argument("Unsupported extended dialogue font command");
    }
}
void FieldDialogue::update_window() {
    if (state_.window == DialogueWindow::opening && state_.phase < FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0) {
        const unsigned inset = (unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} - 1U) - state_.phase;
        ++state_.phase;
        for (unsigned strip = inset; strip < (unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} * 2U - 1U) - inset; ++strip)
            bands_[strip] = state_.phase;
    } else if (state_.window == DialogueWindow::closing) {
        if (state_.phase == 0)
            state_.window = DialogueWindow::closed;
        else {
            --state_.phase;
            const unsigned inset = (unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} - 1U) - state_.phase;
            bands_[inset] = bands_[(unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} * 2U - 2U) - inset] = 0;
        }
    }
}
void FieldDialogue::advance(const FieldDialogueAssets& a, const FieldDialogueValues& values,
                            Buttons buttons) {
    auto next = *this;
    if (next.title_remaining_ == 1)
        next.close_map_title();
    else if (next.title_remaining_ != 0)
        --next.title_remaining_;
    next.update_text(a, values, buttons);
    next.update_window();
    *this = std::move(next);
}
void FieldDialogue::publish() {
    const auto transfer = [&](Position at, unsigned width, unsigned source_x) {
        require(at.line < 4 && at.block < 16, "Dialogue publication outside page");
        for (std::size_t y = 0; y < 16; ++y)
            for (unsigned x = 0; x < width; ++x)
                pixels_[(std::size_t{at.line} * 16 + y) * 256 + at.block * 16 + x] =
                    pending_[y * 16 + source_x + x];
    };
    if (cursor_pending_) {
        transfer(destination_, 8, 8);
        transfer(cursor_destination_, 8, 0);
        cursor_pending_ = false;
    }
    if (text_pending_) {
        transfer(destination_, 16, 0);
        text_pending_ = false;
    }
    if (state_.clear_region > 0 && state_.clear_region < FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_VISITS_0) {
        const unsigned region = unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_VISITS_0} - 1U - state_.clear_region;
        // Tile numbering interleaves top/bottom tiles. A 28-tile clear removes
        // 112x16 pixels, not a 224x8 horizontal strip. The unused tail survives.
        for (unsigned y = (region / 2U) * 16; y < (region / 2U + 1U) * 16; ++y)
            std::fill_n(pixels_.begin() + y * 256 + (region % 2U) * FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_WIDTH_0, FF6_ROM_VALUE_FIELD_DIALOGUE_CLEAR_WIDTH_0, 0);
    }
}
bool FieldDialogue::consume(const FieldDialogueAssets& a, FieldEventRunner& events,
                            EventFlagAccess& flags) {
    const auto& request = events.pending_request();
    if (!request)
        return false;
    if (const auto* message = std::get_if<EventDialogue>(&request->operation)) {
        auto next = *this;
        next.show(a, *message, flags.test(0x2c0));
        events.complete_request(request->identity, flags);
        *this = std::move(next);
        return true;
    }
    if (std::holds_alternative<EventDialogueSelection>(request->operation)) {
        events.complete_request(request->identity, flags, {}, {}, state_.selection);
        state_.selection = 0;
        return true;
    }
    return false;
}

void FieldDialogue::compose(RasterScene& scene, const FieldDialogueAssets& a, std::uint8_t skin,
                            const std::array<Rgb555, 128>& colors,
                            std::optional<std::array<std::size_t, 2>> underlay) const {
    require(scene.width == 256 && scene.height == 224 && skin < 8,
            "Invalid dialogue viewport/skin");
    const bool title = title_remaining_ != 0;
    if (!title && std::all_of(bands_.begin(), bands_.end(), [](auto phase) { return phase == 0; }))
        return;
    if (state_.text_only && !state_.ending_bars) {
        require(underlay && (*underlay)[0] < scene.layers.size() &&
                    (*underlay)[1] < scene.layers.size() && (*underlay)[0] != (*underlay)[1],
                "Text-only dialogue requires both field backgrounds, including hidden ones");
        require(scene.blend_rows.empty() || scene.blend_rows.size() == scene.height,
                "Invalid dialogue row blend extent");
        scene.blend_rows.resize(scene.height);
    }
    TileLayer window;
    window.columns = 32;
    window.rows = 9;
    window.wrap_y = false;
    window.tiles.assign(a.skins[skin].begin(), a.skins[skin].end());
    std::array<Rgb555, 16> wallpaper{};
    std::copy_n(colors.begin() + FF6_ROM_VALUE_FIELD_DIALOGUE_WALLPAPER_PALETTE_0 * 16, 16, wallpaper.begin());
    window.palettes.push_back(wallpaper);
    for (auto tile : a.window_tiles)
        window.cells.push_back({tile, 0, 11});
    window.lines.resize(224);
    for (auto& line : window.lines)
        line.enabled = false;
    TileLayer text;
    text.columns = 32;
    text.rows = 8;
    text.wrap_x = text.wrap_y = false;
    text.palettes.push_back({});
    std::copy_n(colors.begin(), 4, text.palettes[0].begin());
    text.lines.resize(224);
    for (auto& line : text.lines)
        line.enabled = false;
    for (unsigned ty = 0; ty < 8; ++ty)
        for (unsigned tx = 0; tx < 32; ++tx) {
            Tile tile{};
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; ++x)
                    tile[y * 8 + x] = pixels_[(ty * 8 + y) * 256 + tx * 8 + x];
            text.cells.push_back({static_cast<std::uint32_t>(text.tiles.size()), 0, 12});
            text.tiles.push_back(tile);
        }
    // The field's first band is seven scanlines; tile row 1 starts at y=7.
    const unsigned top = !title && state_.bottom
        ? FF6_ROM_VALUE_FIELD_DIALOGUE_BOTTOM_BAND_0 * 8U - 1U
        : FF6_ROM_VALUE_FIELD_DIALOGUE_TOP_BAND_0 * 8U - 1U;
    constexpr std::array<unsigned, 5> shade_starts{
#include "rom-materials/field_dialogue_shade_starts.inc"
    };
    constexpr std::array<unsigned, 4> scroll_starts{
#include "rom-materials/field_dialogue_scroll_starts.inc"
    };
    for (unsigned strip = 0; strip < (title ? FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_BANDS_0 + 1U : unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} * 2U - 1U); ++strip) {
        const unsigned phase = title ? unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} : bands_[strip];
        if (!phase)
            continue;
        const unsigned inset = unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0} - phase;
        for (unsigned row = 0; row < 8; ++row) {
            const unsigned y = top + strip * 8 + row;
            const unsigned local = (strip - inset) * 8 + row;
            if (!scene.blend_spans.empty())
                scene.blend_spans.at(y) = {FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0,
                                         FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1U};
            // UpdateOpenText preserves the ending's row color settings.
            // Ordinary text-only and wallpaper windows replace them.
            if (!(state_.text_only && state_.ending_bars) && !scene.color_effect_rows.empty())
                scene.color_effect_rows.at(y).reset();
            for (std::size_t i = 0; i < scene.layers.size(); ++i) {
                auto& line = scene.layers[i].lines.at(y);
                if (state_.text_only && !state_.ending_bars &&
                    (i == (*underlay)[0] || i == (*underlay)[1])) {
                    line.enabled = true;
                    line.coverage = RasterCoverage{true, false};
                    // Text-only strips select color math off, including an
                    // active field fixed-color ramp below the dialogue.
                    line.blend = ColorBlend::none;
                } else
                    line.enabled = false;
            }
            if (scene.objects) {
                auto& line = scene.objects->lines.at(y);
                if (state_.text_only && !state_.ending_bars) {
                    line.enabled = true;
                    line.coverage = RasterCoverage{true, false};
                } else
                    line.enabled = false;
            }
            if (state_.text_only && !state_.ending_bars)
                scene.blend_rows[y] = CompositeBlend{};
            auto& background = window.lines[y];
            background.enabled = !state_.text_only;
            background.left = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_0;
            background.right = FF6_ROM_VALUE_FIELD_VIEWPORT_BOUNDS_1 + 1U;
            background.offset_y =
                -static_cast<std::int32_t>(top) +
                (title        ? (strip == FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_BANDS_0 ? FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_WINDOW_SCROLL_0 : 0)
                 : phase == FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0 ? 0
                              : signed8(a.window_scroll[scroll_starts[phase - 1] + local]));
            background.operand = static_cast<Rgb555>(
                unsigned{a.shades[(title ? FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_SHADE_ORIGIN_0 : shade_starts[phase - 1]) + local]} * 1057U);
            background.blend = local < (title ? FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_BLEND_SPAN_0 : (phase * 16U - 8U) / 2)
                                   ? ColorBlend::add
                                   : ColorBlend::subtract;
            // Title source sections overlap: every displayed row selects -6.
            const unsigned scroll =
                title ? unsigned{FF6_ROM_VALUE_FIELD_DIALOGUE_TITLE_TEXT_SCROLL_0} : a.text_scroll[phase == FF6_ROM_VALUE_FIELD_DIALOGUE_WINDOW_PHASES_0 ? strip * 8 + row : row];
            // Text formation starts on tile row 1/18, at x=16. Discard its
            // blank surrounding rows instead of wrapping the 64-pixel page.
            const unsigned text_y = (y + 1U + scroll) & 255U;
            const unsigned origin = top + 1U;
            if (text_y >= origin && text_y < origin + 64U) {
                auto& line = text.lines[y];
                line.enabled = true;
                line.left = FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_LEFT_0;
                line.right = FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_RIGHT_0;
                line.offset_x = -FF6_ROM_VALUE_FIELD_DIALOGUE_TEXT_LEFT_0;
                line.offset_y =
                    static_cast<std::int32_t>(text_y - origin) - static_cast<std::int32_t>(y);
            }
        }
    }
    if (!state_.text_only)
        scene.layers.push_back(std::move(window));
    scene.layers.push_back(std::move(text));
}
} // namespace ff6
