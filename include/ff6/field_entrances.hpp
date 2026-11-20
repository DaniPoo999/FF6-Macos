#pragma once
#include "ff6/field_map_transition.hpp"

namespace ff6 {
struct FieldEntrance {
    FieldTilePosition position, destination;
    std::uint16_t map = 0;
    std::uint8_t length = 0, facing = 0;
    bool vertical = false, set_parent = false, lower_level = false, show_name = false;
};
struct FieldEntrances {
    std::vector<FieldEntrance> long_regions, short_regions;
    // Parent sub-map paths accidentally select a byte from the pointer table,
    // indexed by the parent identity. Preserve the consumed title-enable bit.
    std::array<bool, 512> long_parent_names{}, short_parent_names{};
};
FieldEntrances load_field_entrances(const ContentPack& pack, std::uint16_t map);
// Retained destination settings are independent of the actor's current tile.
// A complete map owner supplies them after loading; do not infer them on exit.
struct FieldEntranceMemory {
    FieldMapArrival placement;
    FieldCharacterLocations characters{};
    std::uint8_t saved_level = 1, world_vehicle = 0, return_facing = 0;
    bool run_startup = false, fade_in = true, keep_dimensions = false;
    bool operator==(const FieldEntranceMemory&) const = default;
};
struct FieldEntranceInputs {
    FieldTilePosition tile;
    std::uint8_t terrain_flags = 0, level = 1;
    bool event_running = true, disabled = false, menu_opening = false, battle_pending = false;
    std::uint32_t idle_event = 0;
};
struct FieldEntranceResult {
    std::optional<std::size_t> long_match, short_match;
    // These retained writes have consumers outside ordinary entrance loading.
    bool clear_special_npc_count = false;
    std::optional<FieldTilePosition> saved_scroll_position;
    bool matched() const {
        return long_match.has_value() || short_match.has_value();
    }
};
// Executes both ordered checks transactionally. Queueing a destination starts
// its fade; a long parent/sub-map path can fade without requesting any load.
FieldEntranceResult check_field_entrances(FieldObjects& field, FieldMapTransition& transition,
                                          FieldEntranceMemory& memory,
                                          const FieldEntrances& entrances,
                                          FieldEntranceInputs inputs);
} // namespace ff6
