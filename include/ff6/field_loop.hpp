#pragma once
#include "ff6/battle_commands.hpp"
#include "ff6/driver_loading.hpp"
#include "ff6/field_battle.hpp"
#include "ff6/event_timer.hpp"
#include "ff6/field_contact.hpp"
#include "ff6/field_dialogue.hpp"
#include "ff6/field_display.hpp"
#include "ff6/field_entrances.hpp"
#include "ff6/field_map_startup.hpp"
#include "ff6/field_map_transition.hpp"
#include "ff6/field_map_updates.hpp"
#include "ff6/field_object_view.hpp"
#include "ff6/field_player.hpp"
#include "ff6/game_menu_state.hpp"
#include "ff6/menu_items.hpp"
#include "ff6/music_player.hpp"
#include "ff6/original_sram.hpp"
#include "ff6/world_map.hpp"

namespace ff6 {
struct FieldTileEvent {
    FieldTilePosition position;
    std::uint32_t event = 0;
};
struct FieldLoopAssets {
    FieldMap map;
    FieldAnimationAssets animation;
    FieldObjectViewAssets objects;
    FieldActionData actions;
    FieldPlayerData player;
    PartyData party;
    EquipmentData equipment;
    MenuItemData menu_items;
    FieldDialogueAssets dialogue;
    FieldBattleData battles;
    EventTimerGraphics timer_graphics;
    std::array<std::uint8_t, 256> window_sine{};
    std::vector<FieldTileEvent> triggers;
    FieldEntrances entrances;
    std::uint8_t map_title = 0;
    std::uint8_t battle_background = 0;
    bool random_encounters = false;
    std::uint8_t menu_permissions = 0;
};
FieldLoopAssets load_field_loop_assets(const ContentPack& pack, std::uint16_t map,
                                       const FieldMap* previous_display = nullptr);
// A zero default retains music; the story override explicitly requests even
// retained song zero. The existing native player owns duplicate-song handling.
std::optional<SongLoad> field_map_song(std::uint8_t map_song, bool override_song,
                                       std::uint8_t& retained_song);

struct FieldMenuMemory {
    // Palette identities, not the shifted attribute bits stored in original SRAM.
    std::array<std::uint8_t, 16> character_palettes{};
    FieldTilePosition scroll, party;
    std::uint8_t leader = 0, facing = 0;
    bool operator==(const FieldMenuMemory&) const = default;
};
struct FieldEventTimer {
    std::uint16_t remaining = 0;
    std::uint32_t event = 0; // Zero is disarmed; expiration clears only this owner.
    bool pause_in_menu_battle = false;
    bool visible_field = false, visible_menu_battle = false, terminate_external = false;
    bool operator==(const FieldEventTimer&) const = default;
};
// Explicit already-loaded entry, not a new-game initializer or a saved RAM image.
// Caller completes placement, equipment, actor/animation initialization and
// occupancy/colors, retaining the old player cache for startup.
struct FieldLoopSeed {
    explicit FieldLoopSeed(FieldEventRunner runner) : events(std::move(runner)) {}
    FieldState state{};
    std::optional<WorldState> world;
    std::optional<FieldTilePosition> parked_airship;
    std::optional<WorldFlightResume> airship_resume;
    bool world_minimap = true;
    FieldActionContext actions{};
    FieldPlayerContext player{};
    FieldEventRunner events;
    // Runtime-only, like the original unsaved field counters. Retained across
    // map/menu/battle handoffs, cleared when constructing a new or loaded game.
    std::array<FieldEventTimer, 4> timers{};
    FieldMapTransition transition;
    FieldEntranceMemory travel;
    InputState input;
    InputSettings settings;
    GameMenuState menu;
    FieldMenuMemory menu_return;
    FieldBattleRandom battle_random;
    std::uint16_t encounter_counter = 0;
    // Persistent conditional-formation switches. Cold new-game setup clears
    // them; event mutation and saved restoration remain explicit pending work.
    std::bitset<16> battle_conditions;
    BattleCommandRules battle_command_rules;
    std::uint16_t saved_doom_gaze_hp = 0;
    std::optional<FieldBattleRequest> battle;
    std::optional<ColosseumSelection> colosseum; // Runtime event-menu selection; never serialized.
    // A selected random encounter belongs to its field event until command 4E.
    // It must not interrupt the party regrouping by entering battle immediately.
    std::optional<FieldBattleRequest> selected_encounter;
    FieldPaletteState palette{};
    FieldShake shake;
    FieldColorEffect flash;
    FieldFlashlight flashlight;
    FieldPyramid pyramid;
    // E7 selects a portrait; map entry owns its publication. Fade bars are
    // likewise latched by LoadMap, not toggled by subsequent flag writes.
    std::uint8_t portrait = 0;
    bool ending_bars = false;
    std::array<std::array<Rgb555, 16>, 8> object_colors{}, object_base_colors{};
    Rgb555 font_color = 0x7fff;
    // User colors are separate from the map's animated palettes. Absent uses
    // the extracted default for window_skin when preparing a destination.
    std::optional<WindowPalettes> window_colors;
    FieldDialogue dialogue;
    std::uint8_t window_skin = 0, message_spell = 0;
    std::uint8_t map_song = 0;
    std::uint8_t suspended_song = 0;
    std::uint8_t vblank = 0;
    bool third_phase = false;
    // LoadMap starts the arrival fade only after releasing startup control.
    bool fade_in = false;
    bool party_switch_ready = false;
    std::array<std::uint8_t, 4> party_levels{1, 1, 1, 1};
    // Absent means InitZLevel and initial object publication are already due.
    std::optional<std::uint32_t> startup;
    // A real destination finishes music/title after startup. Earlier prepared
    // development fixtures explicitly omit these entry operations.
    bool map_presentation_pending = false;
};
enum class FieldLoopBoundary {
    startup_visit,
    frame,
    audio_wait,
    budget,
    event_request,
    map_load,
    encounter,
    battle,
    menu_or_party
};
struct FieldLoopResult {
    FieldLoopBoundary boundary;
    bool operator==(const FieldLoopResult&) const = default;
};
struct FieldSoundEffect {
    std::uint8_t effect, pan = 128;
    bool operator==(const FieldSoundEffect&) const = default;
};
using FieldAudioCommand =
    std::variant<SongLoad, FieldSoundEffect, PlaybackVolumeRamp, EffectPanRamp, MusicTempoRamp,
                 MusicPitchRamp, MusicCue, StopPlayback>;
// A retained ordinary field scene with an explicit one-logic-visit-per-display
// profile. A song wait permits display work while retaining the event visit;
// audio_ready comes from the native player's rendered command completion.
// Blank startup has no field display clock. Missing owners stay pending.
class FieldLoop {
  public:
    FieldLoop(FieldLoopAssets assets, FieldLoopSeed seed);
    FieldLoopResult advance(Buttons first = 0, Buttons second = 0,
                            std::size_t command_budget = 1024, bool audio_ready = false,
                            bool music_finished = false,
                            std::optional<std::uint8_t> music_position = std::nullopt);
    // Valid only at the map-load boundary. All decoding/initialization succeeds
    // before returning a replacement; failure leaves this scene and plan intact.
    FieldLoop load_destination(const ContentPack& pack) const;
    const FieldLoopSeed& state() const {
        return owned_;
    }
    const FieldMap& map() const {
        return assets_.map;
    }
    const FieldMapUpdates& updates() const {
        return updates_;
    }
    const FieldMapTransition& transition() const {
        return owned_.transition;
    }
    const std::optional<RasterScene>& visible() const {
        return visible_;
    }
    std::uint64_t frames() const {
        return frames_;
    }
    // Preserve event/startup order across effects and map-song selection.
    std::vector<FieldAudioCommand> take_audio();
    // Presentation intent retained until the caller consumes it.
    bool take_close_map_title();
    // Menu/battle pictures decrement eligible counters, but only the field
    // loop can dispatch an expired event after current script/movement ends.
    // Returns the shared battle-expiry signal sampled before decrement.
    bool advance_external_timers();

