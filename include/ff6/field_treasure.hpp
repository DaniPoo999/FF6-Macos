#pragma once
#include "ff6/field_npcs.hpp"

namespace ff6 {
enum class FieldTreasureKind : std::uint8_t { empty, item, gil, monster };
struct FieldTreasureDefinition {
    FieldTilePosition position;
    std::uint16_t opened_flag = 0;
    std::uint8_t contents = 0;
    FieldTreasureKind kind = FieldTreasureKind::empty;
    bool operator==(const FieldTreasureDefinition&) const = default;
};
FieldTreasureDefinition decode_field_treasure(ByteView record);
std::vector<FieldTreasureDefinition> load_field_treasures(const ContentPack& pack,
                                                          std::uint16_t map);
struct FieldTreasureData {
    FieldMapPatch chest;
    // Named system events in the extracted event language: empty/item/gil/monster.
    std::array<std::uint32_t, 4> events{};
};
FieldTreasureData load_field_treasure_data(const ContentPack& pack);
// New-game initialization clears only the original first 384 flags.
void initialize_field_treasure_flags(FieldFlags& flags);
struct FieldTreasureContext {
    // Retained substitutions for future dialogue, and the pending chest encounter.
    std::uint8_t message_item = 0, battle_formation = 0;
    std::uint32_t message_gil = 0;
    bool operator==(const FieldTreasureContext&) const = default;
};
struct FieldTreasureInputs {
    bool dialogue_open = false, menu_opening = false, map_loading = false;
    bool event_running = true, confirm_down = false;
};
struct FieldTreasureOpened {
    FieldTreasureDefinition treasure;
    bool update_background = false;
    // The successful operation supplies its ROM-selected cue at centered pan.
    std::uint8_t sound_effect = 0;
    bool operator==(const FieldTreasureOpened&) const = default;
};
// Uses the selected actor's cached occupancy position (not its live coordinates).
// Applies reward, flags, map edit, movement save and root-event activation together.
std::optional<FieldTreasureOpened>
try_collect_field_treasure(FieldState& state, FieldMap& map, FieldEventRunner& runner,
                           FieldTreasureContext& context, const FieldOccupancy& occupancy,
                           FieldTerrainCell cached_tile, FieldPlayerTerrain terrain,
                           std::span<const FieldTreasureDefinition> definitions,
                           const FieldTreasureData& data, FieldTreasureInputs inputs);
// Fresh base layouts -> treasure restoration -> door restoration -> display build.
// Flags persist across maps; this does not award contents or schedule an event.
void restore_field_treasures(FieldMap& map, const FieldFlags& flags,
                             std::span<const FieldTreasureDefinition> definitions,
                             const FieldTreasureData& data);
} // namespace ff6
