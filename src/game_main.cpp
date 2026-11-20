#include "ff6/content.hpp"
#include "ff6/io.hpp"
#include "ff6/video.hpp"
#include "field_session_host.hpp"
#include "sdl_input.hpp"
#include "game_presenter.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace {
void check(bool ok) {
    if (!ok)
        throw std::runtime_error(SDL_GetError());
}
}

// A gameplay-only host for the same native session as the inspector's F7 mode.
// No content studies, development map entries or page-switching controls live here.
int main(int argc, char** argv) {
    bool smoke = false;
    try {
        std::filesystem::path save_folder;
        std::filesystem::path preferences_folder;
        std::optional<presentation::Mode> crt_mode;
        bool cycle_crt = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option(argv[i]);
            if (option == "--smoke-test")
                smoke = true;
            else if (option == "--save-folder" && i + 1 < argc)
                save_folder = argv[++i];
            else if (option == "--preferences-folder" && i + 1 < argc)
                preferences_folder = argv[++i];
            else if (option == "--crt" && i + 1 < argc)
                crt_mode = presentation::parse(argv[++i]);
            else if (option == "--smoke-cycle-crt")
                cycle_crt = true;
            else
                throw std::invalid_argument("Usage: ff6-game [--smoke-test] [--save-folder PATH] "
                    "[--preferences-folder PATH] [--crt off|sdr|hdr] [--smoke-cycle-crt]");
        }
        if (smoke && save_folder.empty())
            throw std::invalid_argument("Smoke tests require an isolated --save-folder");
        if (cycle_crt && !smoke)
            throw std::invalid_argument("--smoke-cycle-crt requires --smoke-test");
        check(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
        struct Quit { ~Quit() { SDL_Quit(); } } quit;
        const auto* base = SDL_GetBasePath();
        check(base != nullptr);
        const auto pack = ff6::decode_pack(ff6::read_file(
            std::filesystem::path(base) / "content.ff6pack"));
        ff6::verify_canonical(pack);
        if (!save_folder.empty() && preferences_folder.empty())
            preferences_folder = save_folder.string() + ".preferences";
        if (save_folder.empty() || preferences_folder.empty()) {
            char* path = SDL_GetPrefPath("FFVI Native", "Final Fantasy VI");
            check(path != nullptr);
            if (save_folder.empty()) save_folder = std::filesystem::path(path) / "Saves";
            if (preferences_folder.empty()) preferences_folder = path;
            SDL_free(path);
        }
        SDL_Window* raw_window = SDL_CreateWindow("Final Fantasy VI",768,672,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        check(raw_window != nullptr);
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(raw_window, SDL_DestroyWindow);
        presentation::GamePresenter presenter(window.get(),std::filesystem::path(base)/"crt.metallib",
                                               preferences_folder,crt_mode,!smoke);
        std::vector<std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>> controllers;
        auto host = std::make_unique<FieldSessionHost>(pack, std::nullopt);
        host->set_save_folder(save_folder);
        bool running = true;
        while (running) {
            SDL_Event event{};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT ||
                    event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                    running = false;
                if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                    if (auto* pad = SDL_OpenGamepad(event.gdevice.which))
                        controllers.emplace_back(pad, SDL_CloseGamepad);
                } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                    std::erase_if(controllers, [&](const auto& pad) {
                        return SDL_GetGamepadID(pad.get()) == event.gdevice.which;
                    });
                    host->taps = host->second_taps = 0;
                }
                if (smoke)
                    continue;
                if (presenter.handle_event(event))
                    continue;
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.key == SDLK_ESCAPE)
                        running = false;
                    if (event.key.key == SDLK_P)
                        host->paused = !host->paused;
                    if (event.key.key == SDLK_M)
                        host->toggle_mute();
                    if (event.key.key == SDLK_F10)
                        host->toggle_logging();
                    for (const auto binding : scene_bindings)
                        if (event.key.scancode == binding.key)
                            host->taps |= ff6::button(binding.button);
                }
                if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    for (std::size_t i = 0; i < std::min<std::size_t>(2, controllers.size()); ++i)
                        if (SDL_GetGamepadID(controllers[i].get()) == event.gbutton.which)
                            for (const auto binding : scene_bindings)
                                if (event.gbutton.button == binding.pad)
                                    (i == 0 ? host->taps : host->second_taps) |=
                                        ff6::button(binding.button);
                }
                if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
                    event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                    host->clock.update(SDL_GetTicksNS(), false);
                    host->taps = host->second_taps = 0;
                    host->activate(false);
                }
            }
            const bool active = !host->paused && !host->halted() &&
                (smoke || (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0);
            host->clock.update(SDL_GetTicksNS(), active);
            if (!active)
                host->taps = host->second_taps = 0;
            for (unsigned work = 0, completed = 0;
                 active && work < 64 && completed < 8 && host->clock.due(); ++work) {
                const auto first = smoke ? 0 : mapped_buttons(
                    controllers.empty() ? nullptr : controllers[0].get(), scene_bindings);
                const auto second = smoke ? 0 : mapped_buttons(
                    controllers.size() < 2 ? nullptr : controllers[1].get(), scene_bindings, false);
                if (host->tick(pack, static_cast<ff6::Buttons>(first), static_cast<ff6::Buttons>(second))) {
                    host->clock.consume();
                    ++completed;
                }
                if (host->halted() || (smoke && host->session.ticks() >= 420))
                    break;
            }
            host->activate(active);
            if (smoke && host->halted())
                throw std::runtime_error(host->notice());
            ff6::FrameBuffer frame(256, 224, 0x000000ffU);
            if (const auto& visible = host->session.visible())
                for (std::size_t i = 0; i < visible->colors.size(); ++i)
                    frame.rgba[i] = ff6::rgba8(ff6::decode_color(ff6::quantize_intensity_rgb555(
                        visible->colors[i], visible->intensity[i / visible->width])));
            if (cycle_crt)
                presenter.select(static_cast<presentation::Mode>((host->session.ticks()/70U)%3U));
            check(SDL_SetWindowTitle(window.get(), host->halted() ? "Final Fantasy VI — stopped"
                : host->paused ? "Final Fantasy VI — paused" : "Final Fantasy VI"));
            std::string overlay = host->halted() ? "The game stopped.\n" + host->notice() : "";
            if (const auto logging = host->logging_notice(); !logging.empty())
                overlay += (overlay.empty() ? "" : "\n") + logging;
            presenter.present(frame, overlay);
            if (smoke && host->session.ticks() >= 420)
                running = false;
            else
                SDL_Delay(smoke ? 1 : 8);
        }
        if (smoke) {
            if (host->session.ticks() != 420)
                throw std::runtime_error("Game smoke test interrupted before completion");
            std::cout << "Game startup: ticks=" << host->session.ticks()
                      << " audio_frames=" << host->audio_frames << '\n';
            const auto status = presenter.status();
            std::cout << "Presentation: metal=" << status.metal
                      << " requested=" << presentation::name(status.requested)
                      << " effective=" << presentation::name(status.effective)
                      << " potential=" << status.potential << " current=" << status.current
                      << " frames=" << status.presented << " gpu_p95_ms=" << status.gpu_p95_ms
                      << " error=" << status.error << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Final Fantasy VI: " << error.what() << '\n';
        if (!smoke)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Final Fantasy VI", error.what(), nullptr);
        return 1;
    }
}