  private:
    friend class FieldSession;
    void check_timers();
    void complete_cutscene(std::uint64_t identity, EventCutscene expected, bool skipped);
    bool complete_load_without_save(OriginalSram& storage);
    void publish_battle_transition(Buttons buttons, std::uint8_t mosaic_size, bool visible);
    enum class Phase {
        startup_begin,
        startup_events,
        startup_extra,
        frame_begin,
        frame_events,
        audio_wait,
        after_events,
        stopped
    };
    FieldLoopAssets assets_;
    FieldLoopSeed owned_;
    FieldMapUpdates updates_;
    FieldDisplay display_;
    FieldSpritePublication publication_;
    // DrawObjSprites precedes contact activation. The next display must not
    // render the newly selected party/facing with the previous draw positions.
    FieldObjects drawn_;
    FieldPlayerTerrain drawn_terrain_;
    FieldOverlayState drawn_overlay_;
    FieldScroll drawn_scroll_;
    std::uint8_t drawn_vblank_ = 0;
    std::optional<RasterScene> visible_;
    // Published field pixels are retained independently of the live dialogue
    // window. Opening/closing bands can change in the current field visit;
    // glyph transfers and actor drawing retain their existing publication.
    std::optional<RasterScene> dialogue_underlay_;
    std::array<FieldOrigin, 3> published_origins_{};
    FieldColorEffect published_flash_;
    FieldWindowMask published_window_;
    std::uint8_t published_mosaic_ = 1;
    bool dialogue_backgrounds_ = false;
    std::vector<FieldAudioCommand> audio_;
    bool close_title_ = false, drawing_ready_ = false;
    Phase phase_ = Phase::frame_begin;
    std::optional<Phase> audio_resume_;
    FieldLoopBoundary stopped_ = FieldLoopBoundary::frame;
    std::uint64_t frames_ = 0;
    bool music_finished_ = false;
    std::optional<std::uint8_t> music_position_;
    bool event_running() const;
    FieldLoopResult advance_impl(Buttons first, Buttons second, std::size_t command_budget,
                                 bool audio_ready);
    bool run_events(std::size_t& budget);
    void run_actions(bool startup);
    void move_objects();
    void finish_startup();
    void retain_drawing();
    void publish_frame(bool advance_palette = true, std::uint8_t mosaic_size = 1);
    void compose_dialogue();
    void check_trigger();
    void close_map_title();
    bool check_party_request();
    bool menu_requested() const;
    std::optional<FieldLoopBoundary> check_handoffs();
};
} // namespace ff6
