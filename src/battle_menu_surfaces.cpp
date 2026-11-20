#include "ff6/battle_menu_surfaces.hpp"
#include "rom-materials/battle_window_color_arithmetic.macros.inc"
#include "rom-materials/battle_window_gradient_geometry.macros.inc"
#include "rom-materials/battle_window_gradient_period.macros.inc"
#include "rom-materials/battle_mp_cell_bytes.macros.inc"
#include "rom-materials/battle_stance_columns.macros.inc"
#include "rom-materials/battle_name_normal_palette.macros.inc"
#include "rom-materials/battle_name_selected_palette.macros.inc"
#include "rom-materials/battle_bushido_known_palette.macros.inc"
#include "rom-materials/battle_bushido_unknown_palette.macros.inc"
#include "rom-materials/battle_bushido_selected_palette.macros.inc"
#include "rom-materials/battle_bushido_gauge_palette.macros.inc"
#include "rom-materials/battle_lore_text_flags.macros.inc"
#include "rom-materials/battle_equipment_text_flags.macros.inc"
#include "rom-materials/battle_lore_name_start.macros.inc"
#include "rom-materials/battle_text_initial_flags.macros.inc"
#include "rom-materials/battle_bushido_cell_bytes.macros.inc"
#include "rom-materials/battle_equipment_name_columns.macros.inc"
#include "rom-materials/battle_text_layout.macros.inc"
#include "rom-materials/battle_readout_window_rows.macros.inc"
#include "rom-materials/battle_command_first_band.macros.inc"
#include "rom-materials/battle_list_first_band.macros.inc"
#include "rom-materials/battle_esper_scroll.macros.inc"
#include "rom-materials/battle_equipment_scroll.macros.inc"
#include "rom-materials/battle_list_bands.macros.inc"
#include "rom-materials/battle_list_scroll.macros.inc"
#include "rom-materials/battle_list_end.macros.inc"
#include "rom-materials/battle_equipment_period.macros.inc"
#include "rom-materials/battle_readout_copy_region.macros.inc"
#include "rom-materials/battle_bushido_clear_region.macros.inc"
#include "rom-materials/battle_stance_clear_region.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
template <class E> std::size_t index(E value) {
    return static_cast<std::size_t>(value);
}
template <std::size_t N, std::size_t M>
void names(ByteView bytes, std::array<std::array<std::uint8_t, M>, N>& output) {
    require(bytes.size() == N * M, "Invalid battle menu name table extent");
    for (std::size_t i = 0; i < N; ++i)
        std::ranges::copy(bytes.subspan(i * M, M), output[i].begin());
}
const TileLayer& present(const std::optional<TileLayer>& layer) {
    require(layer.has_value(), "Battle menu surface has not been prepared");
    return *layer;
}
// Change encoded operands before drawing, exactly where text flags apply. In
// particular, blanks before the next color command retain the preceding color.
Bytes choices(ByteView program, std::span<const std::uint8_t> identities,
              std::span<const bool> disabled, std::uint8_t name_command) {
    Bytes out(program.begin(), program.end());
    std::size_t at = 0, slot = 0;
    std::optional<std::size_t> flags;
    while (at < out.size()) {
        const auto command = out[at++];
        if (command == 0) {
            require(at == out.size() && slot == identities.size(),
                    "Invalid battle menu choice program");
            return out;
        }
        if (command >= 32 || command == 1 || command == 22)
            continue;
        require(at < out.size(), "Truncated battle menu choice operand");
        if (command == 4)
            flags = at;
        else if (command == name_command) {
            require(slot < identities.size() && flags.has_value(),
                    "Missing battle choice text flags");
            out[at] = identities[slot];
            if (!disabled.empty() && disabled[slot])
                out[*flags] |= 4;
            ++slot;
        } else
            require(command == 3 || command == 5, "Unsupported battle choice text command");
        ++at;
    }
    throw std::invalid_argument("Unterminated battle choice program");
}
void name_colors(TileLayer& text, BattleReadoutRectangle region, unsigned selected) {
    require(region.width == 7 && region.height == 8 && unsigned{region.x} + 7 <= 32 &&
                region.y == 0 && text.cells.size() == 256,
            "Invalid battle character-name region");
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 7; ++x)
            text.cells[y * 32 + region.x + x].palette = y / 2 == selected
                ? FF6_ROM_VALUE_BATTLE_NAME_SELECTED_PALETTE_0 : FF6_ROM_VALUE_BATTLE_NAME_NORMAL_PALETTE_0;
}
} // namespace
BattleMenuSurfaceAssets load_battle_menu_surface_assets(const ContentPack& pack) {
    BattleMenuSurfaceAssets a;
    a.readout = load_battle_readout_assets(pack);
    const auto slot_bytes = pack.get("menu_graphics_slot_gfx_4bpp");
    require(slot_bytes.size() == 4096, "Invalid battle slot graphics extent");
    // LoadMenuGfx transfers the first 2 KiB of the larger extracted family.
    const auto slots = decode_tiles(slot_bytes.first(2048), 4);
    std::ranges::copy(slots, a.slot_tiles.begin());
    a.magitek = load_battle_magitek_choices(pack);
    names(pack.get("text_item_name_bin"), a.names.items);
    names(pack.get("text_item_symbol_name_bin"), a.names.item_symbols);
    const auto magic_program = pack.get("battle_magic_text_program");
    const auto item_program = pack.get("battle_item_text_program");
    const auto columns = pack.get("battle_magic_item_cursor_columns");
    require(magic_program.size() == 15 && item_program.size() == 15 && columns.size() == 9,
            "Invalid Magic/Item menu data");
    a.magic_program.assign(magic_program.begin(), magic_program.end());
    a.item_program.assign(item_program.begin(), item_program.end());
    a.magic_columns = {columns[0], columns[1]};
    a.item_column = columns[7];
    const auto mp_glyphs = pack.get("battle_mp_glyphs"), mp_layout = pack.get("battle_mp_layout");
    require(mp_glyphs.size() == 21 && mp_layout.size() == 42, "Invalid MP readout tables");
    std::ranges::copy(mp_glyphs, a.mp_glyphs.begin());
    std::ranges::copy(mp_layout, a.mp_layout.begin());
    names(pack.get("text_battle_cmd_name_bin"), a.names.commands);
    names(pack.get("text_magic_name_bin"), a.names.magic);
    // MenuTextCmd_0f in the supported English ROM reads 108 bytes before
    // the esper-name table, using its eight-character stride. Preserve that
    // original bad-pointer quirk with a bounded view of already owned data.
    const auto magic_names = pack.get("text_magic_name_bin");
    const auto esper_names = pack.get("text_genju_name_bin");
    require(esper_names.size() == 216, "Invalid battle esper name table extent");
    Bytes esper_menu(magic_names.end() - 108, magic_names.end());
    append(esper_menu, esper_names.first(108));
    names(esper_menu, a.names.esper);
    names(esper_names, a.names.summon_names);
    const auto summon = pack.get("battle_summon_text_program");
    require(summon.size() == 23, "Invalid summon menu program");
    a.summon_program.assign(summon.begin(), summon.end());
    const auto equipment = pack.get("battle_equipment_header_program");
    require(equipment.size() == 19, "Invalid equipment header extent");
    a.equipment_program.assign(equipment.begin(), equipment.end());
    names(pack.get("text_attack_name_bin"), a.names.attacks);
    names(pack.get("text_dance_name_bin"), a.names.dances);
    const auto windows = pack.get("battle_menu_window_rectangles");
    const auto buffers = pack.get("battle_menu_text_buffers");
    const auto programs = pack.get("battle_command_text_programs");
    const auto magitek = pack.get("battle_magitek_text_program");
    require(windows.size() == 52 && buffers.size() == 140 && programs.size() == 103 &&
                magitek.size() == 13,
            "Invalid battle menu surface resource extent");
    for (unsigned i = 0; i < 13; ++i) {
        const unsigned base = i == 9 || i == 10 ? 0xa97f : 0x8d13;
        const auto address = read16(windows, i * 4 + 2);
        require(address >= base && (address - base) % 2 == 0,
                "Invalid battle menu rectangle address");
        const auto cell = (address - base) / 2;
        const auto w = windows[i * 4], h = windows[i * 4 + 1];
        require(cell < 256 && w >= 2 && h >= 2 && cell % 32 + w <= 32 && cell / 32 + h <= 8,
                "Battle menu rectangle exceeds canvas");
        a.windows[i] = {static_cast<std::uint8_t>(cell % 32), static_cast<std::uint8_t>(cell / 32),
                        w, h};
    }
    constexpr std::array<unsigned, 3> buffer_ids{4, 31, 32};
    constexpr std::array<unsigned, 3> offsets{71, 0, 39}, lengths{32, 39, 32};
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned p = buffer_ids[i] * 4;
        const auto address = read16(buffers, p);
        require(address >= 0x5855 && (address - 0x5855) % 2 == 0 && buffers[p + 2] == 32 &&
                    buffers[p + 3] == 0,
                "Invalid battle command text buffer");
        const auto cell = (address - 0x5855U) / 2;
        require(cell < 32, "Invalid battle command text origin");
        a.command_regions[i] = {static_cast<std::uint8_t>(cell), 0,
                                static_cast<std::uint8_t>(32 - cell), 8};
        a.command_programs[i].assign(programs.begin() + offsets[i],
                                     programs.begin() + offsets[i] + lengths[i]);
    }
    a.magitek_program.assign(magitek.begin(), magitek.end());
    return a;
}
BattleMenuSurfaces make_battle_menu_surfaces(const BattleMenuSurfaceAssets& a,
                                             const BattleReadoutView& view) {
    auto closed = make_battle_readout_layers(a.readout, view);
    BattleMenuSurfaces out;
    out.command_work = closed.text;
    out.windows[index(BattleWindowCanvas::slot)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::slot)];
    out.windows[index(BattleWindowCanvas::magic)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::magic)];
    out.windows[index(BattleWindowCanvas::closed)] = closed.windows;
    out.windows[index(BattleWindowCanvas::row_def)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::row_def)];
    out.windows[index(BattleWindowCanvas::bushido)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::bushido)];
    out.windows[index(BattleWindowCanvas::esper)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::esper)];
    out.windows[index(BattleWindowCanvas::equipment)] =
        make_battle_window_initialization(a, view, {}).panels[index(BattleWindowCanvas::equipment)];
    auto list = closed.windows;
    // The list frame covers every modified cell of the earlier slot frame.
    draw_battle_window(a.readout, a.windows[2], list);
    out.windows[index(BattleWindowCanvas::list)] = std::move(list);
    auto list_text = closed.text;
    list_text.cells.assign(256, RasterCell{255, 0, 10});
    out.text[index(BattleTextCanvas::list)] = std::move(list_text);
    out.text[index(BattleTextCanvas::readout)] = std::move(closed.text);
    return out;
}
void prepare_battle_command_surface(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                    const BattleCommandList& commands, BattleCommandLayout layout,
                                    std::uint8_t selected) {
    const auto kind = index(layout);
    require(kind < 3 && selected < 4, "Invalid battle command surface selection");
    auto readout = present(s.text[index(BattleTextCanvas::readout)]);
    auto text = s.command_work;
    require(text.columns == 32 && text.rows == 8 && text.cells.size() == 256,
            "Invalid retained battle command canvas");
    if (layout != BattleCommandLayout::short_form) {
        name_colors(readout, a.readout.text_regions[1], selected);
        const auto region = a.readout.text_regions[1];
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 7; ++x)
                text.cells[y * 32 + region.x + x] = readout.cells[y * 32 + region.x + x];
    }
    std::array<std::uint8_t, 4> ids;
    std::array<bool, 4> disabled{};
    for (unsigned i = 0; i < 4; ++i) {
        ids[i] = commands.slots[i] ? commands.slots[i]->identity : 255;
        disabled[i] = commands.slots[i] && commands.slots[i]->disabled;
    }
    const auto program = choices(a.command_programs[kind], ids, disabled,
                                 layout == BattleCommandLayout::control ? 17 : 13);
    draw_battle_text(a.readout, {}, a.command_regions[kind], program, text, &a.names);
    auto window = present(s.windows[index(BattleWindowCanvas::closed)]);
    const unsigned rectangle = layout == BattleCommandLayout::window    ? 0
                               : layout == BattleCommandLayout::control ? 12
                                                                        : 11;
    draw_battle_window(a.readout, a.windows[rectangle], window);
    s.command_work = text;
    s.windows[index(BattleWindowCanvas::command)] = std::move(window);
    s.text[index(BattleTextCanvas::command)] = std::move(text);
    s.text[index(BattleTextCanvas::readout)] = std::move(readout);
}
void prepare_battle_magitek_row(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                std::uint8_t row, std::uint8_t identity) {
    require(row < 4, "Invalid Magitek text row");
    const auto& entries = identity == 0 ? a.magitek.terra : a.magitek.other;
    const auto program =
        choices(a.magitek_program, std::span(entries).subspan(unsigned{row} * 2, 2), {}, 6);
    auto text = present(s.text[index(BattleTextCanvas::list)]);
    draw_battle_text(a.readout, {}, {0, static_cast<std::uint8_t>(row * 2), 32, 2}, program, text,
                     &a.names, BattleTextLanguage::list);
    s.text[index(BattleTextCanvas::list)] = std::move(text);
}
void prepare_battle_mp(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a, CombatVitals mp,
                       std::uint8_t cost) {
    auto layer = *s.text[index(BattleTextCanvas::list)];
    layer.cells.assign(256, RasterCell{255, 0, 10});
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned column = 0; column < 7; ++column) {
            const auto code = a.mp_layout[row * 7 + column];
            const auto glyph = code == 255 ? 255 : a.mp_glyphs.at(code - 8U);
            layer.cells[(row * 2 + (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 / 32)) * 32 +
                        (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 % 32) + column] = {static_cast<unsigned>(glyph), 0, 10};
        }
    const auto number = [&](unsigned value, unsigned row, unsigned column) {
        value = std::min(value, 999U);
        for (unsigned digit = 0; digit < 3; ++digit) {
            const auto divisor = digit == 0 ? 100U : digit == 1 ? 10U : 1U;
            layer.cells[row * 32 + column + digit].tile =
                value < divisor && digit < 2 ? 255 : a.mp_glyphs[(value / divisor) % 10];
        }
    };
    // C2/AB8A transfers the seven-cell current/slash/maximum readout and
    // the separate cost row. The slash follows three numeric cells.
    number(mp.current, (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 / 32), (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 % 32));
    number(mp.maximum, (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 / 32), (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_0 / 16 % 32) + 4);
    number(cost, (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_1 / 16 / 32), (FF6_ROM_VALUE_BATTLE_MP_CELL_BYTES_1 / 16 % 32));
    s.mp = std::move(layer);
}
void prepare_battle_magic_row(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                              std::uint8_t row, const BattleSpellList& spells, unsigned first) {
    require(row < 4 && first + 2 <= spells.magic.size(), "Magic list row outside spell table");
    std::array<std::uint8_t, 2> ids{255, 255};
    std::array<bool, 2> disabled{};
    for (unsigned i = 0; i < 2; ++i)
        if (const auto& spell = spells.magic[first + i]) {
            ids[i] = spell->identity;
            disabled[i] = spell->disabled;
        }
    const auto program = choices(a.magic_program, ids, disabled, 15);
    draw_battle_text(a.readout, {}, {0, static_cast<std::uint8_t>(row * 2), 32, 2}, program,
                     *s.text[index(BattleTextCanvas::list)], &a.names, BattleTextLanguage::list);
}
void prepare_battle_lore_row(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                             std::uint8_t row, const std::optional<BattleSpell>& spell) {
    require(row < 4 && (!spell || spell->identity < 24), "Invalid Lore menu row");
    auto& canvas = *s.text[index(BattleTextCanvas::list)];
    for (unsigned y = row * 2; y < row * 2 + 2; ++y)
        for (unsigned x = 0; x < 32; ++x) canvas.cells[y * 32 + x] = {255, 0, 10};
    // English DrawLoreListText (C1/4D73) uses one ten-glyph name at column
    // three. Unknown skills retain their original blank positions, and the
    // shared attack-name decoder reads the already extracted name table.
    if (spell) {
        const Bytes label{4, static_cast<std::uint8_t>(spell->disabled ? (FF6_ROM_VALUE_BATTLE_LORE_TEXT_FLAGS_0 | 4U)
                                                               : FF6_ROM_VALUE_BATTLE_LORE_TEXT_FLAGS_0),
                          15, static_cast<std::uint8_t>(139U + spell->identity), 0};
        draw_battle_text(a.readout, {}, {FF6_ROM_VALUE_BATTLE_LORE_NAME_START_0, static_cast<std::uint8_t>(row * 2), 10, 2},
                        label, canvas, &a.names, BattleTextLanguage::list);
    }
}
void prepare_battle_summon_surface(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                   const BattleSpell& spell) {
    auto program = a.summon_program;
    require(program.size() == 23, "Invalid summon text extent");
    program[12] = spell.identity;
    program[21] = spell.mp_cost;
    if (spell.disabled) program[10] |= 4;
    auto layer = *s.text[index(BattleTextCanvas::list)];
    layer.cells.assign(256, RasterCell{255, 0, 10});
    draw_battle_text(a.readout, {}, {0, 0, 32, 2}, program, layer, &a.names, BattleTextLanguage::list);
    s.text[index(BattleTextCanvas::esper)] = std::move(layer);
}
void prepare_battle_equipment_surface(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                       const std::array<BattleItemStack, 2>& hands) {
    auto layer = *s.text[index(BattleTextCanvas::list)];
    layer.cells.assign(256, RasterCell{255, 0, 10});
    // English DrawEquipListText C1/4BBA: white 13-cell names at columns 2/17.
    // The separately initialized hand labels precede the mutable item row.
    // Selected spacing/attributes are ROM-backed; glyph widths and text opcodes
    // describe the encoded name format. All labels stay in extracted data.
    draw_battle_text(a.readout, {}, {0, 0, 32, 2}, a.equipment_program, layer);
    for (unsigned hand = 0; hand < 2; ++hand) {
        const Bytes label{4, FF6_ROM_VALUE_BATTLE_EQUIPMENT_TEXT_FLAGS_0, 14, hands[hand].item ? hands[hand].item->identity : std::uint8_t{255}, 0};
        draw_battle_text(a.readout, {}, {static_cast<std::uint8_t>(FF6_ROM_VALUE_BATTLE_EQUIPMENT_NAME_COLUMNS_0 + hand * (FF6_ROM_VALUE_BATTLE_EQUIPMENT_NAME_COLUMNS_1 - FF6_ROM_VALUE_BATTLE_EQUIPMENT_NAME_COLUMNS_0)), 2, 13, 2},
                        label, layer, &a.names, BattleTextLanguage::list);
    }
    s.text[index(BattleTextCanvas::equipment)] = std::move(layer);
}
void prepare_battle_item_row(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                             std::uint8_t row, const BattleItemStack& stack) {
    require(row < 4 && a.item_program.size() == 15, "Invalid item list text program");
    auto program = a.item_program;
    program[5] = program[12] = stack.item ? stack.item->identity : 255;
    program[8] = stack.item ? stack.quantity : 0;
    if (!program[8])
        program[6] = 255;
    if (!stack.item || (stack.item->usage_flags & 128U))
        program[3] |= 4;
    draw_battle_text(a.readout, {}, {0, static_cast<std::uint8_t>(row * 2), 32, 2}, program,
                     *s.text[index(BattleTextCanvas::list)], &a.names, BattleTextLanguage::list);
}
void prepare_battle_stance_surface(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                   std::uint8_t row, bool short_form, bool defend) {
    require(row < 4, "Invalid Row/Defend text row");
    auto text = s.command_work;
    // C1/48A4,48CB copy HP/gauges and then the chosen seven-cell label over
    // the auxiliary command canvas. Names and unaffected command rows survive.
    const auto& readout = present(s.text[index(BattleTextCanvas::readout)]);
    // MenuTextPosTbl addresses the scratch base, but C1/48E7 publishes from
    // base + one row. HP's destination row 1 therefore becomes canvas row 0.
    for (unsigned y = 0; y < FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_2; ++y)
        for (unsigned x = FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_0; x < FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_1; ++x)
            text.cells[y * 32 + x] = readout.cells[y * 32 + x];
    const unsigned x = short_form
                           ? (defend ? FF6_ROM_VALUE_BATTLE_STANCE_COLUMNS_3
                                     : FF6_ROM_VALUE_BATTLE_STANCE_COLUMNS_2)
                           : (defend ? FF6_ROM_VALUE_BATTLE_STANCE_COLUMNS_1
                                     : FF6_ROM_VALUE_BATTLE_STANCE_COLUMNS_0);
    const unsigned y = short_form ? 0U : unsigned{row} * 2U;
    // The five-row label buffer includes a blank row BEFORE the text pair.
    // Omitting it leaves the preceding command's glyphs over the popup border.
    for (unsigned line = y ? y - 1 : 0; line < std::min(y + (FF6_ROM_VALUE_BATTLE_STANCE_CLEAR_REGION_1 - 1), 8U); ++line)
        for (unsigned column = x; column < x + FF6_ROM_VALUE_BATTLE_STANCE_CLEAR_REGION_0; ++column)
            text.cells[line * 32 + column] = {255, 0, 10};
    // Command names are ROM-owned content; no game text is embedded in source.
    Bytes label{4, FF6_ROM_VALUE_BATTLE_TEXT_INITIAL_FLAGS_0, 255, 255};
    const auto& name = a.names.commands[defend ? 21 : 20];
    label.insert(label.end(), name.begin(), name.begin() + 5);
    label.push_back(0);
    draw_battle_text(a.readout, {},
                     {static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), FF6_ROM_VALUE_BATTLE_STANCE_CLEAR_REGION_0, 2}, label,
                     text, &a.names);
    s.text[index(BattleTextCanvas::auxiliary)] = std::move(text);
}
void prepare_battle_bushido_surface(BattleMenuSurfaces& s, const BattleMenuSurfaceAssets& a,
                                    unsigned known, std::uint8_t counter) {
    require(known >= 1 && known <= 8 && (counter >> 5U) < known, "Invalid SwdTech charge");
    auto text = s.command_work;
    const auto& readout = present(s.text[index(BattleTextCanvas::readout)]);
    for (unsigned y = 0; y < FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_2; ++y)
        for (unsigned x = FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_0; x < FF6_ROM_VALUE_BATTLE_READOUT_COPY_REGION_1; ++x)
            text.cells[y * 32 + x] = readout.cells[y * 32 + x];
    // C1/4872: a 12x6 text buffer, destination (2,1), published one row
    // later. C1/7D25 changes numeral palettes and two four-cell gauge halves.
    for (unsigned y = 0; y < FF6_ROM_VALUE_BATTLE_BUSHIDO_CLEAR_REGION_2; ++y)
        for (unsigned x = FF6_ROM_VALUE_BATTLE_BUSHIDO_CLEAR_REGION_0; x < FF6_ROM_VALUE_BATTLE_BUSHIDO_CLEAR_REGION_1; ++x) text.cells[y * 32 + x] = {255, 0, 10};
    for (unsigned i = 0; i < 8; ++i) {
        text.cells[FF6_ROM_VALUE_BATTLE_BUSHIDO_CELL_BYTES_0 / 16 + i] = {181 + i, static_cast<unsigned>(i <= (counter >> 5U) ? FF6_ROM_VALUE_BATTLE_BUSHIDO_SELECTED_PALETTE_0
                                                  : i < known ? FF6_ROM_VALUE_BATTLE_BUSHIDO_KNOWN_PALETTE_0
                                                              : FF6_ROM_VALUE_BATTLE_BUSHIDO_UNKNOWN_PALETTE_0), 10};
        const int filled = int{counter} / 4 + 1;
        text.cells[FF6_ROM_VALUE_BATTLE_BUSHIDO_CELL_BYTES_1 / 16 + i] = {
            static_cast<unsigned>(240 + std::clamp(filled - static_cast<int>(i * 8), 0, 8)), FF6_ROM_VALUE_BATTLE_BUSHIDO_GAUGE_PALETTE_0, 10};
    }
    text.cells[FF6_ROM_VALUE_BATTLE_BUSHIDO_CELL_BYTES_1 / 16 - 1] = {249, FF6_ROM_VALUE_BATTLE_BUSHIDO_GAUGE_PALETTE_0, 10};
    text.cells[FF6_ROM_VALUE_BATTLE_BUSHIDO_CELL_BYTES_1 / 16 + 8] = {250, FF6_ROM_VALUE_BATTLE_BUSHIDO_GAUGE_PALETTE_0, 10};
    s.text[index(BattleTextCanvas::auxiliary)] = std::move(text);
    (void)a;
}
BattleMenuTransition make_battle_command_transition() {
    BattleMenuTransition s;
    // Unprepared menu families must fail if a caller tries to present them.
    // A default readout binding would otherwise look like plausible wrong text.
    for (auto& bank : s.text_banks)
        for (auto& line : bank)
            line.canvas = BattleTextCanvas::auxiliary;
    for (unsigned y = 0; y < 224; ++y) {
        // The original samples a blank row outside its four visible text bands.
        s.text[y].offset_y = static_cast<std::int16_t>(-2 - static_cast<int>(y));
        if (y >= FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1 && y < FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_0)
            s.windows[y] = {BattleWindowCanvas::closed, 0, -FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1};
        if (y >= FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_1 && y < FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_0)
            s.text[y].offset_y =
                static_cast<std::int16_t>(static_cast<int>((y - FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_1) / FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_2 * FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_3) - FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_4);
    }
    // Shared display bands come from line_init_long, as in the readout.
    // Binding indices are relative to its extracted window origin.
    auto& command = s.text_banks[index(BattleTextBank::command)];
    auto& list = s.text_banks[index(BattleTextBank::list)];
    for (unsigned i = 0; i < 64; ++i) {
        command[i] = s.text[FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1 + i];
        if (i >= FF6_ROM_VALUE_BATTLE_COMMAND_FIRST_BAND_0 && i < (FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_0 - FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1))
            command[i].canvas = BattleTextCanvas::command;
        const auto band = i >= FF6_ROM_VALUE_BATTLE_LIST_FIRST_BAND_0
                              ? (i - FF6_ROM_VALUE_BATTLE_LIST_FIRST_BAND_0) / FF6_ROM_VALUE_BATTLE_LIST_BANDS_0 : 0;
        list[i] =
            i < FF6_ROM_VALUE_BATTLE_LIST_END_0 ? BattleTextBinding{BattleTextCanvas::list, 0,
                                       static_cast<std::int16_t>(static_cast<int>(band * FF6_ROM_VALUE_BATTLE_LIST_BANDS_1) - FF6_ROM_VALUE_BATTLE_LIST_SCROLL_0)}
                   : BattleTextBinding{BattleTextCanvas::readout, 0, 1};
    }
    auto& esper = s.text_banks[index(BattleTextBank::esper)];
    for (unsigned i = 0; i < esper.size(); ++i)
        esper[i] = {BattleTextCanvas::esper, 0, -FF6_ROM_VALUE_BATTLE_ESPER_SCROLL_0};
    auto& equipment = s.text_banks[index(BattleTextBank::equipment)];
    // C2/B810: header and item rows occupy distinct twelve-pixel bands.
    // Convert their original scroll placement into the local four-row canvas.
    for (unsigned i = 0; i < equipment.size(); ++i)
        equipment[i] = {BattleTextCanvas::equipment, 0, static_cast<std::int16_t>(i < FF6_ROM_VALUE_BATTLE_EQUIPMENT_PERIOD_0 ? -FF6_ROM_VALUE_BATTLE_EQUIPMENT_SCROLL_0 : -FF6_ROM_VALUE_BATTLE_EQUIPMENT_SCROLL_1)};
    auto& stance = s.text_banks[index(BattleTextBank::row_def)];
    for (unsigned i = 0; i < stance.size(); ++i) {
        stance[i] = s.text[FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1 + i];
        if (i >= FF6_ROM_VALUE_BATTLE_COMMAND_FIRST_BAND_0 && i < (FF6_ROM_VALUE_BATTLE_TEXT_LAYOUT_0 - FF6_ROM_VALUE_BATTLE_READOUT_WINDOW_ROWS_1))
            stance[i].canvas = BattleTextCanvas::auxiliary;
    }
    return s;
}
std::vector<TileLayer> make_battle_menu_layers(const BattleMenuSurfaces& s,
                                               const BattleMenuTransition& t) {
    std::vector<TileLayer> out;
    const auto project = [&](const auto& bindings, const auto& canvases, bool window) {
        for (std::size_t i = 0; i < canvases.size(); ++i) {
            std::optional<TileLayer> active;
            for (unsigned y = 0; y < bindings.size(); ++y) {
                std::optional<std::size_t> selected;
                if constexpr (requires { bindings[y].canvas.has_value(); }) {
                    if (bindings[y].canvas)
                        selected = index(*bindings[y].canvas);
                } else
                    selected = index(bindings[y].canvas);
                require(!selected || *selected < canvases.size(), "Invalid battle surface binding");
                if (selected != i)
                    continue;
                if (!active) {
                    active = present(canvases[i]);
                    active->lines.assign(224, RasterLine{});
                    for (auto& line : active->lines)
                        line.enabled = false;
                }
                auto& line = active->lines[y];
                line.enabled = true;
                line.offset_x = bindings[y].offset_x;
                line.offset_y = bindings[y].offset_y;
                if (window) {
                    line.blend = static_cast<ColorBlend>(FF6_ROM_VALUE_BATTLE_WINDOW_COLOR_ARITHMETIC_0);
                    line.operand = static_cast<Rgb555>((y >= FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0 ? (y - FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_GEOMETRY_0) / FF6_ROM_VALUE_BATTLE_WINDOW_GRADIENT_PERIOD_0 : 0) * 1057);
                }
            }
            if (active)
                out.push_back(std::move(*active));
        }
    };
    project(t.windows, s.windows, true);
    // Row/Defend uses the same brightened skin and lower-menu gradient in
    // foreground priority. Other foreground canvases still require preparation.
    require(std::ranges::all_of(t.foreground,
                                [](const auto& b) {
                                    return !b.canvas || *b.canvas == BattleWindowCanvas::row_def ||
                                           *b.canvas == BattleWindowCanvas::bushido ||
                                           *b.canvas == BattleWindowCanvas::esper;
                                }),
            "Unowned foreground menu palette");
    project(t.foreground, s.windows, true);
    project(t.text, s.text, false);
    if (s.mp) {
        auto mp = *s.mp;
        mp.lines.assign(224, RasterLine{});
        bool shown = false;
        for (unsigned y = 0; y < 224; ++y) {
            auto& line = mp.lines[y];
            line.enabled = t.windows[y].canvas == BattleWindowCanvas::magic;
            if (line.enabled) {
                shown = true;
                line.offset_x = t.windows[y].offset_x;
                line.offset_y = t.windows[y].offset_y;
            }
        }
        if (shown)
            out.push_back(std::move(mp));
    }
    return out;
}
} // namespace ff6
