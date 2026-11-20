#include "ff6/field_treasure.hpp"
#include "rom-materials/field_treasure_choices.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::invalid_argument(message);
}
void validate(const FieldTreasureData& data, std::span<const FieldTreasureDefinition> definitions) {
    require(data.chest.width == 1 && data.chest.height == 1 && data.chest.tiles.size() == 1,
            "Invalid treasure chest patch");
    for (const auto& def : definitions)
        require(def.opened_flag < 512 && static_cast<unsigned>(def.kind) < 4,
                "Invalid native treasure definition");
}
} // namespace
FieldTreasureDefinition decode_field_treasure(ByteView record) {
    require(record.size() == 5, "Invalid treasure record extent");
    const auto descriptor = read16(record, 2);
    const auto kind = descriptor & 0x8000U   ? FieldTreasureKind::gil
                      : descriptor & 0x4000U ? FieldTreasureKind::item
                      : descriptor & 0x2000U ? FieldTreasureKind::monster
                                             : FieldTreasureKind::empty;
    return {{record[0], record[1]}, static_cast<std::uint16_t>(descriptor & 511U), record[4], kind};
}
std::vector<FieldTreasureDefinition> load_field_treasures(const ContentPack& pack,
                                                          std::uint16_t map) {
    const auto pointers = pack.get("field_structure_treasure_prop_ptr_bin");
    const auto records = pack.get("field_structure_treasure_prop_bin");
    require(pointers.size() == 416 * 2 && records.size() == 286 * 5 && map < 415,
            "Invalid treasure directory extent or map");
    require(read16(pointers, 0) == 0 && read16(pointers, 830) == records.size(),
            "Invalid treasure directory endpoints");
    for (unsigned i = 0; i < 415; ++i) {
        const auto a = read16(pointers, i * 2), b = read16(pointers, i * 2 + 2);
        require(a <= b && b <= records.size() && a % 5 == 0 && b % 5 == 0,
                "Invalid treasure directory range");
    }
    std::vector<FieldTreasureDefinition> result;
    for (unsigned at = read16(pointers, unsigned{map} * 2);
         at < read16(pointers, unsigned{map} * 2 + 2); at += 5)
        result.push_back(decode_field_treasure(records.subspan(at, 5)));
    return result;
}
FieldTreasureData load_field_treasure_data(const ContentPack& pack) {
    // These are the fixed system entries of the event DATA language, not CPU
    // addresses. The source callers and extracted script headers are pinned in
    // field_treasure_provenance.json and independently checked by private tests.
    FieldTreasureData result{decode_field_map_patch(pack.get("field_treasure_tile")),
                             {20, 8, 16, 64}};
    validate(result, {});
    const auto script = field_event_script(pack);
    for (const auto entry : result.events)
        require(entry + 3 < script.size() && script[entry] == 0x4b,
                "Invalid treasure system event entry");
    return result;
}
void initialize_field_treasure_flags(FieldFlags& flags) {
    for (unsigned i = 0; i < 384; ++i)
        flags.treasure.reset(i);
}
std::optional<FieldTreasureOpened>
try_collect_field_treasure(FieldState& state, FieldMap& map, FieldEventRunner& runner,
                           FieldTreasureContext& context, const FieldOccupancy& occupancy,
                           FieldTerrainCell cached_tile, FieldPlayerTerrain terrain,
                           std::span<const FieldTreasureDefinition> definitions,
                           const FieldTreasureData& data, FieldTreasureInputs inputs) {
    if (inputs.dialogue_open || inputs.menu_opening || inputs.map_loading || inputs.event_running ||
        ((cached_tile.flags & 4U) && terrain.level == 2) || !inputs.confirm_down)
        return {};
    validate(data, definitions);
    const auto player = state.field.update_order[0];
    require(player && *player <= FieldObjects::camera,
            "Treasure interaction needs a selected actor");
    const auto& actor = state.field.objects[*player];
    require(actor.facing < 4 && actor.movement.mode < 16 && actor.movement.options < 16,
            "Invalid treasure actor state");
    const auto layout = map.definition.layouts[0];
    require(occupancy.width == layout.width && occupancy.height == layout.height,
            "Treasure occupancy and map sizes disagree");
    (void)occupancy.at(0, 0); // Validate extents before using their clip masks.
    constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
    const FieldTilePosition target{
        static_cast<std::uint8_t>((actor.occupancy_tile.x + dx[actor.facing]) &
                                  (layout.width - 1U)),
        static_cast<std::uint8_t>((actor.occupancy_tile.y + dy[actor.facing]) &
                                  (layout.height - 1U))};
    if (occupancy.at(target.x, target.y))
        return {};
    const auto found = std::ranges::find(definitions, target, &FieldTreasureDefinition::position);
    if (found == definitions.end() || state.flags.treasure[found->opened_flag])
        return {};
    auto next = state;
    auto event = runner;
    auto messages = context;
    auto edited = map;
    next.flags.treasure.set(found->opened_flag);
    switch (found->kind) {
    case FieldTreasureKind::gil:
        messages.message_gil = unsigned{found->contents} * 100U;
        give_inventory_gil(next.inventory, static_cast<std::uint16_t>(messages.message_gil));
        break;
    case FieldTreasureKind::item:
        messages.message_item = found->contents;
        give_inventory_item(next.inventory, found->contents);
        break;
    case FieldTreasureKind::monster:
        messages.battle_formation = found->contents;
        break;
    case FieldTreasureKind::empty:
        break;
    }
    event.begin_object_event(data.events[static_cast<unsigned>(found->kind)]);
    auto& controlled = next.field.objects[*player];
    controlled.saved_movement = controlled.movement;
    controlled.movement = {4, 0};
    const bool chest = field_map_tile(edited, 0, target.x, target.y) ==
                       FF6_ROM_VALUE_FIELD_TREASURE_CHOICES_0;
    if (chest)
        apply_field_map_patch(edited, 0, target.x, target.y, data.chest);
    state = std::move(next);
    runner = std::move(event);
    context = messages;
    map.tile_edits = std::move(edited.tile_edits);
    return FieldTreasureOpened{*found, chest, static_cast<std::uint8_t>(
        chest ? FF6_ROM_VALUE_FIELD_TREASURE_CHOICES_1 : FF6_ROM_VALUE_FIELD_TREASURE_CHOICES_2)};
}
void restore_field_treasures(FieldMap& map, const FieldFlags& flags,
                             std::span<const FieldTreasureDefinition> definitions,
                             const FieldTreasureData& data) {
    validate(data, definitions);
    auto next = map;
    for (const auto& def : definitions)
        if (flags.treasure[def.opened_flag] &&
            field_map_tile(next, 0, def.position.x, def.position.y) ==
                FF6_ROM_VALUE_FIELD_TREASURE_CHOICES_0)
            apply_field_map_patch(next, 0, def.position.x, def.position.y, data.chest);
    map.tile_edits = std::move(next.tile_edits);
}
} // namespace ff6
