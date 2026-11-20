#include "ff6/field_boot.hpp"

namespace ff6 {
FieldLoopSeed initialize_field_new_game(FieldLoopSeed retained, const ContentPack& pack,
                                        const PartyData& party) {
    const auto active_party = retained.state.roster.active_party;
    retained.state.roster = new_roster(party);
    retained.state.roster.active_party = active_party;
    retained.state.inventory.slots.fill({});
    retained.battle_conditions.reset();
    retained.battle_command_rules = {};
    retained.saved_doom_gaze_hp = 0;
    initialize_field_flags(retained.state, pack);
    initialize_field_treasure_flags(retained.state.flags);

    retained.window_skin = 0;
    retained.font_color = 0x7fff;
    const auto colors = pack.get("menu_graphics_window_pal_pal");
    if (colors.size() != 8 * 32)
        throw std::invalid_argument("Invalid initial window palette resource");
    WindowPalettes windows;
    for (std::size_t theme = 0; theme < windows.size(); ++theme)
        for (std::size_t i = 0; i < windows[theme].size(); ++i)
            windows[theme][i] = read16(colors, theme * 32 + 2 + i * 2) & 0x7fffU;
    retained.window_colors = windows;
    retained.travel.saved_level = 1;
    retained.party_levels.fill(1);
    retained.world.reset();
    retained.world_minimap = true;
    retained.actions.camera_locked = false;

    // These are the named NoEvent and GameStart_ext entries of the extracted
    // event-language ABI. A root return restores movement and parks at NoEvent.
    retained.events = FieldEventRunner::parked(field_event_script(pack), 0);
    retained.events.begin_object_event(3);
    retained.state.field.objects[0].saved_movement = {2, 0};
    retained.state.field.event_visits = 0;
    return retained;
}
FieldMapArrival decode_field_boot_arrival(ByteView data) {
    if (data.size() != 4 || read16(data, 0) < 3 || read16(data, 0) >= 415)
        throw std::invalid_argument("Invalid initial field destination");
    return {{data[2], data[3]}, FieldFacing::up, false, false};
}
FieldLoopSeed make_initial_field_seed(const ContentPack& pack) {
    FieldLoopSeed reset(FieldEventRunner::parked(field_event_script(pack), 0));
    // ClearRAM's zero object pointers mean object zero, not an empty selector.
    // The first map sort truncates disabled selectors and retains its order tail.
    reset.state.field.party_selectors.fill(0);
    reset.state.field.update_order.fill(0);
    reset.state.field.animation_queue.fill(0);
    for (auto& object : reset.state.field.objects)
        object.animation_slot = 0;
    initialize_input(reset.input, reset.settings, default_button_map());
    auto initialized = initialize_field_new_game(std::move(reset), pack, load_party_data(pack));
    const auto destination = pack.get("field_boot_destination");
    FieldMapLoadPlan plan;
    plan.arrival = decode_field_boot_arrival(destination);
    plan.map = read16(destination, 0);
    plan.characters = initialized.travel.characters;
    plan.fade_in = false;
    // Reset sets only NO_FADE_IN; it does not request a map startup event.
    initialized.transition.pending = plan;
    return initialized;
}
FieldSession make_initial_field_session(const ContentPack& pack) {
    auto seed = make_initial_field_seed(pack);
    auto assets = load_field_map_entry_assets(pack, seed.transition.pending->map, FieldMap{});
    auto prepared = prepare_field_map_entry(std::move(assets), std::move(seed));
    return FieldSession(FieldLoop(std::move(prepared.assets), std::move(prepared.seed)));
}
} // namespace ff6
