#include "ff6/field_load_menu.hpp"
#include "rom-materials/new_game_menu_defaults.macros.inc"

namespace ff6 {
bool complete_field_load_without_save(FieldLoopSeed& retained, OriginalSram& storage,
                                      const FieldLoopAssets& assets) {
    const auto& request = retained.events.pending_request();
    if (!request || !std::holds_alternative<EventLoadMenu>(request->operation))
        throw std::invalid_argument("No load-menu event to complete");
    if (storage.valid_header())
        return false;

    auto next = retained;
    auto saved = storage;
    auto& field = next.state.field;
    const auto leader = field.update_order[0];
    if (!leader || *leader >= field.objects.size())
        throw std::invalid_argument("Load-menu entry needs a selected field object");
    // PushCharFlags retains persistent battle order; PopCharFlags later returns
    // that order to live objects. Do not replace the character roster.
    save_field_membership(next.state);
    for (std::size_t id = 0; id < next.state.roster.membership.size(); ++id) {
        next.menu_return.character_palettes[id] = field.objects[id].upper.palette;
    }
    next.travel.characters = save_field_character_locations(field);
    next.menu_return.scroll = {next.actions.scroll.layers[0][0].center,
                               next.actions.scroll.layers[0][1].center};
    next.menu_return.party = {next.actions.player_tile_position[0],
                              next.actions.player_tile_position[1]};
    next.menu_return.leader = *leader;
    next.menu_return.facing = field.objects[*leader].facing;
    next.travel.saved_level = next.actions.player_terrain.level;

    // InitSaveSlot runs before header validation even on this no-UI branch.
    next.menu = new_game_menu_defaults(FF6_ROM_VALUE_NEW_GAME_MENU_DEFAULTS_0,
                                      FF6_ROM_VALUE_NEW_GAME_MENU_DEFAULTS_1 != 0,
                                      FF6_ROM_VALUE_NEW_GAME_MENU_DEFAULTS_2);
    next.font_color = 0x7fff;
    next.window_skin = 0;
    next.window_colors = assets.dialogue.colors;
    for (const auto& palette : *next.window_colors)
        for (const auto color : palette)
            if (color > 0x7fff)
                throw std::invalid_argument("Invalid default window color");
    next.state.inventory.gil = 0;
    next.player.effects.steps = 0;
    // Shared repeat delays reset; the caller's saved field button history and
    // retained menu counters survive the menu's separate input scratch state.
    initialize_input(next.input, next.settings, next.menu.saved_buttons);
    if (saved.begin_load())
        throw std::logic_error("No-save entry unexpectedly accepted SRAM");
    for (std::size_t id = 0; id < next.state.roster.membership.size(); ++id)
        field.objects[id].membership = next.state.roster.membership[id];
    restore_field_character_maps(field, next.travel.characters);
    next.travel.run_startup = true;
    next.transition.fade.brightness = 0;

    const auto seed = saved.finish_load();
    next.actions.random_index = seed;
    next.battle_random = {seed, seed, seed, seed, seed};
    refresh_field_equipment(next.state, assets.party, assets.equipment);
    FieldFlagAccess flags(next.state);
    next.events.complete_request(request->identity, flags, {}, {}, {}, EventLoadResult::new_game);
    retained = std::move(next);
    storage = std::move(saved);
    return true;
}

bool FieldLoop::complete_load_without_save(OriginalSram& storage) {
    if (phase_ != Phase::frame_events && phase_ != Phase::startup_events &&
        phase_ != Phase::startup_extra)
        throw std::logic_error("No retained load-menu event visit to resume");
    if (!complete_field_load_without_save(owned_, storage, assets_))
        return false;
    display_.set_window_colors((*owned_.window_colors)[owned_.window_skin]);
    owned_.palette = display_.palette();
    return true;
}
} // namespace ff6
