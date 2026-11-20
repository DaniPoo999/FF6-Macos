#pragma once
#include "ff6/content.hpp"
#include "ff6/playback_pan.hpp"
#include "ff6/playback_volume.hpp"
#include <array>
#include <optional>
#include <variant>

namespace ff6 {
// Data-language identities, not addresses into an emulated memory image. The
// owner resolves aliases (including recruited flags) to authoritative state.
// Valid identities are 0..0x6ff; access must be consistent and nonthrowing there.
class EventFlagAccess {
  public:
    virtual ~EventFlagAccess() = default;
    virtual bool test(std::uint16_t identity) const = 0;
    virtual bool test_battle(std::uint8_t) const {
        throw std::invalid_argument("Battle condition owner is unavailable");
    }
    virtual void set_battle(std::uint8_t, bool) {
        throw std::invalid_argument("Battle condition owner is unavailable");
    }
    virtual void set(std::uint16_t identity, bool value) = 0;
};
struct EventInputs {
    std::uint8_t facing = 0;
    bool confirm_down = false;
    bool objects_sorted = true, backgrounds_ready = true, map_load_pending = false;
    bool dialogue_open = false, dialogue_text_ready = false, fade_active = false;
    bool object_script_active = true;
    bool scroll_arrived = false;
    bool music_finished = false;
    std::optional<std::uint8_t> music_position;
};
// Every character/object operand below is an encoded object reference, including
// party-slot aliases. The owner resolves it before indexing characters/objects.
struct EventCharacterName {
    std::uint8_t character, name;
};
struct EventCharacterProperties {
    std::uint8_t character, properties;
};
struct EventNormalizeLevel { std::uint8_t character; };
// Direct roster identity, as passed to the menu's Optimum entry point.
struct EventOptimizeEquipment { std::uint8_t character; };
struct EventRemoveEquipment {
    // Direct roster identity; this command does not resolve party-slot aliases.
    std::uint8_t character;
};
struct EventCreateObject {
    std::uint8_t object;
};
struct EventHideObject {
    std::uint8_t object;
};
struct EventShowObject {
    std::uint8_t object;
};
struct EventDeleteObject {
    std::uint8_t object;
};
struct EventCharacterParty {
    std::uint8_t character, party;
};
struct EventObjectGraphics {
    std::uint8_t object, graphics;
};
struct EventObjectPalette {
    // Retain the complete operand. Its high bits can affect sprite attributes;
    // the field owner must reconstruct that rule, not mask this to three bits.
    std::uint8_t object, palette;
};
struct EventObjectCollision {
    std::uint8_t object;
    bool ignore_occupants;
};
struct EventObjectVehicle {
    std::uint8_t object, vehicle;
    bool show_rider;
};
struct EventPartyObjects {
    std::array<std::optional<std::uint8_t>, 4> objects;
};
struct EventActivateParty {
    std::uint8_t party;
};
struct EventPartyMap { std::uint8_t party; std::uint16_t map; };
struct EventObjectEvent { std::uint8_t object; std::uint32_t entry; };
struct EventContactActivation { std::uint8_t object; bool enabled; };
struct EventDefaultParty { bool restore; };
struct EventPartyPosition { std::uint8_t x, y; };
enum class EventStatusChange { remove, add, toggle };
struct EventCharacterStatus {
    std::uint8_t object;
    EventStatusChange change;
    std::uint16_t mask;
};
struct EventCharacterVital {
    std::uint8_t object, amount;
    bool mp;
};
struct EventInventoryItem {
    std::uint8_t item;
    bool give;
};
struct EventInventoryEsper {
    std::uint8_t esper;
    bool give;
};
struct EventLearnTechniques {
    std::uint8_t bushido = 0, blitz = 0;
};
struct EventInventoryGil {
    std::uint16_t amount;
    bool give;
};
struct EventParentMap {
    std::uint16_t map;
    std::uint8_t x, y, facing;
};
struct EventMapLoad {
    std::uint16_t destination = 0; // Nine-bit identity; 510 previous, 511 parent.
    std::uint8_t x = 0, y = 0, facing = 0, world_vehicle = 0;
    bool set_parent = false, lower_level = false, show_name = false;
    bool run_startup = false, fade_in = true, keep_dimensions = false, fade_out = true;
    bool operator==(const EventMapLoad&) const = default;
};
struct EventMapEdit {
    std::uint8_t layer, x, y, width, height;
    Bytes tiles;
    bool immediate;
};
struct EventApplyMapEdits {};
struct EventActivePartyMembers {};
struct EventActivePartyNumber {};
struct EventPartyLeader {};
enum class CharacterMaskSource { initialized, available, enabled_objects, all_parties };
struct EventCharacterMask { CharacterMaskSource source; };
struct EventRandomBranch {}; // One draw from the shared field RNG, supplied by its owner.
enum class EventVariableChange : std::uint8_t { assign, add, subtract, compare };
struct EventVariable {
    EventVariableChange change;
    std::uint8_t identity;
    std::uint16_t value;
};
struct EventGameSoundEffect {
    std::uint8_t identity;
};
struct EventPlaySong {
    std::uint8_t identity, volume;
    bool suspend_current;
    std::optional<std::uint8_t> fade_in;
    bool update_map_song = true;
    bool alternate = false;
};
struct EventFadeSong {
    std::uint8_t duration;
};
struct EventObjectScript {
    std::uint8_t object;
    std::uint32_t entry;
    bool wait;
};
struct EventWaitObject {
    // Unlike object writes, a slot reference reads the current update order
    // without testing membership in the active party.
    std::uint8_t object;
};
struct EventDialogue {
    std::uint16_t message;
    bool bottom, text_only;
};
struct EventDialogueSelection {};
enum class EventCutscene { title, opening_credits, mine_cart, floating_continent, world_ruin, ending, ending_airship, the_end };
struct EventRunCutscene {
    EventCutscene scene;
    std::uint8_t segment = 0; // BA selects an authored ending cinematic family.
};
struct EventLoadMenu {};
struct EventRestartGame {};
struct EventClearOverlay {};
struct EventNameMenu { std::uint8_t character; };
struct EventFinalOrder {};
struct EventPartyMenu {
    std::uint8_t parties;
    bool clear;
    std::uint16_t forced;
};
struct EventScreenMosaic { std::uint8_t speed; };
enum class EventBattleKind { scripted, contact, treasure, random, colosseum };
struct EventStartBattle {
    EventBattleKind kind = EventBattleKind::scripted;
    // Treasure battles resolve their group from the retained chest owner.
    std::uint8_t group = 0, background = 63;
    bool mosaic = true, sound = true;
    bool operator==(const EventStartBattle&) const = default;
};
enum class EventLoadResult { new_game, saved_game };
struct EventRestorePartyMovement {};
struct EventPartyControl {
    bool player;
};
struct EventUpdateParty {};
struct EventSortObjects {};
struct EventCameraLock {
    bool locked;
};
struct EventScreenFade {
    std::uint8_t rate;
    bool fade_out;
};
struct EventStopScreenFade {};
struct EventScreenShake { std::uint8_t control; };
struct EventStopScroll {};
struct EventBackgroundScroll {
    std::uint8_t layer;
    std::int16_t x, y; // Independent signed 8.8 rates, retained until replaced.
};
struct EventLoadPalette {
    std::uint8_t destination, source;
};
struct EventColosseumMenu {};
struct EventShop {
    std::uint8_t identity;
};
// Decoder supplies the range explicitly, including the all-background command.
struct EventBackgroundPalette { std::uint8_t control, first, last; };
struct EventSpritePalette { std::uint8_t control, first = 0, last = 127; };
struct EventPaletteFilter { std::uint8_t channels, first, last; };
struct EventRestoreColorMath {};
struct EventFixedColor { std::uint8_t control; bool subtract; };
struct EventBackgroundRate { std::uint8_t group, rate; };
struct EventFlashlight { std::uint8_t radius; };
struct EventPyramid { std::optional<std::uint8_t> object; };
struct EventPortrait { std::uint8_t identity; };
struct EventScreenFlash {
    std::uint8_t channels;
};
struct EventResumeSong {
    std::uint8_t fade;
};
struct EventContinueSong {};
struct EventStopSound { PlaybackGroup group; };
struct EventMusicAdjustment {
    bool pitch = false;
    std::uint8_t duration = 0;
    std::int8_t target = 0;
};
struct EventSetTimer {
    std::uint8_t identity = 0;
    std::uint16_t frames = 0;
    std::uint32_t event = 0;
    bool pause_in_menu_battle = false;
    bool visible_field = false, visible_menu_battle = false, terminate_external = false;
};
struct EventResetTimer {
    std::uint8_t identity = 0;
};
using EventOperation = std::variant<
    EventCharacterName, EventCharacterProperties, EventCreateObject, EventHideObject,
    EventCharacterParty, EventObjectGraphics, EventObjectPalette, EventPartyObjects,
    EventActivateParty, EventCharacterStatus, EventCharacterVital, EventParentMap, EventDialogue,
    EventRunCutscene, EventRestartGame, EventClearOverlay, EventRestorePartyMovement,
    EventPartyControl, EventUpdateParty, EventMapLoad, EventObjectScript, EventInventoryItem,
    EventInventoryGil, EventInventoryEsper, EventLearnTechniques, EventStopScroll, EventBackgroundScroll, EventMapEdit, EventApplyMapEdits,
    EventActivePartyMembers, EventPartyLeader, EventCharacterMask, EventPartyMenu, EventFinalOrder,
    EventDialogueSelection, EventGameSoundEffect, EventShowObject,
    EventDeleteObject, EventPlaySong, EventFadeSong, PlaybackVolumeRamp, EventCameraLock,
    EventSortObjects, EventScreenFade, EventStopScreenFade, EventWaitObject, EventObjectCollision,
    EventObjectVehicle, EventLoadMenu, EventStartBattle, EventScreenShake, EventLoadPalette,
    EventColosseumMenu, EventShop, EventBackgroundPalette, EventRestoreColorMath, EventNameMenu, EventScreenMosaic,
    EventScreenFlash, EventResumeSong, EventPaletteFilter, EventPartyMap, EventObjectEvent,
    EventContactActivation, EventDefaultParty, EventPartyPosition, EventSpritePalette,
    EffectPanRamp, EventRemoveEquipment, EventRandomBranch, EventVariable, EventNormalizeLevel,
    EventFlashlight, EventBackgroundRate, EventContinueSong, EventStopSound, EventSetTimer,
    EventResetTimer, EventMusicAdjustment, EventFixedColor, EventOptimizeEquipment,
    EventActivePartyNumber, EventPyramid, EventPortrait>;
struct EventRequest {
    std::uint64_t identity;
    // Native startup continuations have no offset in the extracted script.
    std::optional<std::uint32_t> script_offset;
    EventOperation operation;
};
enum class EventYield { operation, frame_wait, budget, idle, finished };
enum class EventContinuation {
    script,
    startup_call,
    startup_release,
    startup_yield,
    startup_return
};

ByteView field_event_script(const ContentPack& pack);
class FieldEventRunner {
  public:
    // Owns a private immutable copy; only this resource's event-language offsets
    // are accepted. Starting an event is one root invocation, not all new-game setup.
    FieldEventRunner(ByteView script, std::uint32_t entry);
    // No active invocation: returning from a startup event restores movement
    // before parking here again. The selected byte must be the idle command.
    static FieldEventRunner parked(ByteView script, std::uint32_t entry);
    // Object-action F9 starts a root event from idle, retaining existing waits
    // and returning to the same idle cursor. It cannot interrupt a live event.
    void begin_object_event(std::uint32_t entry);
    // Contact replaces the root invocation, even if a prior script is active.
    // Retains global pause/object/fade waits, replaces the old dialogue-call
    // continuation, and adds a camera-arrival wait. Returns to the supplied idle
    // entry. In-flight native requests must finish.
    void begin_contact_event(std::uint32_t entry, std::uint32_t idle_entry);
    // A world/vehicle script resumes the suspended field invocation after its
    // load-map command. Preserve its call stack and root return identity.
    void resume_from_world(std::uint32_t entry);
    bool waiting_for_scroll() const {
        return scroll_wait_;
    }
    // Preserves the interrupted stack, pause and waits. An immediate return
    // entry is empty and makes no changes. Reserves wrapper + callee capacity.
    bool begin_map_startup(std::uint32_t entry, EventFlagAccess& flags);
    EventContinuation continuation() const {
        return continuation_;
    }
    // LoadMap performs an extra event visit when a return is about to lead to
    // its idle script. This is a predicate, not automatic script execution.
    bool returning_to(std::uint32_t script_offset) const;
    // A budget boundary is more work in the same logical visit, not elapsed time.
    // Completed operations resume continuation; their owner controls their timing.
    EventYield advance(EventFlagAccess& flags, const EventInputs& input,
                       std::size_t command_budget = 1024);
    const std::optional<EventRequest>& pending_request() const {
        return request_;
    }
    // Apply the complete native operation first. Cutscenes require an explicit
    // skipped/completed result; object-script assignment and explicit object
    // waiting require the resolved wait identity (the displayed actor for an
    // absent party-slot script); B6 requires the retained dialogue choice.
    // Load-menu completion requires its New Game/saved-game result. Other
    // requests reject these extra results.
    void complete_request(std::uint64_t identity, EventFlagAccess& flags,
                          std::optional<bool> cutscene_skipped = std::nullopt,
                          std::optional<std::uint8_t> script_object = std::nullopt,
                          std::optional<std::uint8_t> dialogue_selection = std::nullopt,
                          std::optional<EventLoadResult> load_result = std::nullopt,
                          std::optional<std::uint8_t> random_draw = std::nullopt);
    std::optional<std::uint8_t> waiting_object() const {
        return waiting_object_;
    }
    std::uint32_t position() const {
        if (continuation_ != EventContinuation::script)
            throw std::logic_error("Native event continuation has no script position");
        return position_;
    }
    std::uint16_t pause_remaining() const {
        return pause_;
    }
    std::size_t depth() const {
        return depth_;
    }
    bool finished() const {
        return finished_;
    }

