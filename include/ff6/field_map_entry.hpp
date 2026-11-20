#pragma once
#include "ff6/field_loop.hpp"

namespace ff6 {
struct FieldMapEntryAssets {
    std::uint16_t identity = 0;
    FieldLoopAssets scene;
    std::vector<FieldNpcDefinition> npcs;
    std::uint32_t startup = 0;
};
FieldMapEntryAssets load_field_map_entry_assets(const ContentPack& pack, std::uint16_t identity,
                                                const FieldMap& previous_display);
struct PreparedFieldMapEntry {
    FieldLoopAssets assets;
    FieldLoopSeed seed;
};
// LoadMap through InitNPCMap, including retained-map menu/battle returns.
// Equal identities alone do not imply reload. New retained-dimension loads reject.
// Startup/InitZLevel, music/title and initial publication finish in FieldLoop.
PreparedFieldMapEntry prepare_field_map_entry(FieldMapEntryAssets assets, FieldLoopSeed retained,
                                            bool reloading_same_map = false);
} // namespace ff6
