#include "ff6/battle_character_graphics.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::invalid_argument(message);
}
} // namespace
BattleCharacterGraphicsData decode_battle_character_graphics(ByteView frame_tiles, ByteView sources,
                                                             ByteView graphics, ByteView palettes,
                                                             ByteView magitek_frames,
                                                             ByteView vehicles,
                                                             ByteView magitek_palette) {
    require(frame_tiles.size() == 512 && sources.size() == 96 && graphics.size() % 32 == 0 &&
                palettes.size() == 224 && magitek_frames.size() == 36 && vehicles.size() == 8192 &&
                magitek_palette.size() == 24,
            "Invalid battle character resource extent");
    BattleCharacterGraphicsData out;
    out.tiles = decode_tiles(graphics, 4);
    for (unsigned i = 0; i < 256; ++i) {
        const auto offset = read16(frame_tiles, i * 2);
        if (offset == 65535)
            continue;
        require(offset % 32 == 0, "Unaligned battle character tile");
        out.frame_tiles[i] = offset / 32;
    }
    for (unsigned i = 0; i < 24; ++i) {
        out.palette_ids[i] = sources[i];
        require(sources[i] < 7, "Invalid battle character palette selection");
        const auto address = std::uint32_t{read16(sources, 24 + i * 3)} |
                             (std::uint32_t{sources[26 + i * 3]} << 16U);
        require(address >= 0xd50000 && address % 32 == 0,
                "Invalid battle character graphics pointer");
        out.graphics[i] = (address - 0xd50000) / 32;
        for (auto tile : out.frame_tiles)
            if (tile)
                require(std::uint64_t{out.graphics[i]} + *tile < out.tiles.size(),
                        "Battle character tile exceeds shared graphics");
    }
    for (unsigned p = 0; p < 7; ++p)
        for (unsigned c = 0; c < 12; ++c)
            out.palettes[p][c] = read16(palettes, p * 32 + c * 2) & 0x7fffU;
    const auto vehicle_tiles = decode_tiles(vehicles, 4);
    for (unsigned f = 0; f < 5; ++f) {
        const auto address = read32(magitek_frames, f * 4);
        require(address >= 0xd83000 && address % 32 == 0, "Invalid Magitek graphics pointer");
        const auto first = (address - 0xd83000) / 32;
        // The four source rows are interleaved in the vehicle sheet. Their
        // publication destinations establish the final 32x32 row order.
        constexpr std::array<unsigned, 4> rows{0, 16, 4, 20};
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 4; ++column) {
                const auto at = std::uint64_t{first} + rows[row] + column;
                require(at < vehicle_tiles.size(), "Magitek frame exceeds vehicle graphics");
                out.magitek_frames[f][row * 4 + column] = vehicle_tiles[at];
            }
    }
    for (unsigned action = 0; action < 4; ++action)
        for (unsigned phase = 0; phase < 4; ++phase) {
            const auto frame = magitek_frames[20 + action * 4 + phase];
            require(frame < 5, "Invalid Magitek frame selection");
            out.magitek_sequences[action][phase] = frame;
        }
    for (unsigned c = 0; c < 12; ++c)
        out.magitek_colors[c] = read16(magitek_palette, c * 2) & 0x7fffU;
    return out;
}
BattleCharacterGraphicsData load_battle_character_graphics_data(const ContentPack& pack) {
    return decode_battle_character_graphics(
        pack.get("battle_character_frame_tiles"), pack.get("battle_character_sources"),
        pack.get("map_sprite_map_sprite_gfx_4bpp"),
        pack.get("interface_palettes_battle_char_pal_pal"), pack.get("battle_magitek_frames"),
        pack.get("field_overlay_vehicle_gfx_4bpp"), pack.get("battle_magitek_palette"));
}
bool prepare_battle_character_graphics(const BattleCharacterGraphicsData& data,
                                       const BattleCharacterGraphicsRequest& request,
                                       BattleCharacterGraphics& state) {
    if (request.graphics == 255)
        return false;
    require(request.graphics < data.graphics.size(), "Invalid battle character identity");
    BattleCharacterSheet sheet{};
    for (unsigned i = 0; i < sheet.size(); ++i)
        if (data.frame_tiles[i]) {
            const auto at = std::uint64_t{data.graphics[request.graphics]} + *data.frame_tiles[i];
            require(at < data.tiles.size(), "Battle character tile exceeds shared graphics");
            sheet[i] = data.tiles[at];
        }
    // The original reverses 64 bytes at each of two offsets: four tiles,
    // despite the source comment describing the loop as sixty-four tiles.
    for (unsigned i : {30U, 31U, 134U, 135U})
        for (unsigned row = 0; row < 8; ++row)
            std::reverse(sheet[i].begin() + row * 8, sheet[i].begin() + row * 8 + 8);
    const auto palette =
        request.retained_graphics == 14 && request.character == 1 && request.green_soldier_disguise
            ? 0U
            : unsigned{data.palette_ids[request.graphics]};
    require(palette < data.palettes.size(), "Invalid battle character palette");
    state.sheet = std::move(sheet);
    state.colors = data.palettes[palette];
    state.previous_frame = wrap8(unsigned{state.previous_frame} + 1U);
    return true;
}
void publish_battle_character_frame(const BattleCharacterSheet& sheet, std::uint8_t frame,
                                    bool magitek, BattleCharacterFrame& visible) {
    require(frame < 32, "Invalid ordinary character frame");
    std::copy_n(sheet.begin() + unsigned{frame} * 8, magitek ? 6 : 8, visible.begin());
}
std::optional<BattleMagitekFrameSelection>
select_battle_magitek_frame(const BattleCharacterGraphicsData& data, std::uint8_t slot,
                            std::uint8_t action, std::uint8_t counter, bool battle_time_stopped) {
    require(slot < 4, "Invalid Magitek slot");
    if ((counter & 3U) != slot)
        return std::nullopt;
    require(action < 4, "Invalid Magitek action");
    const auto phase = (unsigned{counter} >> (battle_time_stopped ? 3U : 2U)) & 3U;
    const auto frame = data.magitek_sequences[action][phase];
    require(frame < data.magitek_frames.size(), "Invalid Magitek frame");
    return BattleMagitekFrameSelection{frame, static_cast<std::uint8_t>(frame != 0)};
}
} // namespace ff6
