#include "ff6/field_map_entry.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
} // namespace
FieldMapEntryAssets load_field_map_entry_assets(const ContentPack& pack, std::uint16_t identity,
                                                const FieldMap& previous_display) {
    FieldMapEntryAssets result;
    result.identity = identity;
    result.scene = load_field_loop_assets(pack, identity, &previous_display);
    result.npcs = load_field_npcs(pack, identity);
    result.startup = field_map_startup_event(pack, identity);
    return result;
}
PreparedFieldMapEntry prepare_field_map_entry(FieldMapEntryAssets assets, FieldLoopSeed retained,
                                                bool reloading_same_map) {
    require(retained.transition.ready_to_load(), "Destination is not ready to load");
    const auto plan = *retained.transition.pending;
    reloading_same_map = reloading_same_map || plan.reload_same_map;
    require(!plan.dispatch_world && std::holds_alternative<FieldMapArrival>(plan.arrival),
            "World destinations require the world-map owner");
    require(assets.identity == plan.map && plan.map >= 3 && plan.map < 415,
            "Destination resources do not match the pending map");
    require(!plan.keep_dimensions || reloading_same_map, "Retained-dimension new map entry is not implemented");
    require(!reloading_same_map || retained.state.field.current_map == plan.map,
            "Reload destination must be the retained field map");
    require(!retained.events.pending_request() && !retained.startup,
            "Cannot load through an unfinished event/startup operation");
    require(retained.window_skin < assets.scene.dialogue.colors.size(), "Invalid window theme");
    auto& state = retained.state;
    auto& field = state.field;
    auto& map = assets.scene.map;
    const auto arrival = std::get<FieldMapArrival>(plan.arrival);
    require(static_cast<unsigned>(arrival.facing) < 4, "Invalid destination facing");

    retained.travel.placement = arrival;
    retained.travel.characters = plan.characters;
    if (plan.saved_level)
        retained.travel.saved_level = *plan.saved_level;
    if (plan.return_facing)
        retained.travel.return_facing = *plan.return_facing;
    retained.travel.run_startup = plan.run_startup;
    retained.travel.fade_in = plan.fade_in;
    retained.travel.keep_dimensions = plan.keep_dimensions;
    field.parent_map = plan.parent;
    field.current_map = plan.map;
    field.event_visits = 0;
    retained.player.effects.entrances_disabled = true;
    // LoadMap skips the door-history clear when returning from battle/menu
    // on this map. DrawOpenDoor then restores those openings before startup.
    if (!reloading_same_map) {
        reset_field_doors(retained.player.doors);
        // LoadMap clears the active window, but retains its previous radius.
        retained.flashlight.active = false;
        // Masks belong to the current map's decoded atlas. A scripted arrival
        // can retain non-player movement, so UpdateOverlay will not replace
        // them yet. Do not reinterpret the previous map's image identities.
        clear_field_overlay(retained.player.overlay);
    }
    restore_field_character_positions(field, plan.characters);
    sort_field_objects(state);
    place_field_player(field, arrival.position.x, arrival.position.y);
    retained.actions.scroll_limits = field_scroll_limits(map.definition);
    retained.actions.scroll = initialize_field_scroll(map.definition, map.parallax,
                                                      arrival.position.x, arrival.position.y);
    // GetTopChar is gated by the exact idle continuation, not by whether the
    // event happens to have no pending native request.
    if (retained.events.continuation() == EventContinuation::script &&
        retained.events.position() == assets.scene.player.idle_event)
        (void)select_field_party_leader(state);
    retained.transition.pending.reset();
    retained.transition.fade.brightness = 0;
    retained.player.effects.encounters_enabled = false;
    retained.actions.collision.stage = FieldCollision::Stage::none;
    retained.dialogue = FieldDialogue{};
    retained.ending_bars = state.flags.story[0x2c0];
    field.sort_pending = true;

    restore_field_treasures(map, state.flags, assets.scene.player.treasures,
                            assets.scene.player.treasure);
    restore_field_doors(map, retained.player.doors, assets.scene.player.doors);
    retained.palette = {map.colors, map.colors};
    const auto colors = retained.window_colors ? (*retained.window_colors)[retained.window_skin]
                                               : assets.scene.dialogue.colors[retained.window_skin];
    set_field_window_colors(retained.palette.base, retained.palette.pending, colors);
    std::copy_n(assets.scene.objects.sprites.palettes.begin(), 8, retained.object_colors.begin());
    retained.object_base_colors = retained.object_colors;
    refresh_field_equipment(state, assets.scene.party, assets.scene.equipment);
    state.flags.story.set(0x1b6);
    // LoadMap skips the two temporary-event flag bytes on a retained-map
    // reload, together with NPC/player initialization (field/init.asm).
    // Timed puzzles restore their edited tiles from these flags after battle
    // or menu return; clearing them can strand the party on an inactive bridge.
    if (!reloading_same_map)
        for (unsigned bit = 0x1f0; bit < 0x200; ++bit)
            state.flags.story.reset(bit);
    initialize_field_map_actors(state, map, assets.npcs, assets.scene.actions.poses, reloading_same_map,
                                arrival.preserve_facing ? std::nullopt
                                                        : std::optional{arrival.facing});
    // LoadMap clears this flag only after fresh NPC/player initialization.
    // Retained-map startup scripts inspect it to preserve in-flight actions
    // across menu, battle and party-switch returns instead of restarting routes.
    if (!reloading_same_map)
        state.flags.story.reset(0x1b6);
    prepare_field_special_graphics(field, assets.scene.objects, assets.npcs.size());
    // InitPortrait follows InitSpecialNPCs: its image deliberately overwrites
    // the shared atlas and pending palette 7, but not the base sprite palette.
    if (state.flags.story[0x1c6])
        prepare_field_portrait(assets.scene.objects, retained.portrait, retained.object_colors);
    retained.actions.occupancy =
        initialize_field_occupancy(map, field, *field.update_order[0], retained.actions.player_tile,
                                   retained.actions.player_terrain);
    // Keep the old tile/terrain/local cache through the real startup script.
    // InitZLevel refreshes it only after startup gives control back.
    retained.fade_in = plan.fade_in;
    retained.startup = plan.run_startup ? std::optional{assets.startup} : std::nullopt;
    retained.map_presentation_pending = true;
    return {std::move(assets.scene), std::move(retained)};
}
FieldLoop FieldLoop::load_destination(const ContentPack& pack) const {
    require(phase_ == Phase::stopped && stopped_ == FieldLoopBoundary::map_load,
            "Field scene is not at its destination-load boundary");
    auto assets = load_field_map_entry_assets(pack, owned_.transition.pending->map, display_.map());
    // Ordinary character clearing and vehicle loading do not replace this
    // prefix. New special definitions can overwrite it during preparation.
    std::copy_n(assets_.objects.special_tiles.begin(), 32,
                assets.scene.objects.special_tiles.begin());
    auto entry = prepare_field_map_entry(std::move(assets), owned_);
    return FieldLoop(std::move(entry.assets), std::move(entry.seed));
}
} // namespace ff6
