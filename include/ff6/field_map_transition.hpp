#pragma once
#include "ff6/field_map_state.hpp"

namespace ff6 {
enum class FieldFadeDirection : std::uint8_t { steady, brighter, darker };
struct FieldFade {
    std::uint8_t brightness = 0, step = 0;
    FieldFadeDirection direction = FieldFadeDirection::steady;
    bool active() const {
        return direction != FieldFadeDirection::steady;
    }
    bool operator==(const FieldFade&) const = default;
};
// One original logical brightness-update boundary, independent of host refresh.
// Returns canonical four-bit intensity; retains the fractional brightness bits.
std::uint8_t advance_field_fade(FieldFade& fade);
struct FieldMapArrival {
    FieldTilePosition position;
    FieldFacing facing = FieldFacing::up;
    bool show_name = false;
    bool preserve_facing = false;
    bool operator==(const FieldMapArrival&) const = default;
};
struct WorldMapArrival {
    FieldTilePosition position;
    FieldFacing header_facing = FieldFacing::up;
    std::uint8_t vehicle = 0;
    std::uint32_t event_resume = 0;
    bool operator==(const WorldMapArrival&) const = default;
};
struct FieldMapLoadPlan {
    std::uint16_t map = 0;
    bool dispatch_world = false;
    std::variant<FieldMapArrival, WorldMapArrival> arrival;
    FieldCharacterLocations characters;
    FieldDestination parent;
    // Optional write to persisted return-facing state. Bit 7 selects an explicit
    // parent-exit direction; the world owner otherwise uses header_facing.
    std::optional<std::uint8_t> return_facing;
    bool run_startup = false, fade_in = true, keep_dimensions = false;
    // A party switch/relocation reloads presentation while keeping live NPC
    // tasks and positions on the same map.
    bool reload_same_map = false;
    // Event map commands retain this setting; ordinary field entrances write it.
    std::optional<std::uint8_t> saved_level;
    bool operator==(const FieldMapLoadPlan&) const = default;
};
FieldMapLoadPlan resolve_field_map_load(const FieldObjects& field, const EventMapLoad& command,
                                        std::uint32_t event_resume);
struct FieldMapTransition {
    FieldFade fade;
    std::optional<FieldMapLoadPlan> pending;
    bool ready_to_load() const {
        return pending.has_value() && !fade.active();
    }
    bool operator==(const FieldMapTransition&) const = default;
};
// Applies the scheduling command, not the requested scene load. The pending plan
// must stay owned until the scene/world owner has actually performed the load.
// There is deliberately no automatic load-completion/placeholder path.
bool queue_field_map_request(FieldState& state, FieldMapTransition& transition,
                             FieldEventRunner& runner);
EventYield advance_field_map_event(FieldState& state, const FieldMapTransition& transition,
                                   FieldEventRunner& runner, EventInputs inputs = {},
                                   std::size_t command_budget = 1024);
} // namespace ff6
