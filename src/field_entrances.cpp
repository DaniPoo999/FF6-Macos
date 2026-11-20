#include "ff6/field_entrances.hpp"
#include "rom-materials/field_transition_fade.macros.inc"
#include "rom-materials/field_transition_brightness.macros.inc"
#include "rom-materials/field_transition_step.macros.inc"

namespace ff6 {
// All original entrance branches use the same FadeOut selections as map events.
static_assert(FF6_ROM_VALUE_FIELD_TRANSITION_FADE_0 ==
              (FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0 |
               (FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0 << 8U)));
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
bool contains(const FieldEntrance& entry, FieldTilePosition point, bool long_form) {
    if (!long_form)
        return entry.position == point;
    require(entry.length < 128, "Invalid long entrance length");
    const unsigned start = entry.vertical ? entry.position.y : entry.position.x;
    const unsigned along = entry.vertical ? point.y : point.x;
    const bool cross = entry.vertical ? entry.position.x == point.x : entry.position.y == point.y;
    // Addition is eight-bit, but the lower-bound test is independent: a wrapped
    // end does not turn the region into a wrapped interval.
    return cross && along >= start && along <= wrap8(start + entry.length);
}
} // namespace
FieldEntrances load_field_entrances(const ContentPack& pack, std::uint16_t map) {
    require(map < 512, "Entrance map outside pointer table");
    FieldEntrances result;
    const auto load = [&](const char* pointer_id, const char* data_id, bool long_form) {
        const auto pointers = pack.get(pointer_id), data = pack.get(data_id);
        const unsigned stride = long_form ? 7U : 6U, base = 1026;
        require(pointers.size() == base && data.size() % stride == 0,
                "Invalid entrance resource extent");
        const unsigned first = read16(pointers, map * 2U), end = read16(pointers, map * 2U + 2U);
        require(first >= base && end >= first && end - base <= data.size() &&
                    (first - base) % stride == 0 && (end - first) % stride == 0,
                "Invalid entrance record range");
        auto& entries = long_form ? result.long_regions : result.short_regions;
        for (unsigned at = first - base; at < end - base; at += stride) {
            const unsigned header_at = at + (long_form ? 3U : 2U);
            const auto header = read16(data, header_at);
            entries.push_back({{data[at], data[at + 1]},
                               {data[header_at + 2], data[header_at + 3]},
                               static_cast<std::uint16_t>(header & 511U),
                               static_cast<std::uint8_t>(long_form ? data[at + 2] & 127U : 0U),
                               static_cast<std::uint8_t>((header >> 12U) & 3U),
                               long_form && (data[at + 2] & 128U) != 0,
                               (header & 512U) != 0,
                               (header & 1024U) != 0,
                               (header & 2048U) != 0});
        }
        auto& names = long_form ? result.long_parent_names : result.short_parent_names;
        for (unsigned i = 0; i < names.size(); ++i)
            names[i] = (pointers[i + (long_form ? 4U : 3U)] & 8U) != 0;
    };
    load("field_structure_long_entrance_ptr_bin", "field_structure_long_entrance_bin", true);
    load("field_structure_short_entrance_ptr_bin", "field_structure_short_entrance_bin", false);
    return result;
}
FieldEntranceResult check_field_entrances(FieldObjects& field, FieldMapTransition& transition,
                                          FieldEntranceMemory& memory,
                                          const FieldEntrances& entrances,
                                          FieldEntranceInputs inputs) {
    FieldEntranceResult result;
    if (transition.pending || inputs.menu_opening || inputs.disabled || inputs.battle_pending)
        return result;
    require(field.current_map >= 3 && field.current_map < 512,
            "World-map entrances require the world travel owner");
    require(field.update_order[0] && *field.update_order[0] < field.objects.size(),
            "Entrance check requires a selected actor");
    const auto& actor = field.objects[*field.update_order[0]];
    require(actor.x <= 0xffffff && actor.y <= 0xffffff, "Invalid entrance actor position");
    if (((actor.x >> 8U) & 15U) != 0 || ((actor.y >> 8U) & 15U) != 0 || inputs.event_running ||
        ((inputs.terrain_flags & 4U) != 0 && inputs.level != 1))
        return result;
    auto next = memory;
    auto parent = field.parent_map;
    unsigned header = 0;
    FieldTilePosition world_position{};
    bool requested = false;
    std::optional<std::uint8_t> return_facing;
    const auto apply = [&](const FieldEntrance& entry, bool long_form) {
        require(entry.map < 512 && entry.facing < 4, "Invalid entrance destination");
        // Canonical field-map records never take this path. The original helper
        // returns with eight-bit A into a caller expecting sixteen-bit A; treating
        // it as an ordinary semantic store-parent would invent different behavior.
        // Actual store-parent entries belong to the distinct world-map caller.
        require(!entry.set_parent, "Store-parent entrance requires the world travel owner");
        header = entry.map | (unsigned{entry.set_parent} << 9U) |
                 (unsigned{entry.lower_level} << 10U) | (unsigned{entry.show_name} << 11U) |
                 (unsigned{entry.facing} << 12U);
        bool save_characters = true;
        if (entry.map == 511) {
            require(parent.map < 512, "Parent entrance lookup outside owned pointer data");
            const unsigned facing = (parent.facing & 3U) ^ 2U;
            constexpr std::array<unsigned, 4> dx{0, 1, 0, 255}, dy{255, 0, 1, 0};
            parent.x = wrap8(unsigned{parent.x} + dx[facing]);
            parent.y = wrap8(unsigned{parent.y} + dy[facing]);
            next.return_facing = static_cast<std::uint8_t>(facing | 128U);
            return_facing = next.return_facing;
            header = (header & 0xfe00U) | parent.map;
            if (parent.map < 3) {
                world_position = {parent.x, parent.y};
                requested = true;
                save_characters = long_form;
            } else {
                next.placement.facing = static_cast<FieldFacing>(facing);
                next.placement.preserve_facing = true;
                next.placement.show_name = long_form ? entrances.long_parent_names[parent.map]
                                                     : entrances.short_parent_names[parent.map];
                next.run_startup = next.fade_in = true;
                next.keep_dimensions = false;
                next.world_vehicle = 0;
                // The long path never enables loading or saves the party.
                // The short path writes saved scroll, not destination placement.
                if (!long_form) {
                    result.saved_scroll_position = FieldTilePosition{parent.x, parent.y};
                    requested = true;
                }
                save_characters = !long_form;
            }
        } else if (entry.map < 3) {
            world_position = entry.destination;
            requested = true;
        } else {
            next.placement = {entry.destination, static_cast<FieldFacing>(entry.facing),
                              entry.show_name};
            next.saved_level = entry.lower_level ? 2 : 1;
            next.run_startup = next.fade_in = true;
            next.keep_dimensions = false;
            next.world_vehicle = 0;
            requested = true;
        }
        if (save_characters)
            next.characters = save_field_character_locations(field);
        if (!long_form)
            result.clear_special_npc_count = true;
    };
    const auto scan = [&](const auto& entries, bool long_form) -> std::optional<std::size_t> {
        for (std::size_t i = 0; i < entries.size(); ++i)
            if (contains(entries[i], inputs.tile, long_form)) {
                apply(entries[i], long_form);
                return i;
            }
        return std::nullopt;
    };
    result.long_match = scan(entrances.long_regions, true);
    result.short_match = scan(entrances.short_regions, false);
    if (!result.matched())
        return result;
    auto next_transition = transition;
    next_transition.fade = {FF6_ROM_VALUE_FIELD_TRANSITION_BRIGHTNESS_0,
                            FF6_ROM_VALUE_FIELD_TRANSITION_STEP_0, FieldFadeDirection::darker};
    if (requested) {
        require(inputs.idle_event <= 0xffffff && next.world_vehicle < 4,
                "Invalid entrance world continuation");
        FieldMapLoadPlan plan;
        plan.map = static_cast<std::uint16_t>(header & 511U);
        plan.dispatch_world = (header & 1023U) < 3;
        if (plan.map < 3)
            plan.arrival =
                WorldMapArrival{world_position, static_cast<FieldFacing>((header >> 12U) & 3U),
                                next.world_vehicle, inputs.idle_event};
        else
            plan.arrival = next.placement;
        plan.characters = next.characters;
        plan.parent = parent;
        plan.return_facing = return_facing;
        plan.run_startup = next.run_startup;
        plan.fade_in = next.fade_in;
        plan.keep_dimensions = next.keep_dimensions;
        plan.saved_level = next.saved_level;
        next_transition.pending = std::move(plan);
    }
    memory = std::move(next);
    field.parent_map = parent;
    transition = std::move(next_transition);
    return result;
}
} // namespace ff6
