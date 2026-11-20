#pragma once
#include "ff6/field_boot.hpp"
#include "ff6/field_session.hpp"
#include "ff6/failure_log.hpp"
#include <SDL3/SDL.h>
#include <iostream>
#include <memory>

// SDL transport for the native session. Kept separate from the older isolated
// layer/actor study; that study is a useful diagnostic, not the gameplay owner.
class FieldSessionHost {
  public:
    ff6::FieldSession session;
    ff6::MusicPlayer music;
    ff6::FieldPacer clock;
    ff6::Buttons taps = 0, second_taps = 0;
    bool paused = false, muted = false, silent_test = false;
    bool initial_start = false;
    std::uint64_t audio_frames = 0, commands = 0;
    std::uint64_t max_queued_bytes = 0;
    FieldSessionHost(const ff6::ContentPack& pack, std::optional<std::uint16_t> map)
        : session(map ? ff6::make_development_field_session(pack, *map)
                      : ff6::make_initial_field_session(pack)),
          music(pack, 0), initial_start(!map) {
        if (!SDL_WasInit(SDL_INIT_AUDIO))
            check(SDL_InitSubSystem(SDL_INIT_AUDIO));
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, static_cast<int>(ff6::music_sample_rate)};
        audio_.reset(
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr));
        check(audio_ != nullptr);
        const auto* driver = SDL_GetCurrentAudioDriver();
        const auto* device = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(audio_.get()));
        silent_test = driver && std::string_view(driver) == "dummy";
        std::cout << "Field output: " << (driver ? driver : "unknown driver") << " / "
                  << (device ? device : "default device") << std::endl;
    }
    bool halted() const {
        return session.stopped() || !transport_error_.empty();
    }
    const std::string& notice() const {
        return transport_error_.empty() ? session.notice() : transport_error_;
    }
    void set_save_folder(const std::filesystem::path& folder) {
        session.set_save_folder(folder);
        failure_log_.emplace(folder.parent_path() / "Diagnostics");
    }
    bool logging_enabled() const { return failure_log_ && failure_log_->enabled(); }
    std::string logging_notice() const {
        return SDL_GetTicks() < logging_notice_until_ ? logging_notice_ : "";
    }
    void toggle_logging() {
        if (!failure_log_) return;
        try {
            failure_log_->set_enabled(!failure_log_->enabled());
            logging_notice_ = logging_enabled() ? "Failure logging: ON (F10 to disable)"
                                               : "Failure logging: OFF (F10 to enable)";
            failure_report_attempted_ = false;
            report_failure(); // Enabling after a stop captures the current failure too.
        } catch (const std::exception& error) {
            logging_notice_ = std::string("Could not change failure logging: ") + error.what();
        }
        logging_notice_until_ = SDL_GetTicks() + 5000;
    }
    bool tick(const ff6::ContentPack& pack, ff6::Buttons buttons, ff6::Buttons second = 0) {
        if (halted())
            return false;
        try {
            const auto result = session.advance(pack, buttons | taps, 1024, music.commands_ready(),
                                                second | second_taps, music.music_finished(),
                                                music.music_position());
            if (session.stopped()) {
                report_failure();
                activate(false);
                return false;
            }
            if (result.input_sampled)
                taps = second_taps = 0;
            if (!result.complete) {
                report_failure();
                return false;
            }
            for (const auto& command : result.audio) {
                music.schedule(music.rendered_frames(), command);
                ++commands;
            }
            std::array<ff6::StereoFrame, ff6::field_session_samples> block{};
            music.render(block);
            // Device buffering never determines simulation time or command order.
            // Refuse an unhealthy device backlog instead of growing without bound.
            const auto queued = SDL_GetAudioStreamQueued(audio_.get());
            check(queued >= 0);
            constexpr int limit =
                static_cast<int>(ff6::field_session_samples * sizeof(ff6::StereoFrame) * 16);
            if (queued > limit - static_cast<int>(sizeof(block)))
                throw std::runtime_error(
                    "Field audio device backlog exceeded its bound: ticks=" +
                    std::to_string(session.ticks()) + ", queued=" + std::to_string(queued) +
                    ", paused=" +
                    std::to_string(SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(audio_.get()))) +
                    ", host_playing=" + std::to_string(playing_));
            check(SDL_PutAudioStreamData(audio_.get(), block.data(),
                                         static_cast<int>(sizeof(block))));
            audio_frames += block.size();
            max_queued_bytes =
                std::max(max_queued_bytes, static_cast<std::uint64_t>(queued) + sizeof(block));
            return true;
        } catch (const std::exception& error) {
            transport_error_ = error.what();
            report_failure();
            activate(false);
            return false;
        }
    }
    void activate(bool active) {
        // Two ticks of prebuffer on first launch. Focus/page/host pause freezes
        // the device and simulation together without altering native pause state.
        const bool play =
            active && !paused && !halted() && audio_frames >= 2 * ff6::field_session_samples;
        if (play == playing_)
            return;
        check(play ? SDL_ResumeAudioStreamDevice(audio_.get())
                   : SDL_PauseAudioStreamDevice(audio_.get()));
        playing_ = play;
    }
    void toggle_mute() {
        muted = !muted;
        check(SDL_SetAudioStreamGain(audio_.get(), muted ? 0.0f : 1.0f));
    }

  private:
    std::unique_ptr<SDL_AudioStream, decltype(&SDL_DestroyAudioStream)> audio_{
        nullptr, SDL_DestroyAudioStream};
    bool playing_ = false;
    std::string transport_error_;
    std::optional<ff6::FailureLog> failure_log_;
    bool failure_report_attempted_ = false;
    std::string logging_notice_;
    std::uint64_t logging_notice_until_ = 0;
    void report_failure() {
        if (!halted() || !logging_enabled() || failure_report_attempted_) return;
        failure_report_attempted_ = true;
        try {
            auto report = ff6::describe_session_failure(session, notice());
            report += "audio_frames=" + std::to_string(audio_frames) +
                      " max_queued_bytes=" + std::to_string(max_queued_bytes) +
                      " paused=" + std::to_string(paused) + '\n';
            failure_log_->record(report);
        } catch (const std::exception& error) {
            // Preserve the actual stop reason even if diagnostics are unwritable.
            logging_notice_ = std::string("Could not write failure log: ") + error.what();
            logging_notice_until_ = SDL_GetTicks() + 5000;
        }
    }
    static void check(bool value) {
        if (!value)
            throw std::runtime_error(SDL_GetError());
    }
};
