#pragma once
#include "ff6/field_doors.hpp"
#include "ff6/field_overlay.hpp"
#include "ff6/field_player_effects.hpp"
#include "ff6/field_treasure.hpp"

namespace ff6 {
struct FieldPlayerContext {
    FieldStepEffects effects;
    FieldDoorHistory doors;
    FieldTreasureContext treasure;
    FieldOverlayState overlay;
    FieldLocalTiles local;
    bool operator==(const FieldPlayerContext&) const = default;
};
struct FieldPlayerData {
    FieldDoorTiles doors;
    FieldTreasureData treasure;
    std::vector<FieldTreasureDefinition> treasures;
    FieldOverlaySet overlay;
    FieldOverlayLayouts layouts;
    std::uint32_t idle_event = 0;
};
FieldPlayerData load_field_player_data(const ContentPack& pack, std::uint16_t map);
struct FieldPlayerInputs {
    Buttons buttons = 0;
    bool fully_bright = false, menu_opening = false, map_loading = false, dialogue_open = false;
    std::array<std::uint8_t, 3> background_updates{};
    // The scene supplies its retained drawing origin. Follow rates are read
    // after this visit's camera update, never from a stale presentation input.
    std::array<std::uint16_t, 2> overlay_origin{};
};
struct FieldPlayerResult {
    FieldStep step;
    std::optional<FieldDoorOpened> door;
    std::optional<std::uint8_t> talk;
    std::optional<FieldTreasureOpened> treasure;
    bool close_map_title() const {
        return talk.has_value() || treasure.has_value();
    }
};
// One aligned player action visit. The scheduler's outer control gate and
// arrival precede this function; object/camera integration follow the pass.
// All owned state commits together. Returned sound, title and background
// publication intentions still require their scene owners.
FieldPlayerResult advance_field_player(FieldState& state, FieldActionContext& context,
                                       FieldPlayerContext& player, FieldMap& map,
                                       const FieldActionData& actions, const FieldPlayerData& data,
                                       FieldEventRunner& events, std::uint8_t caller,
                                       const FieldPlayerInputs& input);
} // namespace ff6
