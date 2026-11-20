#pragma once
#include "ff6/field_camera.hpp"
#include "ff6/field_object_actions.hpp"

namespace ff6 {
struct FieldPlayerContext;
struct FieldPlayerData;
struct FieldPlayerInputs;
struct FieldPlayerResult;
struct FieldCollision {
    enum class Stage : std::uint8_t { none, detected, activated };
    Stage stage = Stage::none;
    std::uint8_t character = 0, npc = 0, direction = 0;
    bool operator==(const FieldCollision&) const = default;
};
struct FieldContactState {
    // Retained independently of the next detected-contact record. Defaults are
    // empty native storage, not the original new-game initialization contract.
    FieldTilePosition scroll_target;
    std::uint8_t default_party = 0;
    bool operator==(const FieldContactState&) const = default;
};
struct FieldActionContext {
    FieldOccupancy occupancy;
    FieldScroll scroll;
    FieldScrollLimits scroll_limits;
    FieldCollision collision;
    FieldContactState contact;
    // These belong to the player/local-tile owner and may be retained during
    // startup. Arrival must not replace them with a fresh lookup.
    FieldTerrainCell player_tile;
    FieldPlayerTerrain player_terrain;
    std::array<std::uint8_t, 2> player_tile_position{};
    std::uint8_t random_index = 0;
    bool camera_locked = false;
    bool operator==(const FieldActionContext&) const = default;
};
struct FieldActionGates {
    bool event_running = true, menu_opening = false, map_loading = false;
};
enum class FieldActionBoundary { complete, player, event };
struct FieldActionDispatch {
    FieldActionBoundary boundary = FieldActionBoundary::complete;
    std::optional<std::uint8_t> object;
    bool operator==(const FieldActionDispatch&) const = default;
};
// A pass selects six of the first 24 update-order slots from the event-visit
// counter. It owns arrival, contact detection, script/random/active modes and
// script camera refresh. Movement integration belongs after the complete pass.
class FieldActionPass {
  public:
    explicit FieldActionPass(std::uint8_t event_visits);
    // Runs until done or an unfinished external owner is required. Repeated
    // calls at a pending boundary return it without repeating arrival work.
    // Player handling resumes only through its complete native owner below.
    // An exception rolls this call's state, grid, scroll, RNG and cursor back.
    FieldActionDispatch advance(FieldState& state, FieldActionContext& context, const FieldMap& map,
                                const FieldActionData& data, FieldActionGates gates = {});
    void consume_event(FieldState& state, const FieldActionData& data, FieldEventRunner& events);
    FieldPlayerResult consume_player(FieldState& state, FieldActionContext& context,
                                     FieldPlayerContext& player, FieldMap& map,
                                     const FieldActionData& actions, const FieldPlayerData& data,
                                     FieldEventRunner& events, const FieldPlayerInputs& input);
    bool complete() const {
        return next_slot_ == end_slot_ && !pending_;
    }
    std::uint8_t next_slot() const {
        return next_slot_;
    }

  private:
    std::uint8_t next_slot_ = 0, end_slot_ = 6;
    std::optional<FieldActionDispatch> pending_;
    bool event_started_ = false;
    FieldActionDispatch run(FieldState& state, FieldActionContext& context, const FieldMap& map,
                            const FieldActionData& data, FieldActionGates gates);
};
} // namespace ff6
