#include "ff6/music_player.hpp"
#include "ff6/audio_performance.hpp"
#include "ff6/driver_timeline.hpp"
#include <deque>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace ff6 {
namespace {
PlaybackCommand grouped_command(PlaybackGroup group, bool stop) {
    switch (group) {
    case PlaybackGroup::music:
        return stop ? PlaybackCommand::stop_music : PlaybackCommand::music_volume;
    case PlaybackGroup::effects:
        return stop ? PlaybackCommand::stop_effects : PlaybackCommand::effect_volume;
    case PlaybackGroup::music_and_effects:
        return stop ? PlaybackCommand::stop : PlaybackCommand::volume;
    }
    throw std::invalid_argument("Invalid playback group");
}
struct PlaybackControl {
    PlaybackCommand command;
    std::uint8_t duration = 0, value = 0;
};
// Translate typed host/game requests to the existing semantic handler contract.
// The signed-to-unsigned conversions below are defined modulo 256; the recovered
// ramp implementation performs signed comparison and explicit fixed-point math.
std::optional<PlaybackControl> playback_control(const MusicPlayerCommand& command) {
    return std::visit(
        [](const auto& value) -> std::optional<PlaybackControl> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PlaybackVolumeRamp>)
                return PlaybackControl{grouped_command(value.group, false), value.duration,
                                       value.target};
            else if constexpr (std::is_same_v<T, EffectPanRamp>)
                return PlaybackControl{PlaybackCommand::effect_pan, value.duration, value.target};
            else if constexpr (std::is_same_v<T, MusicTempoRamp>)
                return PlaybackControl{PlaybackCommand::tempo, value.duration,
                                       static_cast<std::uint8_t>(value.target)};
            else if constexpr (std::is_same_v<T, MusicPitchRamp>)
                return PlaybackControl{PlaybackCommand::pitch, value.duration,
                                       static_cast<std::uint8_t>(value.target)};
            else if constexpr (std::is_same_v<T, MonoPlayback>)
                return PlaybackControl{PlaybackCommand::mono, 0,
                                       static_cast<std::uint8_t>(value.enabled)};
            else if constexpr (std::is_same_v<T, MutedPlaybackVoices>)
                return PlaybackControl{PlaybackCommand::mute, 0, value.mask};
            else if constexpr (std::is_same_v<T, MusicCue>)
                return PlaybackControl{PlaybackCommand::conditional};
            else if constexpr (std::is_same_v<T, StopPlayback>)
                return PlaybackControl{grouped_command(value.group, true)};
            else
                return std::nullopt;
        },
        command);
}
} // namespace

std::bitset<85> load_game_song_policy(const ContentPack& pack) {
    const auto data = pack.get("audio_auto_suspend_songs");
    if (data.size() != 5 || data.back() != 255)
        throw std::invalid_argument("Invalid game-song suspension table");
    std::bitset<85> result;
    for (const auto song : data.first(4)) {
        if (song >= result.size() || result[song])
            throw std::invalid_argument("Invalid automatic song suspension identity");
        result.set(song);
    }
    return result;
}

struct MusicPlayer::Impl {
    MusicScripts scripts;
    SampleBank samples;
    DriverSequencer driver;
    DriverTimeline timeline{0, 256};
    AudioPerformance performance;
    Synthesis synthesis;
    SoundTime output_at = 128;
    SoundTime command_ready_at = 0;
    std::bitset<85> song_policy;
    // CPU-side requested identity survives natural driver termination/fade-out;
    // only an explicit music stop invalidates it. It is not a voice-owner mask.
    std::optional<std::uint8_t> game_song;
    bool music_poll = false;
    struct ScheduledCommand {
        SoundTime at;
        MusicPlayerCommand command;
    };
    std::deque<ScheduledCommand> commands;

