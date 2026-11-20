#include "ff6/field_state.hpp"
#include "rom-materials/field_event_hp_base.macros.inc"
#include "rom-materials/field_event_mp_quantity.macros.inc"
#include <algorithm>
#include <type_traits>

namespace ff6 {
void save_field_membership(FieldState& state) {
    for (std::size_t id = 0; id < state.roster.membership.size(); ++id) {
        const auto position = state.roster.membership[id].position;
        state.roster.membership[id] = state.field.objects[id].membership;
        state.roster.membership[id].position = position;
    }
}
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
void validate_membership(const PartyMembership& value) {
    require(value.party < 8 && value.position < 4, "Invalid field membership");
}
void validate(const FieldState& state) {
    require(state.roster.active_party < 8 && state.field.update_count <= 50,
            "Invalid field party/update count");
    for (const auto& member : state.roster.membership)
        validate_membership(member);
    for (const auto& object : state.field.objects) {
        validate_membership(object.membership);
        require(object.movement.mode < 16 && object.movement.options < 16 &&
                    object.saved_movement.mode < 16 && object.saved_movement.options < 16 &&
                    object.facing < 4 && object.saved_facing < 4,
                "Invalid field movement/facing");
        for (const auto& appearance : {object.upper, object.lower})
            require(appearance.palette < 8 && appearance.priority < 4, "Invalid field appearance");
        require(!object.animation_slot || *object.animation_slot < 24,
                "Invalid object animation slot");
    }
    const auto validate_refs = [&](const auto& refs) {
        for (auto id : refs)
            require(!id || *id < state.field.objects.size(), "Invalid field object identity");
    };
    validate_refs(state.field.party_selectors);
    validate_refs(state.field.update_order);
    validate_refs(state.field.animation_queue);
    for (const auto identity : state.field.animation_queue)
        require(!identity || *identity < FieldObjects::camera,
                "Animation queue uses empty entries for the camera sentinel");
}
std::optional<std::uint8_t> resolve(const FieldState& state, std::uint8_t reference) {
    require(reference <= 52, "Invalid event object reference");
    if (reference <= FieldObjects::camera)
        return reference;
    const auto object = state.field.update_order[reference - 49U];
    if (!object || *object == FieldObjects::camera ||
        state.field.objects[*object].membership.party != state.roster.active_party)
        return std::nullopt;
    return object;
}
std::uint8_t object_target(const FieldState& state, std::uint8_t reference) {
    const auto id = resolve(state, reference);
    require(id.has_value(), "Event dummy-object writes are not implemented");
    return *id;
}
std::optional<std::uint8_t> character_target(const FieldState& state, std::uint8_t reference) {
    const auto id = resolve(state, reference);
    return id && *id < 16 ? id : std::nullopt;
}
std::uint8_t writable_character(const FieldState& state, std::uint8_t reference) {
    const auto id = character_target(state, reference);
    require(id.has_value(), "Event character write outside native roster");
    return *id;
}
void truncate_selectors(FieldObjects& field, std::uint8_t identity) {
    const auto found = std::ranges::find(field.party_selectors, identity);
    std::fill(found, field.party_selectors.end(), std::nullopt);
}
} // namespace
std::optional<std::uint8_t> find_field_object_reference(const FieldState& state, std::uint8_t reference) {
    validate(state);
    return resolve(state, reference);
}
std::uint8_t resolve_field_object_reference(const FieldState& state, std::uint8_t reference) {
    validate(state);
    return object_target(state, reference);
}
void refresh_field_equipment(FieldState& state, const PartyData& data,
                             const EquipmentData& equipment) {
    std::array<std::uint8_t, 16> selected{};
    std::size_t count = 0;
    for (std::uint8_t i = 0; i < 16; ++i) {
        const auto& member = state.field.objects[i].membership;
        if (member.enabled && member.party == state.roster.active_party)
            selected[count++] = i;
    }
    state.field.equipment_effects =
        refresh_equipment(state.roster, data, equipment, std::span(selected).first(count))
            .field_effects;
}
namespace {
void assign_party(FieldState& state, const EventCharacterParty& operation, const PartyData& data,
                  const EquipmentData& equipment) {
    require(operation.party < 8, "Invalid event party");
    // Restore before both the free-slot scan and alias resolution. The existing
    // update order survives; current field flags may have differed from storage.
    for (std::size_t i = 0; i < 16; ++i)
        state.field.objects[i].membership = state.roster.membership[i];
    std::array<bool, 3> occupied{};
    for (std::size_t i = 0; i < 16; ++i) {
        const auto& member = state.field.objects[i].membership;
        if (member.enabled && member.party == operation.party && member.position < 3)
            occupied[member.position] = true;
    }
    std::uint8_t position = 0;
    if (operation.party != 0)
        while (position < occupied.size() && occupied[position])
            ++position;
    const auto identity = writable_character(state, operation.character);
    auto& object = state.field.objects[identity];
    object.membership.party = operation.party;
    object.membership.position = position;
    object.saved_movement = {};
    truncate_selectors(state.field, identity);
    state.roster.membership[identity] = object.membership;
    refresh_field_equipment(state, data, equipment);
}
void set_palette(FieldAppearance& appearance, std::uint8_t operand) {
    appearance.palette = operand & 7U;
    appearance.priority |= (operand >> 3U) & 3U;
    appearance.flip_x = appearance.flip_x || (operand & 0x20U) != 0;
    appearance.flip_y = appearance.flip_y || (operand & 0x40U) != 0;
}
void sort_impl(FieldState& state) {
    auto& field = state.field;
    std::size_t count = 0;
    const auto append = [&](std::uint8_t identity) {
        require(count < field.update_order.size(), "Field update order overflow");
        field.update_order[count++] = identity;
    };
    const auto append_active = [&](std::uint8_t identity) {
        field.objects[identity].map = field.current_map;
        append(identity);
    };
    for (std::size_t slot = 0; slot < field.party_selectors.size(); ++slot) {
        const auto identity = field.party_selectors[slot];
        if (!identity)
            continue;
        const auto& member = field.objects[*identity].membership;
        if (!member.enabled) {
            std::fill(field.party_selectors.begin() + static_cast<std::ptrdiff_t>(slot),
                      field.party_selectors.end(), std::nullopt);
            break;
        }
        if (member.party == state.roster.active_party)
            append_active(*identity);
    }
    for (std::uint8_t i = 0; i < 16; ++i) {
        const auto& member = field.objects[i].membership;
        if (std::ranges::find(field.party_selectors, i) == field.party_selectors.end() &&
            member.enabled && member.party == state.roster.active_party)
            append_active(i);
    }
    append(FieldObjects::camera);
    for (std::uint8_t i = 0; i < 16; ++i) {
        const auto& object = field.objects[i];
        if (object.membership.enabled && (object.membership.party == 0 ||
                                          (object.membership.party != state.roster.active_party &&
                                           object.map == field.current_map)))
            append(i);
    }
    for (std::uint8_t i = 16; i < FieldObjects::camera; ++i)
        if (field.objects[i].membership.enabled)
            append(i);
    field.update_count = static_cast<std::uint8_t>(count);
    field.sort_pending = false;
}
} // namespace

