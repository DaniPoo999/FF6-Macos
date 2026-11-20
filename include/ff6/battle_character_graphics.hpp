#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"

namespace ff6 {
using BattleCharacterSheet = std::array<Tile, 256>;
// Two 16x16 pieces: four tiles each, in row-major order within each piece.
using BattleCharacterFrame = std::array<Tile, 8>;
using BattleCharacterColors = std::array<Rgb555, 12>;
struct BattleCharacterGraphicsData {
    std::array<std::optional<std::uint16_t>, 256> frame_tiles{};
    std::array<std::uint32_t, 24> graphics{}; // Normalized indices within the shared tile pool.
    std::array<std::uint8_t, 24> palette_ids{};
    std::vector<Tile> tiles;
    std::array<BattleCharacterColors, 7> palettes{};
    std::array<std::array<Tile, 16>, 5> magitek_frames{}; // Four rows of four tiles.
    std::array<std::array<std::uint8_t, 4>, 4> magitek_sequences{};
    BattleCharacterColors magitek_colors{};
};
BattleCharacterGraphicsData decode_battle_character_graphics(ByteView frame_tiles, ByteView sources,
                                                             ByteView graphics, ByteView palettes,
                                                             ByteView magitek_frames,
                                                             ByteView vehicles,
                                                             ByteView magitek_palette);
BattleCharacterGraphicsData load_battle_character_graphics_data(const ContentPack& pack);
struct BattleCharacterGraphics {
    BattleCharacterSheet sheet{};
    // Status/cursor colors 12..15 belong to a different owner and are not replaced.
    BattleCharacterColors colors{};
    std::uint8_t previous_frame = 0;
    bool operator==(const BattleCharacterGraphics&) const = default;
};
struct BattleCharacterGraphicsRequest {
    std::uint8_t graphics = 255, retained_graphics = 255, character = 255;
    bool green_soldier_disguise = false;
};
// Absent graphics preserve retained state. Successful loading invalidates the
// previous frame using byte wrap, including when loading the same identity.
bool prepare_battle_character_graphics(const BattleCharacterGraphicsData& data,
                                       const BattleCharacterGraphicsRequest& request,
                                       BattleCharacterGraphics& state);
// Ordinary frame publication only. Magitek deliberately preserves the last two
// tiles. Vanish/condemned overlays and frame scheduling have separate owners.
void publish_battle_character_frame(const BattleCharacterSheet& sheet, std::uint8_t frame,
                                    bool magitek, BattleCharacterFrame& visible);
struct BattleMagitekFrameSelection {
    std::uint8_t frame = 0, rider_offset_y = 0;
    bool operator==(const BattleMagitekFrameSelection&) const = default;
};
// Only the slot selected by the shared counter updates. Callers retain the old
// selection when this returns no update and publish graphics at the scene boundary.
std::optional<BattleMagitekFrameSelection>
select_battle_magitek_frame(const BattleCharacterGraphicsData& data, std::uint8_t slot,
                            std::uint8_t action, std::uint8_t counter, bool battle_time_stopped);
} // namespace ff6