    Impl(MusicScripts script_data, SampleBank sample_data, EchoFilterPresets filters,
         NotePitchTable pitches, std::uint8_t song, std::bitset<85> policy)
        : scripts(std::move(script_data)), samples(std::move(sample_data)),
          driver(scripts, samples, filters, pitches), performance(samples),
          synthesis(std::vector<StereoFrame>(FF6_ROM_VALUE_AUDIO_INITIAL_ECHO_DELAY_0 * 512), {}),
          song_policy(policy), game_song(song) {
        load({song, 255, false, false}, 0);
    }
    void load(SongLoad command, SoundTime begun) {
        const auto request = driver.load(command);
        const auto start = timeline.begin_load(begun, request);
        performance.append(start.release);
        // Preview assets are already resident. This is an explicit native ready
        // event, not a reconstruction of the game's CPU transfer duration.
        const auto finish = timeline.finish_load(start.awaiting_content_at);
        performance.append(finish.output);
        music_poll = false;
        game_song = command.identity;
    }
    struct CommandResult {
        bool advanced = false;
        std::optional<DriverHandlerWork> handled;
    };
    void begin_service(const DriverIdlePoll& poll) {
        if (!poll.opportunity.due())
            return;
        const auto begin = driver.begin_service();
        performance.append(timeline.begin_service(poll.sampled_at, begin).publication);
        music_poll = begin.music_poll;
    }
    CommandResult inspect_command() {
        if (commands.empty() || commands.front().at > timeline.available_at())
            return {};
        CommandResult result;
        const auto& command = commands.front().command;
        std::optional<SongLoad> selected;
        if (const auto* game = std::get_if<GameSong>(&command)) {
            if (game_song == game->identity) {
                // PlaySong returns before changing volume or suspension state.
                commands.pop_front();
                return result;
            }
            selected = SongLoad{game->identity, game->volume, game->alternate,
                                game->suspend_current || song_policy[game->identity]};
        } else if (const auto* song = std::get_if<SongLoad>(&command))
            selected = *song;
        if (selected) {
            const auto dispatch =
                16U + driver_dispatch_work(DriverRequestFamily::allocation).duration();
            if (timeline.available_at() > std::numeric_limits<SoundTime>::max() - dispatch)
                throw std::overflow_error("Music command timeline exhausted");
            // Retain the sequencer, publication and synthesis owners. The load
            // handler supplies its documented partial reset or song restoration.
            load(*selected, timeline.available_at() + dispatch);
            result.advanced = true;
        } else if (const auto* effect = std::get_if<GameSoundEffect>(&command)) {
            result.handled = {DriverRequestFamily::allocation,
                              driver.effect(effect->identity, effect->pan), false};
        } else if (const auto* system = std::get_if<SystemSoundEffect>(&command)) {
            result.handled = {DriverRequestFamily::system_effect,
                              driver.system_effect(system->identity), false};
        } else if (const auto* pause_command = std::get_if<PauseMusic>(&command)) {
            const auto request =
                driver.command(PlaybackCommand::pause, 0, pause_command->paused ? 1 : 0);
            const auto pause = timeline.publish_pause(request);
            performance.append(pause.publication);
            music_poll = false;
            if (pause.idle)
                begin_service(*pause.idle);
            result.advanced = true;
        } else {
            const auto control = playback_control(command);
            if (!control)
                throw std::logic_error("Unhandled native music command");
            const auto requested =
                driver.command(control->command, control->duration, control->value);
            // These commands change retained state/dirty masks. Voices and
            // releases publish at their normal service boundaries; inventing an
            // immediate gain/reset here would change fades and effect ownership.
            const auto family = control->command <= PlaybackCommand::pitch ||
                                        control->command == PlaybackCommand::conditional
                                    ? DriverRequestFamily::mix_controls
                                    : DriverRequestFamily::playback_controls;
            result.handled = {family, requested.work, false};
            if (const auto* stop = std::get_if<StopPlayback>(&command);
                stop && stop->group != PlaybackGroup::effects)
                game_song.reset();
        }
        commands.pop_front();
        return result;
    }
    StereoFrame render() {
        if (output_at > std::numeric_limits<SoundTime>::max() - 64)
            throw std::overflow_error("Music timeline exhausted");
        // Resolve source selection through the next priming boundary. The native
        // producer advances independently of SDL's buffer size or device clock.
        while (timeline.available_at() <= output_at + 38) {
            // Stop between music and effects so a command can be assigned to
            // that inspection without advancing an entire service prematurely.
            const auto command =
                !driver.service_pending() || music_poll ? inspect_command() : CommandResult{};
            if (command.advanced) {
                command_ready_at = timeline.available_at();
                continue;
            }
            if (driver.service_pending()) {
                performance.append(
                    timeline.finish_service(driver.finish_service(), command.handled).publication);
                music_poll = false;
                if (command.handled)
                    command_ready_at = timeline.available_at();
                continue;
            }
            const auto poll = timeline.poll_idle(command.handled);
            if (command.handled)
                command_ready_at = timeline.available_at();
            begin_service(poll);
        }
        const auto input =
            performance.controls(output_at, (synthesis.next_sample_index() & 1U) == 0);
        const auto result = synthesis.render(input).echo.output;
        output_at += 64;
        return result;
    }
};
MusicPlayer::MusicPlayer(const ContentPack& pack, std::uint8_t song)
    : MusicPlayer(load_music_scripts(pack), load_sample_bank(pack), load_echo_filters(pack),
                  load_note_pitches(pack), song, load_game_song_policy(pack)) {}