bool FieldFlagAccess::test(std::uint16_t identity) const {
    require(identity < 0x700, "Invalid field flag identity");
    if (identity < 0x2f0)
        return state_.flags.story[identity];
    if (identity < 0x300)
        return (state_.roster.recruited & (1U << (identity - 0x2f0U))) != 0;
    return state_.flags.npc[identity - 0x300U];
}
void FieldFlagAccess::set(std::uint16_t identity, bool value) {
    require(identity < 0x700, "Invalid field flag identity");
    if (identity < 0x2f0)
        state_.flags.story[identity] = value;
    else if (identity < 0x300) {
        const auto bit = 1U << (identity - 0x2f0U);
        state_.roster.recruited = static_cast<std::uint16_t>(
            value ? state_.roster.recruited | bit : state_.roster.recruited & ~bit);
    } else
        state_.flags.npc[identity - 0x300U] = value;
}
bool FieldFlagAccess::test_battle(std::uint8_t identity) const {
    if (identity == 0x40)
        return state_.last_battle_lost;
    if (identity == 0x41)
        return state_.terra_magic_pending;
    require(identity < state_.battle_variables.size() * 8, "Invalid global battle flag");
    return (state_.battle_variables[identity / 8] & (1U << (identity % 8))) != 0;
}
void FieldFlagAccess::set_battle(std::uint8_t identity, bool value) {
    if (identity == 0x40)
        state_.last_battle_lost = value;
    else if (identity == 0x41)
        state_.terra_magic_pending = value;
    else {
        require(identity < state_.battle_variables.size() * 8, "Invalid global battle flag");
        auto& byte = state_.battle_variables[identity / 8];
        const auto bit = 1U << (identity % 8);
        byte = static_cast<std::uint8_t>(value ? byte | bit : byte & ~bit);
    }
}
void initialize_field_flags(FieldState& state, const ContentPack& pack) {
    const auto data = pack.get("data_c0e0a0_initial_npc_switches");
    require(data.size() == 128, "Invalid initial NPC flag resource");
    state.flags.story.reset();
    state.roster.recruited = 0;
    for (std::size_t i = 0; i < state.flags.npc.size(); ++i)
        state.flags.npc[i] = (data[i / 8] & (1U << (i % 8))) != 0;
}
void sort_field_objects(FieldState& state) {
    validate(state);
    auto next = state;
    sort_impl(next);
    state = std::move(next);
}
void begin_field_event_visit(FieldState& state, bool map_load_pending, bool reloading_same_map) {
    validate(state);
    auto next = state;
    next.field.event_visits = wrap8(unsigned{next.field.event_visits} + 1U);
    if (!map_load_pending && (reloading_same_map || (next.field.event_visits & 3U) == 0))
        sort_impl(next);
    state = std::move(next);
}
EventYield advance_field_event(FieldState& state, FieldEventRunner& runner, EventInputs inputs,
                               std::size_t command_budget) {
    validate(state);
    if (const auto object = runner.waiting_object())
        inputs.object_script_active = state.field.objects[*object].movement.mode != 0;
    inputs.objects_sorted = !state.field.sort_pending;
    inputs.facing = 0;
    // No facing read occurs while sorting/pause blocks execution. The field
    // owner must have seeded or produced an order before actual continuation.
    if (!runner.pending_request() && !runner.finished() && inputs.objects_sorted &&
        inputs.backgrounds_ready && !inputs.map_load_pending && runner.pause_remaining() == 0) {
        require(state.field.update_order[0].has_value(), "Field update order is not initialized");
        inputs.facing = state.field.objects[*state.field.update_order[0]].facing;
    }
    FieldFlagAccess flags(state);
    return runner.advance(flags, inputs, command_budget);
}
bool apply_field_operation(FieldState& state, const EventOperation& operation,
                           const PartyData& party_data, const EquipmentData& equipment) {
    if (std::holds_alternative<EventOptimizeEquipment>(operation) ||
        std::holds_alternative<EventDialogue>(operation) ||
        std::holds_alternative<EventStartBattle>(operation) ||
        std::holds_alternative<EventLoadMenu>(operation) ||
        std::holds_alternative<EventRestartGame>(operation) ||
        std::holds_alternative<EventClearOverlay>(operation) ||
        std::holds_alternative<EventNameMenu>(operation) ||
        std::holds_alternative<EventPartyMenu>(operation) ||
        std::holds_alternative<EventFinalOrder>(operation) ||
        std::holds_alternative<EventLoadPalette>(operation) ||
        std::holds_alternative<EventColosseumMenu>(operation) ||
        std::holds_alternative<EventShop>(operation) ||
        std::holds_alternative<EventScreenMosaic>(operation) ||
        std::holds_alternative<EventDialogueSelection>(operation) ||
        std::holds_alternative<EventRunCutscene>(operation) ||
        std::holds_alternative<EventUpdateParty>(operation) ||
        std::holds_alternative<EventMapLoad>(operation) ||
        std::holds_alternative<EventMapEdit>(operation) ||
        std::holds_alternative<EventApplyMapEdits>(operation) ||
        std::holds_alternative<EventStopScroll>(operation) ||
        std::holds_alternative<EventCameraLock>(operation) ||
        std::holds_alternative<EventScreenFade>(operation) ||
        std::holds_alternative<EventStopScreenFade>(operation) ||
        std::holds_alternative<EventWaitObject>(operation) ||
        std::holds_alternative<EventGameSoundEffect>(operation) ||
        std::holds_alternative<EventPlaySong>(operation) ||
        std::holds_alternative<EventPortrait>(operation) ||
        std::holds_alternative<EventFadeSong>(operation) ||
        std::holds_alternative<EventContinueSong>(operation) ||
        std::holds_alternative<EventStopSound>(operation) ||
        std::holds_alternative<EventMusicAdjustment>(operation) ||
        std::holds_alternative<EventSetTimer>(operation) ||
        std::holds_alternative<EventResetTimer>(operation) ||
        std::holds_alternative<EventRandomBranch>(operation) ||
        std::holds_alternative<PlaybackVolumeRamp>(operation) ||
        std::holds_alternative<EffectPanRamp>(operation) ||
        std::holds_alternative<EventDeleteObject>(operation) ||
        std::holds_alternative<EventHideObject>(operation) ||
        std::holds_alternative<EventPaletteFilter>(operation) ||
        std::holds_alternative<EventDefaultParty>(operation) ||
        std::holds_alternative<EventPartyPosition>(operation))
        return false;
    validate(state);
    auto next = state;
    std::visit(
        [&](const auto& op) {
            using T = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<T, EventCharacterName>) {
                assign_character_name(next.roster, party_data,
                                      writable_character(next, op.character), op.name);
            } else if constexpr (std::is_same_v<T, EventCharacterProperties>) {
                const auto identity = writable_character(next, op.character);
                assign_character_properties(next.roster, party_data, identity, op.properties);
                auto& object = next.field.objects[identity];
                object.movement.mode = 1;
                object.walking_animation = true;
                object.membership = {};
            } else if constexpr (std::is_same_v<T, EventNormalizeLevel>) {
                normalize_character_level(next.roster, party_data,
                                          writable_character(next, op.character));
            } else if constexpr (std::is_same_v<T, EventCreateObject>) {
                const auto identity = object_target(next, op.object);
                auto& object = next.field.objects[identity];
                if (!object.membership.enabled) {
                    object.membership.enabled = true;
                    object.membership.visible = false;
                    std::uint8_t slot = 0;
                    while (slot < 23 && next.field.animation_queue[slot])
                        ++slot;
                    // The original uses the camera identity as its empty queue
                    // sentinel. Creating the camera records a slot but leaves
                    // that queue entry available to the next ordinary object.
                    next.field.animation_queue[slot] =
                        identity == FieldObjects::camera ? std::nullopt : std::optional{identity};
                    object.animation_slot = slot;
                    if (identity < 16)
                        next.roster.membership[identity].enabled = true;
                }
            } else if constexpr (std::is_same_v<T, EventShowObject>) {
                // CalcObjPtr redirects absent party slots to a disabled object;
                // EventCmd_41 then returns without showing anything. Shared
                // party reveal scripts deliberately address all four slots.
                const auto identity = resolve(next, op.object);
                if (!identity)
                    return;
                auto& object = next.field.objects[*identity];
                if (object.membership.enabled && !object.membership.visible) {
                    object.membership.visible = true;
                    object.upper.priority = object.lower.priority = 2;
                    if (*identity < 16)
                        next.roster.membership[*identity].visible = true;
                }
            } else if constexpr (std::is_same_v<T, EventCharacterParty>) {
                assign_party(next, op, party_data, equipment);
            } else if constexpr (std::is_same_v<T, EventObjectGraphics>) {
                const auto identity = object_target(next, op.object);
                next.field.objects[identity].graphics = op.graphics;
                if (identity < 16)
                    next.roster.characters[identity].graphics = op.graphics;
            } else if constexpr (std::is_same_v<T, EventObjectPalette>) {
                auto& object = next.field.objects[object_target(next, op.object)];
                set_palette(object.upper, op.palette);
                set_palette(object.lower, op.palette);
            } else if constexpr (std::is_same_v<T, EventObjectCollision>) {
                // EventCmd_36/78 also address empty party slots in the shared
                // inn choreography. CalcObjPtr redirects those to the dummy;
                // its passability bits cannot affect any real actor.
                const auto identity = resolve(next, op.object);
                if (!identity)
                    return;
                auto& object = next.field.objects[*identity];
                object.movement.options =
                    static_cast<std::uint8_t>(op.ignore_occupants ? object.movement.options | 1U
                                                                  : object.movement.options & 14U);
            } else if constexpr (std::is_same_v<T, EventObjectVehicle>) {
                require(op.vehicle < 4, "Invalid event vehicle identity");
                // C0/9CCA uses CalcObjPtr's inactive dummy for an empty party
                // slot. GameOver clears all four riders, even in a smaller party.
                const auto identity = resolve(next, op.object);
                if (!identity)
                    return;
                auto& object = next.field.objects[*identity];
                // Event 44 replaces all vehicle/rider bits. Object action C9
                // instead ORs them; using that action here would retain old bits.
                object.ride = static_cast<FieldRide>(op.vehicle);
                object.rider_visible = op.show_rider;
                object.special_graphics = object.ride == FieldRide::none && op.show_rider;
                if (object.special_pose)
                    object.special_pose->speed = op.vehicle;
            } else if constexpr (std::is_same_v<T, EventPartyObjects>) {
                // Event 3C may clear every temporary selector. C0/714A still
                // enumerates enabled active-party objects in identity order;
                // these selectors never rewrite persistent battle positions.
                next.field.party_selectors.fill(std::nullopt);
                for (std::size_t i = 0; i < op.objects.size() && op.objects[i]; ++i)
                    next.field.party_selectors[i] = object_target(next, *op.objects[i]);
                next.field.sort_pending = true;
            } else if constexpr (std::is_same_v<T, EventSortObjects>) {
                // Sorting is deferred to the ordinary four-visit scheduling
                // boundary. ContinueEvent waits on this flag in the meantime.
                next.field.sort_pending = true;
            } else if constexpr (std::is_same_v<T, EventActivateParty>) {
                require(op.party < 8, "Invalid active field party");
                next.roster.active_party = op.party;
                next.field.party_selectors.fill(std::nullopt);
            } else if constexpr (std::is_same_v<T, EventPartyMap>) {
                require(op.party < 8 && op.map < 512, "Invalid party map");
                for (unsigned id = 0; id < 16; ++id) {
                    auto& object = next.field.objects[id];
                    if (object.membership.enabled && object.membership.party == op.party)
                        object.map = op.map;
                }
            } else if constexpr (std::is_same_v<T, EventObjectEvent>) {
                require(op.entry <= 0xffffff, "Invalid field interaction entry");
                auto& value = next.field.objects[object_target(next, op.object)].interaction_data;
                value = op.entry;
            } else if constexpr (std::is_same_v<T, EventContactActivation>) {
                auto& options = next.field.objects[object_target(next, op.object)].movement.options;
                options = static_cast<std::uint8_t>(op.enabled ? options | 4U : options & 11U);
            } else if constexpr (std::is_same_v<T, EventCharacterStatus>) {
                require(op.change == EventStatusChange::remove ||
                            op.change == EventStatusChange::add ||
                            op.change == EventStatusChange::toggle,
                        "Invalid character status operation");
                if (const auto identity = character_target(next, op.object)) {
                    auto& character = next.roster.characters[*identity];
                    const auto status =
                        static_cast<std::uint16_t>(unsigned{character.status} |
                                                   (unsigned{character.persistent_effects} << 8U));
                    const auto updated = op.change == EventStatusChange::remove
                                             ? status & (op.mask ^ 0xffffU)
                                         : op.change == EventStatusChange::add ? status | op.mask
                                                                               : status ^ op.mask;
                    character.status = wrap8(updated);
                    character.persistent_effects = wrap8(updated >> 8U);
                }
            } else if constexpr (std::is_same_v<T, EventCharacterVital>) {
                const unsigned index = op.amount & 127U;
                require(index == 127 || index < (op.mp ? 7U : 8U), "Unowned field HP/MP amount");
                if (const auto identity = character_target(next, op.object)) {
                    auto& character = next.roster.characters[*identity];
                    auto& vital = op.mp ? character.mp : character.hp;
                    const auto maximum = maximum_vital(vital, op.mp ? 999 : 9999);
                    // EventCmd_8b/8c select independent ROM amount tables. The
                    // extractor proves the HP doubling series and equal MP
                    // entries before preserving this native expression shape.
                    const unsigned amount = op.mp ? unsigned{FF6_ROM_VALUE_FIELD_EVENT_MP_QUANTITY_0}
                        : index == 127 ? 0 : unsigned{FF6_ROM_VALUE_FIELD_EVENT_HP_BASE_0} << index;
                    if (index == 127) vital.current = maximum;
                    else if (!(op.amount & 128U))
                        vital.current = static_cast<std::uint16_t>(std::min(unsigned{maximum}, unsigned{vital.current} + amount));
                    else if (vital.current)
                        vital.current = static_cast<std::uint16_t>(std::max(op.mp ? 0 : 1, int{vital.current} - int(amount)));
                }
            } else if constexpr (std::is_same_v<T, EventRemoveEquipment>) {
                require(op.character < next.roster.characters.size(),
                        "Equipment removal outside native character roster");
                auto& character = next.roster.characters[op.character];
                character.esper.reset();
                for (auto& item : character.equipment) {
                    if (item == 255)
                        continue;
                    // EventCmd_8d clears each slot even at the quantity cap or
                    // when no inventory slot is available. Existing identities
                    // take precedence over empty slots; keep their byte semantics.
                    if (std::ranges::any_of(next.inventory.slots, [item](const auto& slot) {
                            return slot.item == item || slot.item == 255;
                        }))
                        give_inventory_item(next.inventory, item);
                    item = 255;
                }
                refresh_field_equipment(next, party_data, equipment);
            } else if constexpr (std::is_same_v<T, EventInventoryItem>) {
                if (op.give)
                    give_inventory_item(next.inventory, op.item);
                else
                    take_inventory_item(next.inventory, op.item);
            } else if constexpr (std::is_same_v<T, EventInventoryEsper>) {
                require(op.esper < 27, "Invalid event esper identity");
                const auto mask = std::uint32_t{1} << op.esper;
                // C0/ADB8–AE0C: acquiring magicite updates ownership only;
                // taking it also releases it from every character, including
                // those outside the active party. Learned spells are retained.
                if (op.give) next.roster.espers_owned |= mask;
                else {
                    next.roster.espers_owned &= ~mask;
                    for (auto& character : next.roster.characters)
                        if (character.esper == op.esper) character.esper.reset();
                }
            } else if constexpr (std::is_same_v<T, EventLearnTechniques>) {
                // C0/AFF8 gives all SwdTechs; C0/B002 adds Bum Rush without
                // granting unlearned earlier Blitzes. Neither changes levels.
                next.roster.abilities.bushido |= op.bushido;
                next.roster.abilities.blitz |= op.blitz;
            } else if constexpr (std::is_same_v<T, EventInventoryGil>) {
                if (op.give)
                    give_inventory_gil(next.inventory, op.amount);
                else
                    next.flags.story[0x1be] = !take_inventory_gil(next.inventory, op.amount);
            } else if constexpr (std::is_same_v<T, EventParentMap>) {
                require(op.facing < 4, "Invalid parent-map facing");
                next.field.parent_map = {op.map, op.x, op.y, op.facing};
            } else if constexpr (std::is_same_v<T, EventPartyControl>) {
                require(next.field.update_order[0].has_value(),
                        "Field update order is not initialized");
                auto& object = next.field.objects[*next.field.update_order[0]];
                object.movement = {static_cast<std::uint8_t>(op.player ? 2 : 1), 0};
                // Returning player control also replaces the saved mode that
                // the root FE later restores. Script control leaves it intact.
                if (op.player)
                    object.saved_movement = object.movement;
            } else if constexpr (std::is_same_v<T, EventRestorePartyMovement>) {
                require(next.field.update_order[0].has_value(),
                        "Field update order is not initialized");
                auto& object = next.field.objects[*next.field.update_order[0]];
                object.movement = object.saved_movement;
            } else if constexpr (std::is_same_v<T, EventObjectScript>) {
                require(op.entry <= 0xffffff, "Invalid object-action script offset");
                // ExecPartyObjScript redirects an absent/foreign party slot to
                // an inactive dummy. It never runs that object's actions. This
                // is common in shared scripts addressing all four party slots.
                if (const auto target = resolve(next, op.object)) {
                    auto& object = next.field.objects[*target];
                    object.movement.mode = 1;
                    object.action_steps = 0;
                    object.action_script = op.entry;
                }
            } else if constexpr (std::is_same_v<T, EventVariable>) {
                require(op.identity < next.event_variables.size() &&
                            static_cast<unsigned>(op.change) <= 3,
                        "Invalid event variable operation");
                auto& value = next.event_variables[op.identity];
                // E9/EA saturate, unlike ordinary machine-width wrapping.
                // EB replaces all sixteen case flags, not just the low three.
                switch (op.change) {
                case EventVariableChange::assign:
                    value = op.value;
                    break;
                case EventVariableChange::add:
                    value = static_cast<std::uint16_t>(
                        std::min(std::uint32_t{value} + op.value, std::uint32_t{65535}));
                    break;
                case EventVariableChange::subtract:
                    value = value < op.value ? 0 : static_cast<std::uint16_t>(value - op.value);
                    break;
                case EventVariableChange::compare: {
                    const unsigned result = value == op.value ? 0 : value > op.value ? 1 : 2;
                    for (unsigned bit = 0; bit < 16; ++bit)
                        next.flags.story[0x1a0 + bit] = bit == result;
                    break;
                }
                }
            } else if constexpr (std::is_same_v<T, EventCharacterMask>) {
                // E0/E1 copy separate initialized/available masks. DF scans
                // enabled field objects, including party zero and hidden actors.
                for (unsigned id = 0; id < 16; ++id)
                    next.flags.story[0x1a0 + id] =
                        op.source == CharacterMaskSource::initialized ? next.flags.story[0x2e0 + id]
                        : op.source == CharacterMaskSource::available
                            ? (next.roster.recruited & (1U << id)) != 0
                            : op.source == CharacterMaskSource::enabled_objects
                                ? next.field.objects[id].membership.enabled
                                : next.field.objects[id].membership.party != 0;
            } else if constexpr (std::is_same_v<T, EventPartyLeader>) {
                // Event E2 scans persistent party order, unlike DE's field
                // object membership. Visibility, KO and the camera do not
                // select the speaker. Equal positions retain the first ID.
                std::optional<std::size_t> leader;
                unsigned first_position = 4;
                for (std::size_t id = 0; id < next.roster.membership.size(); ++id) {
                    const auto& member = next.roster.membership[id];
                    if (member.party == next.roster.active_party &&
                        member.position < first_position) {
                        leader = id;
                        first_position = member.position;
                    }
                }
                require(leader.has_value(), "Party leader query has no active-party member");
                for (std::size_t id = 0; id < 16; ++id)
                    next.flags.story[0x1a0 + id] = id == *leader;
            } else if constexpr (std::is_same_v<T, EventActivePartyNumber>) {
                // C0/B39E clears the high byte and shifts an eight-bit value
                // by the active group number. These are group bits, not actors.
                for (unsigned bit = 0; bit < 16; ++bit)
                    next.flags.story[0x1a0 + bit] = bit < 8 && bit == next.roster.active_party;
            } else if constexpr (std::is_same_v<T, EventActivePartyMembers>) {
                // Event DE reads field membership, even for hidden/disabled
                // objects and party zero. Persistent roster membership may differ.
                for (std::size_t id = 0; id < 16; ++id)
                    next.flags.story[0x1a0 + id] =
                        next.field.objects[id].membership.party == next.roster.active_party;
            } else {
                static_assert(
                    std::is_same_v<T, EventSpritePalette> || std::is_same_v<T, EventDialogue> ||
                        std::is_same_v<T, EventStartBattle> ||
                        std::is_same_v<T, EventDialogueSelection> ||
                        std::is_same_v<T, EventRunCutscene> || std::is_same_v<T, EventLoadMenu> ||
                        std::is_same_v<T, EventRestartGame> ||
                        std::is_same_v<T, EventClearOverlay> ||
                        std::is_same_v<T, EventLoadPalette> || std::is_same_v<T, EventShop> ||
                        std::is_same_v<T, EventNameMenu> || std::is_same_v<T, EventColosseumMenu> || std::is_same_v<T, EventPartyMenu> || std::is_same_v<T, EventFinalOrder> ||
                        std::is_same_v<T, EventScreenMosaic> ||
                        std::is_same_v<T, EventBackgroundRate> || std::is_same_v<T, EventFlashlight> || std::is_same_v<T, EventPyramid> || std::is_same_v<T, EventScreenFlash> || std::is_same_v<T, EventFixedColor> || std::is_same_v<T, EventResumeSong> ||
                        std::is_same_v<T, EventUpdateParty> || std::is_same_v<T, EventMapLoad> ||
                        std::is_same_v<T, EventMapEdit> || std::is_same_v<T, EventApplyMapEdits> ||
                        std::is_same_v<T, EventStopScroll> ||
                        std::is_same_v<T, EventBackgroundScroll> ||
                        std::is_same_v<T, EventCameraLock> || std::is_same_v<T, EventScreenFade> ||
                        std::is_same_v<T, EventScreenShake> ||
                        std::is_same_v<T, EventBackgroundPalette> ||
                        std::is_same_v<T, EventRestoreColorMath> ||
                        std::is_same_v<T, EventPaletteFilter> ||
                        std::is_same_v<T, EventDefaultParty> ||
                        std::is_same_v<T, EventPartyPosition> ||
                        std::is_same_v<T, EventStopScreenFade> ||
                        std::is_same_v<T, EventWaitObject> ||
                        std::is_same_v<T, EventGameSoundEffect> ||
                        std::is_same_v<T, EventPlaySong> || std::is_same_v<T, EventFadeSong> ||
                        std::is_same_v<T, EventPortrait> ||
                        std::is_same_v<T, EventContinueSong> || std::is_same_v<T, EventStopSound> ||
                        std::is_same_v<T, EventMusicAdjustment> ||
                        std::is_same_v<T, EventSetTimer> || std::is_same_v<T, EventResetTimer> ||
                        std::is_same_v<T, EventRandomBranch> || std::is_same_v<T, EventOptimizeEquipment> ||
                        std::is_same_v<T, PlaybackVolumeRamp> || std::is_same_v<T, EffectPanRamp> ||
                        std::is_same_v<T, EventDeleteObject> || std::is_same_v<T, EventHideObject>,
                    "New event operations need an explicit field-owner decision");
                throw std::logic_error("External field operation reached the state-only owner");
            }
        },
        operation);
    state = std::move(next);
    return true;
}
bool consume_field_request(FieldState& state, FieldEventRunner& runner, const PartyData& party_data,
                           const EquipmentData& equipment) {
    require(runner.pending_request().has_value(), "No pending field event request");
    const auto& request = *runner.pending_request();
    if (const auto* wait = std::get_if<EventWaitObject>(&request.operation)) {
        validate(state);
        require(wait->object <= 52, "Invalid event wait reference");
        const auto target = wait->object <= FieldObjects::camera
                                ? std::optional{wait->object}
                                : state.field.update_order[wait->object - 49U];
        require(target.has_value(), "Event wait targets an unrepresented dummy object");
        FieldFlagAccess flags(state);
        runner.complete_request(request.identity, flags, std::nullopt, target);
        return true;
    }
    std::optional<std::uint8_t> script_object;
    if (const auto* operation = std::get_if<EventObjectScript>(&request.operation)) {
        validate(state);
        script_object = resolve(state, operation->object);
        // The original absent-slot fallback resets the wait selector to SLOT_1
        // while assigning its script to an inactive dummy. A synchronous call
        // therefore waits for the displayed actor, not for an invented object.
        if (!script_object)
            script_object = state.field.update_order[0];
        require(script_object.has_value(), "Object script has no selected wait target");
    }
    if (!apply_field_operation(state, request.operation, party_data, equipment))
        return false;
    FieldFlagAccess flags(state);
    runner.complete_request(request.identity, flags, std::nullopt, script_object);
    return true;
}
} // namespace ff6
