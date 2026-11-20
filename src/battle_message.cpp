#include "ff6/battle_message.hpp"
#include "rom-materials/battle_attack_name_center.macros.inc"
#include "rom-materials/battle_attack_name_capacity.macros.inc"
#include "rom-materials/battle_message_ink_origin.macros.inc"
#include "rom-materials/battle_announcement_selections.macros.inc"
#include "rom-materials/battle_message_window_sizes.macros.inc"
#include "rom-materials/battle_message_window_columns.macros.inc"
#include "rom-materials/battle_dialogue_window_placement.macros.inc"
#include "rom-materials/battle_dialogue_text_column.macros.inc"
#include "rom-materials/battle_announcement_text_column.macros.inc"
#include "rom-materials/battle_message_text_widths.macros.inc"
#include "rom-materials/battle_dialogue_text_origin.macros.inc"
#include "rom-materials/battle_window_color_arithmetic.macros.inc"
#include "rom-materials/battle_window_gradient_geometry.macros.inc"
#include "rom-materials/battle_message_gradient_period.macros.inc"
#include "rom-materials/battle_window_gradient_period.macros.inc"
#include <algorithm>
#include "rom-materials/battle_message_first_wait.macros.inc"
#include "rom-materials/battle_message_wait_steps.macros.inc"
#include <stdexcept>
namespace ff6 {
// Extraction checks every consecutive table difference before supplying the scale.

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
}
BattleMessageAssets load_battle_message_assets(const ContentPack& pack) {
    BattleMessageAssets out;
    const auto own = [&](const char* id) { const auto bytes = pack.get(id); return Bytes(bytes.begin(), bytes.end()); };
    out.messages = own("text_attack_msg_bin");
    out.pointers = own("text_attack_msg_ptr_bin");
    out.item_names = own("text_item_name_bin");
    out.attack_names = own("text_attack_name_bin");
    out.magic_names = own("text_magic_name_bin");
    out.esper_names = own("text_genju_name_bin");
    out.bushido_names = own("text_bushido_name_bin");
    out.command_names = own("text_battle_cmd_name_bin");
    out.glyphs = decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
    const auto widths = pack.get("font_tables_widths");
    require(widths.size() == 256 && out.pointers.size() == 512 && out.item_names.size() == 256 * 13 &&
                out.attack_names.size() == 175 * 10 && out.magic_names.size() == 54 * 7 &&
                out.esper_names.size() == 27 * 8 && out.bushido_names.size() == 8 * 12 && out.command_names.size() == 32 * 7,
            "Invalid battle message resources");
    std::ranges::copy(widths, out.widths.begin());
    return out;
}
BattleMessage format_battle_ability_name(const BattleMessageAssets& assets, std::uint8_t attack) {
    // C1/5FB8–6042 chooses the table by attack identity. Spell names omit
    // their category symbol; Rage can dispatch any of these ordinary spells.
    const auto name = attack < 54 ? slice(assets.magic_names, unsigned{attack} * 7U + 1U, 6)
                    : attack < 81 ? slice(assets.esper_names, unsigned{attack - 54U} * 8U, 8)
                                  : slice(assets.attack_names, unsigned{attack - 81U} * 10U, 10);
    BattleMessage message;
    message.attack_name = true;
    for (auto glyph : name) {
        if (glyph == 255) break;
        require(glyph >= 128, "Invalid battle ability name glyph");
        message.glyphs.push_back(glyph);
    }
    return message;
}
BattleMessage format_battle_command_name(const BattleMessageAssets& assets, std::uint8_t command) {
    // C1/5F16: seven-byte command names, stopping at their padding sentinel.
    require(command < 32, "Invalid battle command name identity");
    BattleMessage message;
    message.attack_name = true;
    for (auto glyph : slice(assets.command_names, unsigned{command} * 7U, 7)) {
        if (glyph == 255) break;
        require(glyph >= 128, "Invalid battle command name glyph");
        message.glyphs.push_back(glyph);
    }
    return message;
}
BattleMessage format_battle_bushido_name(const BattleMessageAssets& assets, std::uint8_t attack) {
    require(attack >= 85 && attack <= 92, "Invalid SwdTech name identity");
    BattleMessage message;
    message.attack_name = true;
    for (auto glyph : slice(assets.bushido_names, unsigned{attack - 85U} * 12U, 12)) {
        if (glyph == 255 || glyph == 0) break;
        require(glyph >= 128, "Invalid SwdTech name glyph");
        message.glyphs.push_back(glyph);
    }
    return message;
}
BattleMessage format_battle_item_name(const BattleMessageAssets& assets, std::uint8_t item) {
    BattleMessage message;
    message.attack_name = true;
    // The icon occupies the first byte; the centered battle name excludes it.
    for (auto glyph : slice(assets.item_names, unsigned{item} * 13U + 1U, 12)) {
        if (glyph == 255) break;
        require(glyph >= 128, "Invalid battle item name glyph");
        message.glyphs.push_back(glyph);
    }
    return message;
}
BattleMessage format_battle_message(const BattleMessageAssets& assets, std::uint8_t message,
                                   std::array<std::uint32_t, 4> values, const BattleParty& party) {
    require(assets.pointers.size() == 512, "Invalid battle message directory");
    const auto pointer = read16(assets.pointers, unsigned{message} * 2U);
    require(pointer >= 0xf000, "Battle message pointer precedes its resource");
    std::size_t at = pointer - 0xf000U;
    BattleMessage result;
    auto& glyphs = result.glyphs;
    const auto read = [&] { require(at < assets.messages.size(), "Truncated battle message"); return assets.messages[at++]; };
    const auto name = [&](ByteView text) {
        for (const auto c : text) { if (c == 255) break; glyphs.push_back(c); }
    };
    for (;;) {
        const auto op = read();
        if (!op) break;
        if (op >= 32) glyphs.push_back(op);
        else if (op == 7) result.wait_for_confirm = true;
        else if (op == 16 || op == 17 || op == 19 || op == 20) {
            const auto value = values[op == 16 ? 0 : op == 17 ? 1 : op == 19 ? 2 : 3];
            for (const char digit : std::to_string(value))
                glyphs.push_back(static_cast<std::uint8_t>(180 + digit - '0'));
        } else if (op == 18) {
            const auto type = read();
            if (type == 0) {
                require(values[1] < 4, "Battle message character outside party");
                name(party.slots[values[1]].name);
            } else if (type == 1) {
                require(values[0] < 256, "Battle message item outside inventory");
                name(slice(assets.item_names, values[0] * 13 + 1, 12));
            } else if (type == 2) {
                require(values[0] < 256, "Battle message attack outside ability table");
                const auto ability = format_battle_ability_name(assets, static_cast<std::uint8_t>(values[0]));
                glyphs.insert(glyphs.end(), ability.glyphs.begin(), ability.glyphs.end());
            } else throw std::invalid_argument("Unsupported battle message substitution");
        } else throw std::invalid_argument("Unsupported battle message command");
        require(glyphs.size() <= 80, "Battle message exceeds single-line bounds");
    }
    return result;
}
std::uint8_t battle_announcement_message(BattleAnnouncement announcement) {
    switch (announcement) {
    case BattleAnnouncement::back: return FF6_ROM_VALUE_BATTLE_ANNOUNCEMENT_SELECTIONS_0;
    case BattleAnnouncement::pincer: return FF6_ROM_VALUE_BATTLE_ANNOUNCEMENT_SELECTIONS_1;
    case BattleAnnouncement::side: return FF6_ROM_VALUE_BATTLE_ANNOUNCEMENT_SELECTIONS_2;
    case BattleAnnouncement::preemptive: return FF6_ROM_VALUE_BATTLE_ANNOUNCEMENT_SELECTIONS_3;
    }
    throw std::invalid_argument("Unknown battle announcement");
}
BattleMessagePresentation::BattleMessagePresentation(BattleMessage message, std::uint8_t speed)
    : message_(std::move(message)), remaining_(FF6_ROM_VALUE_BATTLE_MESSAGE_FIRST_WAIT_0 +
          (speed & 7U) * FF6_ROM_VALUE_BATTLE_MESSAGE_WAIT_STEPS_0) {
    steps_ = std::move(message_.steps);
}
void BattleMessagePresentation::advance(Buttons buttons) {
    const bool confirm = held(buttons, Button::a), pressed = confirm && !confirm_held_;
    confirm_held_ = confirm;
    if (key_wait_) {
        if (pressed) {
            key_wait_ = false;
            confirmed_text_ = true;
        }
        return;
    }
    if (wait_) {
        --wait_;
        return;
    }
    if (scroll_) {
        if (++scroll_ == 16) {
            scroll_ = 0;
            message_.glyphs.clear();
        }
        return;
    }
    if (at_ < steps_.size()) {
        const auto step = steps_[at_++];
        switch (step.kind) {
        case BattleMessageStepKind::glyph:
            require(message_.glyphs.size() < 80, "Monster dialogue line exceeds bounds");
            message_.glyphs.push_back(step.value);
            break;
        case BattleMessageStepKind::newline:
            scroll_ = 1;
            break;
        case BattleMessageStepKind::wait:
            wait_ = step.value;
            break;
        case BattleMessageStepKind::confirm:
            key_wait_ = true;
            break;
        }
        return;
    }
    if (confirmed_text_)
        remaining_ = 0;
    else if (message_.wait_for_confirm) {
        if (pressed)
            remaining_ = 0;
    } else if (remaining_)
        --remaining_;
}
void compose_battle_message(RasterScene& scene, const BattleReadoutAssets& assets,
                            const BattleReadoutView& readout,
                            const BattleMessageAssets& text_assets, const BattleMessage& message,
                            bool dialogue, unsigned scroll) {
    // WindowBufTbl's wide-message rectangle and InitMsgWindowHDMA's gradient.
    // Keep the message surface independent of the command-list canvases.
    auto window = make_battle_readout_layers(assets, readout).windows;
    window.cells.assign(256, RasterCell{28, 0, 20});
    const bool narrow = message.attack_name && !dialogue;
    draw_battle_window(assets, {static_cast<std::uint8_t>(narrow ? FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_COLUMNS_1 : FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_COLUMNS_0), 0,
                               static_cast<std::uint8_t>(narrow ? FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_SIZES_1 : FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_SIZES_0),
                               static_cast<std::uint8_t>(dialogue ? FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_SIZES_3 : FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_SIZES_2)}, window);
    for (unsigned y = 0; y < window.lines.size(); ++y) {
        auto& line = window.lines[y];
        const unsigned top = dialogue ? FF6_ROM_VALUE_BATTLE_DIALOGUE_WINDOW_PLACEMENT_0 : FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_1;
        line.enabled = y >= top && y < top + (dialogue ? FF6_ROM_VALUE_BATTLE_DIALOGUE_WINDOW_PLACEMENT_1 : FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_2);
        line.offset_y = -static_cast<int>(top);
        line.blend = static_cast<ColorBlend>(FF6_ROM_VALUE_BATTLE_WINDOW_COLOR_ARITHMETIC_0);
        line.operand = static_cast<Rgb555>((dialogue ? (y >= FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0 ? (y - FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0) / FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_PERIOD_0 : 0)
                                                     : (line.enabled ? (y - top) / FF6_ROM_VALUE_BATTLE_MESSAGE_GRADIENT_PERIOD_0 : 0)) * 1057);
    }
    scene.layers.push_back(std::move(window));
    RasterSprite text;
    // Original strip origins/extents come from the tile-row writers. Convert
    // two-byte cells to eight-pixel columns without embedding selected layout.
    text.x = dialogue ? FF6_ROM_VALUE_BATTLE_DIALOGUE_TEXT_COLUMN_0 * 8 : FF6_ROM_VALUE_BATTLE_ANNOUNCEMENT_TEXT_COLUMN_0 * 8;
    if (narrow) {
        unsigned width = 0;
        for (auto glyph : message.glyphs)
            width += text_assets.widths[wrap8(unsigned{glyph} - 96U)];
        require(width <= FF6_ROM_VALUE_BATTLE_ATTACK_NAME_CAPACITY_0, "Battle ability name exceeds narrow window");
        // C1/9887 centers the proportional text within the narrow message.
        text.x = FF6_ROM_VALUE_BATTLE_ATTACK_NAME_CENTER_0 - static_cast<int>(width / 2);
    }
    text.y = dialogue ? FF6_ROM_VALUE_BATTLE_DIALOGUE_TEXT_ORIGIN_0 : FF6_ROM_VALUE_BATTLE_MESSAGE_INK_ORIGIN_0 - 1;
    text.width = narrow ? FF6_ROM_VALUE_BATTLE_ATTACK_NAME_CAPACITY_0 : dialogue ? FF6_ROM_VALUE_BATTLE_MESSAGE_TEXT_WIDTHS_0 * 4 : FF6_ROM_VALUE_BATTLE_MESSAGE_TEXT_WIDTHS_1 * 4;
    // Interior height excludes both frame cells. Each logical text row uses
    // two tiles; the last row needs 11 glyph rows, shadow and one safety row.
    text.height = dialogue ? (FF6_ROM_VALUE_BATTLE_MESSAGE_WINDOW_SIZES_3 - 2) * 8 - 16 + 13 : 13;
    text.depth = 21;
    text.pixels.resize(text.width * text.height);
    text.colors[1] = readout.font_color;
    text.colors[2] = 0;
    unsigned cursor = 0, row = 0;
    for (const auto c : message.glyphs) {
        if (dialogue && c == 1) {
            row += 16; cursor = 0; continue;
        }
        require(row + 12 < text.height, "Battle dialogue exceeds three lines");
        const unsigned glyph = wrap8(unsigned{c} - 128U);
        require(glyph < text_assets.glyphs.size(), "Unowned battle message glyph");
        for (unsigned y = 0; y < 11; ++y)
            for (unsigned x = 0; x < 16; ++x)
                if (text_assets.glyphs[glyph][y] & (1U << (15U - x))) {
                    const auto dx = static_cast<int>(cursor + x) - 4;
                    if (dx >= 0 && static_cast<unsigned>(dx + 1) < text.width) {
                        auto& shadow = text.pixels[(row + y + 1) * text.width + static_cast<unsigned>(dx + 1)];
                        if (!shadow) shadow = 2;
                        text.pixels[(row + y) * text.width + static_cast<unsigned>(dx)] = 1;
                    }
                }
        cursor += text_assets.widths[wrap8(unsigned{c} - 96U)];
    }
    // The original publishes 26 dialogue tiles, while the proportional cursor
    // can extend beyond them (including authored tutorial text). Clip the ink
    // to that surface; a line's advance width is not malformed script data.
    if (!dialogue)
        require(cursor <= text.width, "Battle message exceeds its window");
    require(scroll <= 16, "Invalid message scroll displacement");
    if (scroll) {
        for (unsigned y = 0; y < text.height; ++y)
            for (unsigned x = 0; x < text.width; ++x)
                text.pixels[y * text.width + x] =
                    y + scroll < text.height ? text.pixels[(y + scroll) * text.width + x] : 0;
    }
    for (unsigned part = 0; part * 32 < text.width; ++part) {
        auto piece = text;
        piece.x += static_cast<int>(part * 32);
        piece.width = static_cast<std::uint16_t>(std::min(32U, text.width - part * 32));
        piece.pixels.resize(piece.width * piece.height);
        for (unsigned y = 0; y < piece.height; ++y)
            std::copy_n(text.pixels.begin() + static_cast<std::ptrdiff_t>(y * text.width + part * 32),
                        piece.width, piece.pixels.begin() + static_cast<std::ptrdiff_t>(y * piece.width));
        scene.objects->sprites.insert(scene.objects->sprites.begin(), std::move(piece));
    }
}
} // namespace ff6