MusicPlayer::MusicPlayer(MusicScripts scripts, SampleBank samples, EchoFilterPresets filters,
                         NotePitchTable pitches, std::uint8_t song, std::bitset<85> song_policy)
    : impl_(std::make_unique<Impl>(std::move(scripts), std::move(samples), filters, pitches, song,
                                   song_policy)) {}
MusicPlayer::~MusicPlayer() = default;
void MusicPlayer::render(std::span<StereoFrame> frames) {
    for (auto& frame : frames)
        frame = impl_->render();
}
void MusicPlayer::schedule(std::uint64_t frame, MusicPlayerCommand command) {
    if (frame < rendered_frames() || frame > (std::numeric_limits<SoundTime>::max() - 192) / 64)
        throw std::invalid_argument("Music command frame outside renderable future");
    const auto at = 128 + frame * 64;
    if (!impl_->commands.empty() && at < impl_->commands.back().at)
        throw std::invalid_argument("Music commands must be scheduled in frame order");
    if (impl_->commands.size() == music_command_capacity)
        throw std::length_error("Music command queue is full");
    if (const auto* song = std::get_if<SongLoad>(&command))
        (void)impl_->scripts.song(song->identity);
    if (const auto* song = std::get_if<GameSong>(&command))
        (void)impl_->scripts.song(song->identity);
    if (const auto* effect = std::get_if<SystemSoundEffect>(&command))
        (void)impl_->scripts.system_effect(effect->identity);
    (void)playback_control(command); // Reject invalid groups before changing the queue.
    impl_->commands.push_back({at, command});
}
std::size_t MusicPlayer::pending_commands() const {
    return impl_->commands.size();
}
bool MusicPlayer::commands_ready() const {
    return impl_->commands.empty() && impl_->command_ready_at <= impl_->output_at;
}
std::optional<std::uint8_t> MusicPlayer::playing_song() const {
    return impl_->driver.state().playback.ownership.song;
}
std::uint8_t MusicPlayer::music_position() const {
    return impl_->driver.state().global.output_code;
}
bool MusicPlayer::music_finished() const {
    const auto& playback = impl_->driver.state().playback;
    // Event FA observes the driver's selected completion condition: after a
    // music fade to zero, Main reports the remaining fade duration, not live
    // sequencer voices. A silent looping song need not terminate to release it.
    return commands_ready() && (playback.controls.fade_status
                                    ? playback.controls.music_volume.remaining == 0
                                    : playback.ownership.music == 0);
}
std::optional<std::uint8_t> MusicPlayer::suspended_song() const {
    return impl_->driver.state().playback.ownership.suspended_song;
}
bool MusicPlayer::paused() const {
    return impl_->driver.state().playback.ownership.paused();
}
PlaybackControls MusicPlayer::playback_controls() const {
    return impl_->driver.state().playback.controls;
}
std::uint64_t MusicPlayer::rendered_frames() const {
    return impl_->synthesis.next_sample_index();
}
} // namespace ff6
