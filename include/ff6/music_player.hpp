#pragma once
#include "ff6/content.hpp"
#include "ff6/driver_sequence.hpp"
#include "ff6/mixer.hpp"
#include "ff6/playback_pan.hpp"
#include "ff6/playback_volume.hpp"
#include <bitset>
#include <memory>

namespace ff6 {
inline constexpr unsigned music_sample_rate = 32040;
// Game-side song selection has duplicate suppression and data-driven suspension
// before it reaches the native driver, including alternate song openings.
struct GameSong {
    std::uint8_t identity = 0, volume = 255;
    bool suspend_current = false;
    bool alternate = false;
};
std::bitset<85> load_game_song_policy(const ContentPack& pack);
struct GameSoundEffect {
    std::uint8_t identity = 0, pan = 128;
};
struct SystemSoundEffect {
    std::uint8_t identity = 0;
};
struct PauseMusic {
    bool paused = true;
};
struct MusicTempoRamp {
    std::uint8_t duration = 0;
    std::int8_t target = 0; // Original signed adjustment; zero selects normal tempo.
};
struct MusicPitchRamp {
    std::uint8_t duration = 0;
    std::int8_t target = 0; // Original signed adjustment; zero selects normal pitch.
};
struct MonoPlayback {
    bool enabled = true;
};
struct MutedPlaybackVoices {
    // Shared output voices, regardless of current music/effect/system ownership.
    std::uint8_t mask = 0;
};
// Enables the next conditional music-script branch on each active music voice.
struct MusicCue {};
struct StopPlayback {
    PlaybackGroup group = PlaybackGroup::music_and_effects;
};
using MusicPlayerCommand =
    std::variant<SongLoad, GameSoundEffect, SystemSoundEffect, PauseMusic, PlaybackVolumeRamp,
                 EffectPanRamp, MusicTempoRamp, MusicPitchRamp, MonoPlayback, MutedPlaybackVoices,
                 MusicCue, StopPlayback, GameSong>;
inline constexpr std::size_t music_command_capacity = 256;
// Development playback of extracted music/effects with fixed delay and explicit
// quiescent startup. Owns native sequencing, publication and synthesis; there
// are no recorded controls or original executable inputs. Does not yet model
// original game-side arrivals or device recovery. See MUSIC_PLAYER.md.
class MusicPlayer {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    MusicPlayer(const ContentPack& pack, std::uint8_t song);
    MusicPlayer(MusicScripts scripts, SampleBank samples, EchoFilterPresets filters,
                NotePitchTable pitches, std::uint8_t song, std::bitset<85> song_policy = {});
    ~MusicPlayer();
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;
    void render(std::span<StereoFrame> frames);
    // Single-producer API, on the same thread as render. Schedule before the
    // relevant audio is generated; absolute canonical frame times are independent
    // of device buffers. Commands must be ordered; ties retain insertion order.
    // Consumed at the next available idle/post-music inspection, never midway
    // through an already-prepared native service. Throws before queue mutation
    // for invalid identity/time/order or a full bounded queue.
    void schedule(std::uint64_t frame, MusicPlayerCommand command);
    std::size_t pending_commands() const;
    // Every queued command has reached its native handler completion in rendered
    // canonical time. Does not wait for songs/fades to end or for device playback.
    bool commands_ready() const;
    // Event song-wait completion: fade-to-zero duration when selected by a
    // volume command, otherwise active music voices. Independent of device
    // buffering; a completed fade can leave the silent sequencer running.
    bool music_finished() const;
    // Authored music F9/FA cue counter, used by field scenes to synchronize
    // performance and dialogue. This is sequence state, not a device port.
    std::uint8_t music_position() const;
    std::optional<std::uint8_t> playing_song() const;
    std::optional<std::uint8_t> suspended_song() const;
    // The recovered ownership gate. A pause with no active music/ordinary
    // effects still silences voices, but does not create a paused owner mask.
    bool paused() const;
    // Producer state for inspection; already queued device audio can lag this.
    PlaybackControls playback_controls() const;
    std::uint64_t rendered_frames() const;
};
} // namespace ff6
