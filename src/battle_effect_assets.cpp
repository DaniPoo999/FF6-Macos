#include "rom-materials/effect_character_image_scripts.macros.inc"
#include "rom-materials/effect_common_frame_last.macros.inc"
#include "rom-materials/effect_summon_sprite_bytes.macros.inc"
#include "rom-materials/effect_summon_background_bytes.macros.inc"
#include "rom-materials/effect_summon_wait_base.macros.inc"
#include "rom-materials/effect_supplemental_image_tiles.macros.inc"
#include "ff6/battle_effects.hpp"
#include <stdexcept>
#include <algorithm>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
std::vector<std::uint16_t> words(ByteView data) {
    require(data.size() % 2 == 0, "Partial animation directory word");
    std::vector<std::uint16_t> out;
    for (std::size_t i = 0; i < data.size(); i += 2)
        out.push_back(read16(data, i));
    return out;
}
std::vector<Tile> three_bit_tiles(ByteView data) {
    require(data.size() % 24 == 0, "Partial three-bit attack tile");
    Bytes expanded(data.size() / 24 * 32);
    for (std::size_t i = 0; i < data.size() / 24; ++i) {
        for (unsigned j = 0; j < 16; ++j)
            expanded[i * 32 + j] = data[i * 24 + j];
        for (unsigned j = 0; j < 8; ++j)
            expanded[i * 32 + 16 + j * 2] = data[i * 24 + 16 + j];
    }
    return decode_tiles(expanded, 4);
}
} // namespace
std::array<BattleEffectSharedFrame, 7> decode_battle_effect_shared_frames(ByteView data) {
    require(data.size() == 42, "Invalid shared animation frame directory");
    std::array<BattleEffectSharedFrame, 7> result;
    for (unsigned i = 0; i < result.size(); ++i) {
        const auto normal = read16(data, i * 6 + 2), flipped = read16(data, i * 6 + 4);
        require(normal >= 0xce3f && flipped >= 0xd8bf && normal - 0xce3f == flipped - 0xd8bf &&
                    (normal - 0xce3f) % 84 == 0 && (normal - 0xce3f) / 84 < 31,
                "Shared animation frame points outside its frame directory");
        result[i] = {read16(data, i * 6), static_cast<std::uint8_t>((normal - 0xce3f) / 84)};
    }
    return result;
}
BattleEffectDefinition decode_battle_effect_definition(ByteView data) {
    require(data.size() == 14, "Animation property must contain fourteen bytes");
    BattleEffectDefinition out;
    for (unsigned i = 0; i < 4; ++i) {
        const auto value = read16(data, i == 3 ? 11 : i * 2);
        if (value != 0xffff)
            out.layers[i] = BattleEffectSelection{static_cast<std::uint16_t>(value & 0x7fffU),
                                                  (value & 0x8000U) == 0};
    }
    std::copy_n(data.begin() + 6, 3, out.palettes.begin());
    out.sound = data[9];
    out.initialization = data[10];
    out.target_delay = data[13];
    return out;
}
BattleEffectGraphics decode_battle_effect_graphics(ByteView data) {
    require(data.size() == 6, "Animation graphics property must contain six bytes");
    return {static_cast<std::uint16_t>(data[1] | ((data[0] & 64U) << 2U)),
            read16(data, 2),
            static_cast<std::uint8_t>(data[0] & 63U),
            data[4],
            data[5],
            (data[0] & 128U) != 0};
}
std::vector<BattleEffectPiece> decode_battle_effect_frame(ByteView data) {
    std::vector<BattleEffectPiece> out;
    require(data.size() % 2 == 0, "Partial animation frame piece");
    for (std::size_t at = 0; at < data.size(); at += 2) {
        const auto xy = data[at], tile = data[at + 1];
        if (xy == 255 && tile == 255)
            break;
        out.push_back({static_cast<std::uint8_t>(xy & 240U),
                       static_cast<std::uint8_t>((xy & 15U) * 16U),
                       static_cast<std::uint8_t>((tile & 7U) * 2U + (tile & 56U) * 4U),
                       (tile & 64U) != 0, (tile & 128U) != 0});
    }
    return out;
}
std::array<BattleMiscMonsterAnimation, 14> decode_battle_misc_monster_animations(ByteView bytes) {
    std::array<BattleMiscMonsterAnimation, 14> result{};
    require(bytes.size() == 106, "Invalid misc. monster animation data size");
    for (unsigned i = 0; i < result.size(); ++i) {
        const auto pointer = read16(bytes, i * 2);
        require(pointer >= 0xe5d3 && pointer - 0xe5d3U + 6 <= bytes.size(),
                "Invalid misc. monster animation pointer");
        const auto at = pointer - 0xe5d3U;
        require(bytes[at] == 0 && (bytes[at + 1] == 4 || bytes[at + 1] == 5) &&
                    bytes[at + 4] == 1 && bytes[at + 5] == (bytes[at + 1] == 5 ? 254 : 255) &&
                    read16(bytes, at + 2) % 14 == 0,
                "Invalid misc. monster animation program");
        result[i] = {static_cast<std::uint16_t>(read16(bytes, at + 2) / 14), bytes[at + 1] == 5};
    }
    return result;
}
BattleEffectLibrary load_battle_effects(const ContentPack& pack) {
    BattleEffectLibrary out;
    out.affine_tiles = decode_tiles(decompress_lzss(
        pack.get("attack_graphics_attack_gfx_mode7_4bpp_lz"), 65536), 4);
    out.affine_layouts = decompress_lzss(pack.get("attack_graphics_attack_tiles_mode7_scr_lz"), 65536);
    auto& blocks = out.blocks.emplace();
    const auto selections = pack.get("battle_block_selections");
    const auto block_colors = pack.get("battle_block_palettes");
    require(selections.size() == 21 && block_colors.size() == 64, "Invalid block presentation tables");
    for (unsigned i = 0; i < 7; ++i) {
        require(selections[i + 7] < 4, "Invalid block palette selection");
        blocks.selections[i] = {selections[i], selections[i + 7], selections[i + 14]};
    }
    for (unsigned bank = 0; bank < 4; ++bank)
        for (unsigned color = 0; color < 8; ++color)
            blocks.palettes[bank][color] = read16(block_colors, bank * 16 + color * 2) & 0x7fffU;
    // InitBattleGfx expands StatusGfx; Fight takes its 32-tile block image.
    // These are full four-bit pixels, including the upper palette-bank bit.
    const auto status = decompress_lzss(pack.get("menu_graphics_status_gfx_4bpp_lz"), 65536);
    blocks.tiles = decode_tiles(slice(status, 0x1000, 0x400), 4);
    blocks.status_tiles = decode_tiles(slice(status, 0x1400, 0x400), 4);
    const auto event_espers = pack.get("battle_event_esper_images");
    require(event_espers.size() == 10, "Invalid event Esper image table");
    for (unsigned i = 0; i < out.event_esper_images.size(); ++i) {
        const auto image = read16(event_espers, i * 2U);
        require(image >= 384 && image < 384 + out.summons.size(), "Invalid event Esper image");
        out.event_esper_images[i] = static_cast<std::uint8_t>(image - 384U);
    }
    const auto shake = pack.get("battle_screen_shake");
    require(shake.size() == out.screen_shake.size(), "Invalid battle screen shake table");
    std::ranges::copy(shake, out.screen_shake.begin());
    const auto event_colors = pack.get("battle_event_palettes");
    require(event_colors.size() == 160, "Invalid battle event palettes");
    for (unsigned bank = 0; bank < 5; ++bank) {
        std::array<Rgb555, 16> colors{};
        for (unsigned i = 0; i < 16; ++i) colors[i] = read16(event_colors, bank * 32 + i * 2);
        out.event_palettes.push_back(colors);
    }
    out.gradients = decode_battle_effect_gradients(pack.get("battle_effect_gradients"));
    const auto lighting = pack.get("battle_chadarnook_lighting");
    require(lighting.size() == 192, "Invalid monster-transition lighting palettes");
    out.transition_lighting.emplace();
    for (unsigned mode = 0; mode < 2; ++mode)
        for (unsigned bank = 0; bank < 3; ++bank)
            for (unsigned i = 0; i < 16; ++i)
                (*out.transition_lighting)[mode][bank][i] = read16(lighting, (mode * 48 + bank * 16 + i) * 2) & 0x7fffU;
    const auto copies = pack.get("battle_effect_target_copies");
    require(copies.size() == 14, "Invalid target-dependent animation task counts");
    out.target_copies.emplace();
    for (unsigned i = 0; i < 7; ++i) {
        require(copies[i] < 8 && copies[i] + 1 == copies[i + 7],
                "Inconsistent target-dependent animation task counts");
        (*out.target_copies)[i] = copies[i];
    }
    out.shared_frames = decode_battle_effect_shared_frames(pack.get("battle_effect_shared_frames"));
    const auto curve = pack.get("battle_effect_curved_scroll_repeats");
    require(curve.size() == 54, "Invalid curved scroll repeat table");
    out.curved_scroll_repeats.emplace();
    for (unsigned i = 0; i < curve.size(); ++i) {
        require(curve[i] != 0, "Curved scroll repeat must advance");
        (*out.curved_scroll_repeats)[i] = curve[i];
    }
    out.monster_misc = decode_battle_misc_monster_animations(pack.get("battle_monster_misc_programs"));
    const auto flashes = pack.get("battle_monster_flash_delays");
    require(flashes.size() == 64 && std::ranges::none_of(flashes, [](auto v) { return v == 0; }),
            "Invalid monster flash delays");
    out.monster_flash_delays.emplace();
    std::ranges::copy(flashes, out.monster_flash_delays->begin());
    const auto masks = pack.get("battle_monster_dissolve_masks");
    require(masks.size() == 112, "Invalid monster dissolve masks");
    out.monster_dissolve_masks.emplace();
    for (unsigned i = 0; i < 56; ++i) (*out.monster_dissolve_masks)[i] = read16(masks, i * 2);

    const auto monsters = pack.get("monster_tables_properties");
    const auto summon_palettes = pack.get("monster_graphics_monster_pal_pal");
    // Tiger Break deliberately uses character palette zero instead of the
    // monster/summon palette stored with its stencil (C2/EBCB).
    const auto character_palettes = pack.get("interface_palettes_battle_char_pal_pal");
    require(character_palettes.size() == 224, "Invalid character palette extent");
    out.tigerbreak_palette.emplace();
    for (unsigned color = 0; color < 16; ++color)
        (*out.tigerbreak_palette)[color] = read16(character_palettes, color * 2) & 0x7fffU;
    out.sketch_monsters.reserve(384);
    for (unsigned i = 0; i < 384; ++i)
        out.sketch_monsters.push_back(decode_monster_graphic(slice(monsters, i * 5, 5),
            pack.get("monster_graphics_monster_gfx_stc"),
            pack.get("monster_graphics_monster_stencil_small_stn"),
            pack.get("monster_graphics_monster_stencil_large_stn"), summon_palettes));
    for (unsigned i = 0; i < out.summons.size(); ++i) {
        out.summons[i] = decode_monster_graphic(
            slice(monsters, (384 + i) * 5, 5), pack.get("monster_graphics_monster_gfx_stc"),
            pack.get("monster_graphics_monster_stencil_small_stn"),
            pack.get("monster_graphics_monster_stencil_large_stn"),
            pack.get("monster_graphics_monster_pal_pal"));
        const auto next = std::size_t{out.summons[i].palette_identity} * 16 + 32;
        if (next + 32 <= summon_palettes.size()) {
            auto& colors = out.summon_following_colors[i].emplace();
            for (unsigned c = 0; c < 16; ++c)
                colors[c] = read16(summon_palettes, next + c * 2) & 0x7fffU;
        }
    }
    const auto character_motion = load_battle_character_motion(pack);
    out.character_walking = character_motion.walking;
    out.character_jumping = character_motion.jumping;
    const auto sine = pack.get("data_c2fc6d_btlgfx_lookup_tables"),
               random = pack.get("encounter_rng_tbl_bin");
    require(sine.size() == 768 && random.size() == 256, "Invalid animation arithmetic tables");
    std::copy_n(sine.begin() + 512, 256, out.sine.begin());
    for (unsigned i = 0; i < 256; ++i)
        out.sine16[i] = read16(sine, i * 2);
    std::copy(random.begin(), random.end(), out.random.begin());
    const auto angles = pack.get("battle_effect_angles");
    const auto deltas = decompress_lzss(pack.get("battle_effect_distances_lz"), 1024);
    require(angles.size() == 1024 && deltas.size() == 1024, "Invalid animation vector tables");
    std::copy(angles.begin(), angles.end(), out.angles.begin());
    // Each 32-entry row restarts its cumulative distance independently.
    for (unsigned row = 0; row < 32; ++row) {
        std::uint16_t distance = 0;
        for (unsigned column = 0; column < 32; ++column) {
            const auto at = row * 32 + column;
            distance = wrap16(unsigned{distance} + deltas[at]);
            out.distances[at] = distance;
        }
    }
    auto data = pack.get("data_d00000_attack_animation_scripts");
    out.programs.assign(data.begin(), data.end());
    data = pack.get("attack_frames_attack_anim_frames_bin");
    out.frame_data.assign(data.begin(), data.end());
    out.entries = words(pack.get("ptr_table_d1ead8_attack_animation"));
    out.frame_offsets = words(pack.get("attack_frames_attack_anim_frames_ptr_bin"));
    // Frame pointers are bank-local references to this separately extracted
    // data resource. Convert once; the runtime never addresses a ROM image.
    for (auto& offset : out.frame_offsets) {
        require(offset >= 0x141 && offset - 0x141U <= out.frame_data.size(),
                "Animation frame pointer leaves its resource");
        offset = static_cast<std::uint16_t>(offset - 0x141U);
    }
    out.tiles_two = words(pack.get("attack_graphics_attack_tiles_2bpp_scr"));
    out.tiles_three = words(pack.get("attack_graphics_attack_tiles_3bpp_scr"));
    out.pixels_two = decode_tiles(pack.get("attack_graphics_attack_gfx_2bpp_2bpp"), 2);
    out.pixels_three = three_bit_tiles(pack.get("attack_graphics_attack_gfx_3bpp_3bpp"));
    data = pack.get("attack_graphics_attack_pal_pal");
    require(data.size() % 16 == 0, "Partial attack palette");
    for (std::size_t at = 0; at < data.size(); at += 16) {
        std::array<Rgb555, 8> colors{};
        for (unsigned i = 0; i < 8; ++i)
            colors[i] = read16(data, at + 2 * i) & 0x7fffU;
        out.palettes.push_back(colors);
    }
    const auto tail = pack.get("battle_effect_tail_palette");
    require(tail.size() == 16, "Invalid animation tail palette extent");
    out.tail_palette.emplace();
    for (unsigned i = 0; i < 8; ++i)
        (*out.tail_palette)[i] = read16(tail, i * 2) & 0x7fffU;
    const auto rainbow = pack.get("battle_effect_rainbow_palettes");
    require(rainbow.size() == 8, "Invalid rainbow palette selection table");
    out.rainbow_palettes.emplace();
    for (unsigned i = 0; i < rainbow.size(); ++i) {
        (void)battle_effect_palette(out, rainbow[i]);
        (*out.rainbow_palettes)[i] = rainbow[i];
    }
    const auto random_palettes = pack.get("battle_effect_random_sprite_palettes");
    require(random_palettes.size() == 8, "Invalid random sprite palette directory");
    out.random_sprite_palettes.emplace();
    for (unsigned i = 0; i < random_palettes.size(); ++i) {
        (void)battle_effect_palette(out, random_palettes[i]);
        (*out.random_sprite_palettes)[i] = random_palettes[i];
    }
    data = pack.get("battle_fight_step_motion");
    require(data.size() == 16, "Invalid Fight step motion extent");
    out.fight_step_y.emplace();
    for (unsigned i = 0; i < 8; ++i) {
        require(data[i] < 32 && (data[i + 8] == 0xc0 || data[i + 8] == 0x20),
                "Invalid Fight step direction");
        (*out.fight_step_y)[i] = static_cast<std::int16_t>(
            data[i + 8] == 0xc0 ? -int{data[i]} : int{data[i]});
    }
    data = pack.get("data_ece400_weapon_anim_prop");
    require(data.size() == 1024, "Invalid weapon animation properties");
    for (std::size_t at = 0; at < data.size(); at += 8) {
        std::array<std::uint8_t, 8> property{};
        std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(at), 8, property.begin());
        out.weapon_properties.push_back(property);
    }
    data = pack.get("data_d07fb2_attack_anim_prop");
    require(data.size() % 14 == 0, "Partial attack property");
    for (std::size_t at = 0; at < data.size(); at += 14)
        out.definitions.push_back(decode_battle_effect_definition(slice(data, at, 14)));
    data = pack.get("data_d4d000_attack_gfx_prop");
    require(data.size() % 6 == 0, "Partial attack graphics property");
    for (std::size_t at = 0; at < data.size(); at += 6)
        out.graphics.push_back(decode_battle_effect_graphics(slice(data, at, 6)));
    return out;
}
std::uint64_t battle_effect_image_preparation_work(const BattleEffectLibrary& library,
    BattleEffectGraphics graphics, BattleEffectLayer layer) {
    require(layer == BattleEffectLayer::sprite || layer == BattleEffectLayer::foreground ||
                layer == BattleEffectLayer::overlay, "Invalid preparation image layer");
    const bool two = graphics.two_bit || layer == BattleEffectLayer::overlay;
    const auto& directory = two ? library.tiles_two : library.tiles_three;
    const unsigned count = layer == BattleEffectLayer::foreground ? 160U : 128U;
    const auto first = std::size_t{graphics.tile_group} * 32U;
    // Some palette-only tasks nominate a transfer beyond the tile directory
    // but never display it (Mind Blast's hidden overlay). Such bytes have no
    // native image owner. Count the nominal load, and only inspect known tiles
    // for this lower bound; image sampling still rejects any unowned tile.
    // LoadAnimGfx3bpp/2bpp/BG3, through entry to WaitTfrVRAM. These are
    // resource-size formulas for visible preparation, not instruction replay.
    // Include pointer setup, tile traversal, final untaken branches and both
    // row loops. A 3-bit source has two eight-row loops; 2-bit BG1/sprites
    // have a conversion loop and a zero-padding loop. BG3 has no padding.
    // See BATTLE_BUSHIDO.md for the source derivation and independent checks.
    const bool overlay = layer == BattleEffectLayer::overlay;
    const unsigned tile_cycles = !two ? 1283U : overlay ? 626U : 763U;
    const unsigned setup_and_tail = !two ? 121U : overlay ? 124U : 126U;
    const unsigned rows = two ? 8U : 16U;
    std::uint64_t cycles = setup_and_tail + std::uint64_t{count} * tile_cycles;
    for (unsigned i = 0; i < count && first + i < directory.size(); ++i) {
        const auto entry = directory[first + i];
        if (entry & 0x4000U)
            // ApplyTileHFlip: 263 versus 22 source cycles for EACH row.
            // Reversal visits all sixteen bits even for symmetric pixels.
            cycles += rows * 241U;
        if (two && (entry & 0x8000U)) {
            // The reversed padded path increments its source pointer in each
            // of eight zero rows. The unpadded BG3 path only omits a jump.
            if (overlay) cycles -= 3U;
            else cycles += 56U;
        }
    }
    // The rare 3-bit source-bank carry adds work. Omitting it, display
    // service and slow accesses keeps this a lower bound, not a load duration.
    return cycles;
}
namespace {
std::vector<BattleEffectPiece> effect_frame(const BattleEffectLibrary& library, unsigned index) {
    require(index + 1 < library.frame_offsets.size(), "Attack frame index leaves directory");
    const auto first = library.frame_offsets[index], next = library.frame_offsets[index + 1];
    // Pointer aliases can run backwards. In that case the reader must reach
    // an explicit terminator before the resource ends.
    const auto end = next >= first ? std::size_t{next} : library.frame_data.size();
    require(first <= end && end <= library.frame_data.size(), "Invalid attack frame span");
    if (next < first) {
        bool terminated = false;
        for (std::size_t at = first; at + 1 < end; at += 2)
            if (library.frame_data[at] == 255 && library.frame_data[at + 1] == 255) {
                terminated = true;
                break;
            }
        require(terminated, "Aliased attack frame has no terminator");
    }
    return decode_battle_effect_frame(slice(library.frame_data, first, end - first));
}
std::uint64_t frame_preparation_cycles(const BattleEffectLibrary& library,
                                       BattleEffectGraphics graphics, bool sprite) {
    require(graphics.frames != 0, "Zero-frame attack graphics require a retained image owner");
    std::uint64_t pieces = 0;
    for (unsigned frame = 0; frame < graphics.frames; ++frame)
        pieces += effect_frame(library, unsigned{graphics.first_frame} + frame).size();
    // LoadSpriteAnimFrames builds both orientations; LoadBGAnimFrames builds
    // one. Count their ordinary piece loops and shortest frame-end path.
    // Explicit FF terminators and coordinate FF take longer, never shorter.
    return sprite ? 40U + 117U * graphics.frames + 186U * pieces
                  : 18U + 90U * graphics.frames + 160U * pieces;
}
unsigned whole_preparation_pictures(std::uint64_t cycles) {
    // Fastest accesses and an otherwise empty NTSC picture give a conservative
    // duration. Fractions can overlap the next explicit publication wait.
    return static_cast<unsigned>(cycles * 6U / (262U * 1364U));
}
} // namespace
unsigned battle_effect_image_preparation_waits(const BattleEffectLibrary& library,
    BattleEffectGraphics graphics, BattleEffectLayer layer) {
    return whole_preparation_pictures(battle_effect_image_preparation_work(library, graphics, layer));
}
unsigned battle_effect_preparation_waits(const BattleEffectLibrary& library,
    const BattleEffectDefinition& definition, bool repeated, bool message_cleanup) {
    // LoadAnimProp always clears 85 task records, even when a repeated hit
    // retains its converted pixels. Keep fractional work together ONLY until
    // the next explicit wait; work on either side cannot be rounded together.
    std::uint64_t cycles = 38089; // ClearThreadData: 85 * 16 iterations.
    unsigned pictures = 0;
    const auto publish = [&] {
        pictures += whole_preparation_pictures(cycles);
        cycles = 0;
    };
    for (unsigned layer = 0; layer < 3; ++layer) {
        if (layer == 1 && message_cleanup)
            publish(); // Text cleanup sits between sprite and BG1 loading.
        if (const auto selection = definition.layers[layer]) {
            require(selection->script < library.graphics.size(), "Unowned preparation graphics identity");
            const auto graphics = library.graphics[selection->script];
            if (!repeated) {
                cycles += battle_effect_image_preparation_work(library, graphics,
                    static_cast<BattleEffectLayer>(layer));
                publish(); // Converted image's explicit upload waits.
            }
            // Clearing and rebuilding layouts is outside the repeat guard.
            // Sprite: 1,344 paired stores; BG1/BG3: 1,024/640 four-way stores.
            constexpr std::array<std::uint64_t, 3> clear_cycles{29588, 34852, 21796};
            cycles += clear_cycles[layer] + frame_preparation_cycles(library, graphics, layer == 0);
        }
    }
    if (library.shared_frames)
        for (const unsigned common : {0U, unsigned{FF6_ROM_VALUE_EFFECT_COMMON_FRAME_LAST_0}}) {
            const auto script = library.shared_frames->at(common).script;
            require(script < library.graphics.size(), "Unowned common preparation graphics identity");
            cycles += frame_preparation_cycles(library, library.graphics[script], true);
        }
    publish(); // InitAnimThreads follows with its own explicit waits.
    return pictures;
}
namespace {
std::vector<Tile> effect_tiles(const BattleEffectLibrary& library, BattleEffectGraphics graphics,
                              BattleEffectLayer layer, std::vector<bool>* ownership) {
    const bool two = graphics.two_bit || layer == BattleEffectLayer::overlay;
    const auto& directory = two ? library.tiles_two : library.tiles_three;
    const auto& pixels = two ? library.pixels_two : library.pixels_three;
    const auto count = layer == BattleEffectLayer::foreground ? 160U : 128U;
    const auto first = std::size_t{graphics.tile_group} * 32;
    if (!ownership)
        require(first <= directory.size() && count <= directory.size() - first,
                "Attack tile group leaves its resource");
    else
        ownership->assign(count, false);
    std::vector<Tile> out(count);
    for (unsigned i = 0; i < count && first + i < directory.size(); ++i) {
        const auto entry = directory[first + i];
        require((entry & 0x3fffU) < pixels.size(), "Attack tile identity leaves its resource");
        if (ownership) (*ownership)[i] = true;
        const auto& tile = pixels[entry & 0x3fffU];
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x)
                out[i][y * 8 + x] =
                    tile[((entry & 0x8000U) ? 7 - y : y) * 8 + ((entry & 0x4000U) ? 7 - x : x)];
    }
    if (ownership && std::ranges::all_of(*ownership, [](bool owned) { return owned; }))
        ownership->clear(); // An empty mask denotes a fully owned image.
    return out;
}
} // namespace
std::vector<Tile> load_battle_effect_tiles(const BattleEffectLibrary& library,
                                           BattleEffectGraphics graphics, BattleEffectLayer layer) {
    return effect_tiles(library, graphics, layer, nullptr);
}
const std::array<Rgb555, 8>& battle_effect_palette(const BattleEffectLibrary& library,
                                                  std::uint8_t identity) {
    if (identity < library.palettes.size())
        return library.palettes[identity];
    require(identity == 255 && library.tail_palette, "Unowned attack palette identity");
    return *library.tail_palette;
}
BattleEffectImage load_battle_effect_image(const BattleEffectLibrary& library, std::uint16_t script,
                                           BattleEffectLayer layer, std::uint8_t palette) {
    require(script < library.graphics.size(), "Unowned attack graphics identity");
    const auto& colors = battle_effect_palette(library, palette);
    BattleEffectImage out;
    out.geometry = library.graphics[script];
    require(out.geometry.frames != 0, "Zero-frame attack graphics require a retained image owner");
    // LoadAnimProp selects the published character tiles for these shared
    // frame families. Their geometry still belongs to the animation resource.
    out.character_frames = layer == BattleEffectLayer::foreground &&
                           (script == FF6_ROM_VALUE_EFFECT_CHARACTER_IMAGE_SCRIPTS_0 ||
                            script == FF6_ROM_VALUE_EFFECT_CHARACTER_IMAGE_SCRIPTS_3 ||
                            script == FF6_ROM_VALUE_EFFECT_CHARACTER_IMAGE_SCRIPTS_1 ||
                            script == FF6_ROM_VALUE_EFFECT_CHARACTER_IMAGE_SCRIPTS_2);
    if (!out.character_frames)
        // Retain explicit ownership, not fabricated pixels for unused ROM
        // overreads. The renderer requires ownership for every sampled tile.
        out.tiles = effect_tiles(library, out.geometry, layer, &out.owned_tiles);
    for (unsigned frame = 0; frame < out.geometry.frames; ++frame) {
        const auto index = unsigned{out.geometry.first_frame} + frame;
        out.frames.push_back(effect_frame(library, index));
        out.frame_geometry.push_back(out.geometry);
    }
    // LoadBG1AnimFrames clears all sixteen frame surfaces before loading the
    // authored subset. Shared hit programs can deliberately run past a shorter
    // image's last frame; those remaining surfaces are owned, empty images.
    // Preserve this at image loading, rather than hiding arbitrary bad indices
    // in the renderer or making an exception for one monster/weapon.
    if (layer == BattleEffectLayer::foreground && out.frames.size() < 16) {
        out.frames.resize(16);
        out.frame_geometry.resize(16, out.geometry);
    }
    for (unsigned i = 0; i < 16; ++i)
        out.colors[i] = colors[i & 7U];
    out.base_colors = out.colors;
    return out;
}
void BattleAttackAnimation::load_summon(std::size_t identity, BattleEffectLayer layer) {
    const auto index = static_cast<std::size_t>(layer);
    require(identity < library_->summons.size() && index < 2 && images_[index].has_value(),
            "Summon graphics need an owned sprite or foreground frame set");
    load_monster_image(library_->summons[identity], layer);
    images_[index]->tiles.resize(index == 0 ?
        FF6_ROM_VALUE_EFFECT_SUMMON_SPRITE_BYTES_0 / 32U :
        FF6_ROM_VALUE_EFFECT_SUMMON_BACKGROUND_BYTES_0 / 32U);
    loaded_summon_ = identity;
}
void BattleAttackAnimation::load_monster_image(const MonsterGraphic& source, BattleEffectLayer layer) {
    const auto index = static_cast<std::size_t>(layer);
    require(index < 2 && images_[index], "Monster image needs an owned frame set");
    if (initializing_)
        loading_waits_ += index == 0 ? FF6_ROM_VALUE_EFFECT_SUMMON_WAIT_BASE_0 :
                                     (FF6_ROM_VALUE_EFFECT_SUMMON_WAIT_BASE_0 + 1U);
    require(source.dimensions.width <= 16 && source.dimensions.height <= 16,
            "Invalid summon dimensions");
    auto& image = *images_[index];
    // Both owners expand the full 16-by-16 stencil. Summons select a shorter
    // image afterward; LoadSketchMonsterGfx retains all 256 tiles (8192 bytes).
    std::vector<Tile> tiles(256);
    unsigned tile = 0;
    for (unsigned y = 0; y < source.dimensions.height; ++y)
        for (unsigned x = 0; x < source.dimensions.width; ++x)
            if (source.stencil[y] & (0x8000U >> x)) {
                require(y < 16 && x < 16 && tile < source.packed_tiles.size(),
                        "Invalid sparse summon image");
                tiles[y * 16 + x] = source.packed_tiles[tile++];
            }
    image.character_frames = false;
    image.tiles = std::move(tiles);
    image.owned_tiles.clear();
    image.colors = image.base_colors = source.colors;
}
void BattleAttackAnimation::load_supplemental_summon(std::size_t identity) {
    require(images_[0].has_value(), "Supplemental summon needs sprite frames");
    // The small figure owns a ROM-selected tile extent alongside the two large
    // figures. Keep both resources; replacing the main image loses the wings.
    const auto saved = *images_[0];
    load_summon(identity, BattleEffectLayer::sprite);
    supplemental_image_ = std::move(images_[0]);
    supplemental_image_->tiles.resize(FF6_ROM_VALUE_EFFECT_SUPPLEMENTAL_IMAGE_TILES_0);
    images_[0] = saved;
}
} // namespace ff6