  private:
    enum class FrameKind { call, loop, map_startup };
    struct Frame {
        FrameKind kind = FrameKind::call;
        std::uint32_t start = 0, resume = 0;
        std::uint8_t remaining = 1;
        EventContinuation resume_continuation = EventContinuation::script;
    };
    enum class Wait { none, dialogue_close, fade, object };
    enum class Completion {
        continue_script,
        dialogue_close,
        finish,
        map_load,
        map_load_fade,
        startup_return,
        stop_scroll,
        end_visit
    };
    Bytes script_;
    std::uint32_t position_ = 0;
    std::optional<std::uint32_t> root_idle_;
    std::uint16_t pause_ = 0;
    std::array<Frame, 16> stack_{};
    std::size_t depth_ = 1;
    Wait wait_ = Wait::none;
    bool scroll_wait_ = false;
    std::optional<std::uint8_t> waiting_object_;
    bool finished_ = false, return_pending_ = false;
    std::uint64_t next_request_ = 1;
    std::optional<EventRequest> request_;
    std::uint32_t resume_ = 0;
    Completion completion_ = Completion::continue_script;
    EventContinuation continuation_ = EventContinuation::script;
    void validate_target(std::uint32_t target) const;
    std::uint32_t following(std::size_t length) const;
    EventYield request(EventOperation operation, std::size_t length,
                       Completion completion = Completion::continue_script);
};
} // namespace ff6
