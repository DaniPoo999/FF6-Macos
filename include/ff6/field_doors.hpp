#pragma once
#include "ff6/field_npcs.hpp"

namespace ff6 {
using FieldDoorTiles = std::array<FieldMapPatch, 3>;
FieldDoorTiles load_field_door_tiles(const ContentPack& pack);
struct FieldDoorHistory {
    // The original writes slot 23 but never increments past 23 counted entries.
    // Keep that last, uncounted write; do not reinterpret this as a 24-door list.
    std::array<FieldTilePosition, 24> positions{};
    std::uint8_t count = 0;
    bool operator==(const FieldDoorHistory&) const = default;
};
void reset_field_doors(FieldDoorHistory& history); // New map only; retain unused positions.
struct FieldDoorOpened {
    FieldTilePosition position;
    std::uint8_t background_update = 4; // Request BG1 publication from the display owner.
    // The successful door owner supplies its ROM-derived ordinary effect.
    std::uint8_t sound_effect = 0;
    bool operator==(const FieldDoorOpened&) const = default;
};
// Uses caller-retained tiles/position, not an implicit fresh player lookup.
// History and map edits commit together; no step, cache or occupancy mutation.
std::optional<FieldDoorOpened>
try_open_field_door(FieldMap& map, FieldDoorHistory& history, const FieldDoorTiles& patches,
                    const FieldOccupancy& occupancy, FieldTilePosition retained_position,
                    const FieldLocalTiles& tiles, FieldPlayerTerrain terrain, std::uint8_t facing);
// Called after loading fresh layout data (and treasure edits), even on reload.
// Does not play sounds, request a display refresh or change history. The initial
// display build follows later. Repeated calls on already edited data are not idempotent.
void restore_field_doors(FieldMap& map, const FieldDoorHistory& history,
                         const FieldDoorTiles& patches);
} // namespace ff6
