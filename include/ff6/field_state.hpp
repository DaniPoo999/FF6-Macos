#pragma once
#include "ff6/equipment.hpp"
#include "ff6/field_events.hpp"
#include "ff6/field_sprites.hpp"
#include "ff6/inventory.hpp"
#include <bitset>

namespace ff6 {
struct FieldMovement {
    // Mode 0 = stopped, 1 = script, 2 = player; other object modes and the
    // four retained option flags belong to the future movement engine.
    std::uint8_t mode = 0, options = 0;
    bool operator==(const FieldMovement&) const = default;
};
struct FieldAppearance {
    std::uint8_t palette = 0, priority = 0;
    bool tile_bank = false, flip_x = false, flip_y = false;
    bool operator==(const FieldAppearance&) const = default;
};
struct FieldTilePosition {
    std::uint8_t x = 0, y = 0;
    bool operator==(const FieldTilePosition&) const = default;
};
struct FieldObject {
    PartyMembership membership;
    FieldMovement movement, saved_movement;
    bool walking_animation = false;
    std::uint8_t graphics = 0, facing = 0, saved_facing = 0;
    // Special-size NPC definitions use these 18 bits as graphics-link data
    // instead of an interaction cursor. Neither meaning is executable code.
    std::uint32_t interaction_data = 0;
    std::uint8_t terrain_level = 0, action_wait = 0, action_steps = 0;
    // Cursor in the extracted object-action data language; execution belongs to
    // the native object-action owner. It is never a machine-code address.
    std::optional<std::uint32_t> action_script;
    std::uint16_t map = 0;
    FieldAppearance upper, lower;
    // Last occupancy cell, deliberately separate from the current position.
    // Leader changes release this cached cell before regrouping the party.
    FieldTilePosition occupancy_tile;
    std::optional<std::uint8_t> animation_slot;
    // Unsigned 16.8 positions with explicit 24-bit wrapping. Velocities are
    // signed 1/256-pixel steps; choosing them belongs to movement/collision logic.
    std::uint32_t x = 0, y = 0;
    std::int16_t velocity_x = 0, velocity_y = 0;
    std::uint8_t speed = 0, move_direction = 0;
    // 0 follows terrain, 1 raises the upper half, 2 raises both, 3 lowers both.
    std::uint8_t layer_priority_mode = 0;
    std::uint16_t jump_y = 0;
    std::uint8_t jump_counter = 0, pending_pose = 0, pose = 0;
    FieldRide ride = FieldRide::none;
    std::optional<FieldSpecialPose> special_pose;
    enum class DrawGroup : std::uint8_t { normal, high, low };
    DrawGroup draw_group = DrawGroup::normal;
    bool scroll_with_background2 = false, special_graphics = false, rider_visible = false;
    bool operator==(const FieldObject&) const = default;
};
struct FieldDestination {
    std::uint16_t map = 0;
    std::uint8_t x = 0, y = 0, facing = 0;
    bool operator==(const FieldDestination&) const = default;
};
struct FieldObjects {
    static constexpr std::uint8_t camera = 48;
    std::array<FieldObject, 49> objects;
    std::array<std::optional<std::uint8_t>, 4> party_selectors{};
    // The original retains the tail; aliases use the first four entries even
    // when the last sort produced fewer than four objects. No bus addresses.
    std::array<std::optional<std::uint8_t>, 50> update_order{};
    std::uint8_t update_count = 0;
    std::array<std::optional<std::uint8_t>, 24> animation_queue{};
    std::uint16_t current_map = 0;
    FieldDestination parent_map;
    std::uint8_t event_visits = 0, equipment_effects = 0;
    bool sort_pending = false;
    bool operator==(const FieldObjects&) const = default;
};
struct FieldFlags {
    std::bitset<0x2f0> story;
    std::bitset<0x400> npc;
    std::bitset<512> treasure;
    // Event E0 and equipment menus use initialized actors ($1EDC), not
    // E1's available actors ($1EDE). Keep the saved story bits authoritative.
    std::uint16_t initialized_characters() const {
        std::uint16_t mask = 0;
        for (unsigned i = 0; i < 16; ++i)
            if (story[0x2e0 + i])
                mask |= static_cast<std::uint16_t>(1U << i);
        return mask;
    }
    bool operator==(const FieldFlags&) const = default;
};
struct FieldState {
    Roster roster;
    Inventory inventory;
    FieldObjects field;
    FieldFlags flags;
    // Persistent unsigned words used by the authored event data language.
    std::array<std::uint16_t, 8> event_variables{};
    // Persistent variables shared by the battle AI and field event languages.
    std::array<std::uint8_t, 20> battle_variables{};
    // Eligible encounters remembered by UpdateSRAM for later Veldt selection.
    std::bitset<512> encountered_formations;
    bool last_battle_lost = false, terra_magic_pending = false;
    bool operator==(const FieldState&) const = default;
};
// Defaults describe empty native storage, not complete original new-game state.
// Owners must supply initial object/order state and initialize flags explicitly.
class FieldFlagAccess final : public EventFlagAccess {
  public:
    explicit FieldFlagAccess(FieldState& state) : state_(state) {}
    bool test(std::uint16_t identity) const override;
    bool test_battle(std::uint8_t identity) const override;
    void set_battle(std::uint8_t identity, bool value) override;
    void set(std::uint16_t identity, bool value) override;

  private:
    FieldState& state_;
};
void initialize_field_flags(FieldState& state, const ContentPack& pack);
void sort_field_objects(FieldState& state);
// Field flags become persistent while the roster's battle positions survive.
void save_field_membership(FieldState& state);
// CalcObjPtr semantics for owners outside the actor-only event consumer.
// Rejects unsupported dummy-object writes; party aliases use retained order.
std::optional<std::uint8_t> find_field_object_reference(const FieldState& state, std::uint8_t reference);
std::uint8_t resolve_field_object_reference(const FieldState& state, std::uint8_t reference);
// UpdateEquip selects enabled characters in the active field party. The
// roster's stored membership can differ while an event is changing the party.
void refresh_field_equipment(FieldState& state, const PartyData& data,
                             const EquipmentData& equipment);
void begin_field_event_visit(FieldState& state, bool map_load_pending = false,
                             bool reloading_same_map = false);
// Readiness/facing used by the runner comes from owned field state. Other inputs
// remain explicit until their native owners and the full field clock exist.
EventYield advance_field_event(FieldState& state, FieldEventRunner& runner, EventInputs inputs = {},
                               std::size_t command_budget = 1024);
// False leaves operations needing another owner (dialogue/cutscene/occupancy) pending.
// Invalid state/unsupported dummy writes throw without changing any owned state.
bool apply_field_operation(FieldState& state, const EventOperation& operation,
                           const PartyData& party_data, const EquipmentData& equipment);
// Acknowledge only an operation actually completed here. Unknown owners keep the
// exact request pending; calling without a pending request is a contract error.
bool consume_field_request(FieldState& state, FieldEventRunner& runner, const PartyData& party_data,
                           const EquipmentData& equipment);
} // namespace ff6
