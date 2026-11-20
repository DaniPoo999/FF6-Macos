#pragma once
#include "rom-materials/audio_cadence_choices.macros.inc"
#include "ff6/playback_controls.hpp"

namespace ff6 {
// Presence is independent of whether a channel is enabled. Old cursors and
// loops survive replacement until the particular initializer overwrites them.
struct DriverScriptCursor {
    std::optional<ScriptPosition> position;
    std::array<ScriptLoop, 4> loops{};
    std::uint8_t loop_slot = 0, wait = 0;
    bool operator==(const DriverScriptCursor&) const = default;
};
struct DriverChannels {
    std::array<DriverChannelControls, 8> controls{};
    std::array<DriverNoteState, 8> notes{};
    std::array<DriverScriptCursor, 8> scripts{};
    bool operator==(const DriverChannels&) const = default;
};
struct DriverState {
    PlaybackState playback;
    DriverGlobalControls global;
    DriverNoteQueues queues;
    DriverChannels music, effects;
    // Stopping playback invalidates its current-song identity but does not
    // unload script/sample resources needed by retained cursors and effects.
    std::optional<std::uint8_t> loaded_song;
    std::uint8_t music_phase = 0, effects_phase = 0,
                 control_countdown = FF6_ROM_VALUE_AUDIO_CADENCE_CHOICES_0;
    bool forced_progression = false;
    bool operator==(const DriverState&) const = default;
};
// No external mix policy, selected routing, effect ownership or service countdown
// is suspended. Dirty fields in global are likewise not part of the snapshot.
struct SuspendedDriverSong {
    std::uint8_t identity = 0, enabled_music = 0;
    DriverChannels music, effects;
    DriverGlobalControls global;
    DriverNoteQueues queues;
    std::uint8_t releases = 0, music_phase = 0, effects_phase = 0;
    bool operator==(const SuspendedDriverSong&) const = default;
};
struct DriverResumeVoice {
    DriverSampleRequest sample;
    std::uint16_t pitch = 0;
    std::uint16_t pitch_work = 0;
    bool operator==(const DriverResumeVoice&) const = default;
};
struct DriverLoadWork {
    // Offsets from handler entry to immediate release and the content barrier.
    // The barrier's duration belongs to the external producer, not this driver.
    std::uint32_t release_offset = 0, content_wait_offset = 0;
    // From content-ready continuation to the first resumed sample operation,
    // or (without resumed voices) the common final routing/clock cleanup.
    // Excludes voice publication and final cleanup; see LOAD_WORK.md.
    std::uint32_t preparation = 0;
    bool operator==(const DriverLoadWork&) const = default;
};
struct DriverLoadRequests {
    bool release_all = false, abandon_service = false;
    // All eight voices, in ascending order: sample/envelope, zero stereo gain,
    // then pitch. These are driver-time intentions, not effective sample events.
    std::optional<std::array<DriverResumeVoice, 8>> resume;
    DriverLoadWork work;
    bool operator==(const DriverLoadRequests&) const = default;
};
struct SongLoad {
    std::uint8_t identity = 0, volume = 0;
    bool alternate = false, suspend_current = false;
};
// Song entries are decoded content; a malformed request leaves both state and
// snapshot unchanged. No ROM addresses or upload protocol are accepted.
DriverLoadRequests load_driver_song(const SongScript& song, SongLoad request, DriverState& state,
                                    std::optional<SuspendedDriverSong>& suspended);
// Return complete handler work through the caller return, in 1/64-sample ticks.
// No output is published. Suppressed/null requests still consume their original
// decision work; system effects scan ownership even when the entry is absent.
std::uint16_t start_driver_effect(std::optional<ScriptPosition> first,
                                  std::optional<ScriptPosition> second, std::uint8_t pan,
                                  const DriverSampleDefaults& defaults, DriverState& state);
std::uint16_t start_driver_system_effect(std::optional<ScriptPosition> entry, DriverState& state);
} // namespace ff6
