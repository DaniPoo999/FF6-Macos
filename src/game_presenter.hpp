#pragma once
#include "ff6/video.hpp"
#include "presentation.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <optional>

namespace presentation {
struct Status {
    bool metal{};
    Mode requested{}, effective{};
    float potential{1}, current{1};
    std::uint64_t presented{}, coalesced{};
    double gpu_p95_ms{};
    std::string error;
};
class Backend {
  public:
    virtual ~Backend() = default;
    virtual void present(const ff6::FrameBuffer& frame, Mode requested) = 0;
    virtual void overlay(const std::string& text) = 0;
    virtual Status status() const = 0;
};
#ifdef __APPLE__
std::unique_ptr<Backend> make_metal_backend(SDL_Window* window,
                                          const std::filesystem::path& library);
#endif
class GamePresenter {
  public:
    GamePresenter(SDL_Window* window, const std::filesystem::path& library,
                  std::filesystem::path preferences, std::optional<Mode> initial = {},
                  bool persist = true, std::unique_ptr<Backend> backend = {});
    ~GamePresenter();
    bool handle_event(const SDL_Event& event);
    void select(Mode mode, bool save = false);
    void present(const ff6::FrameBuffer& frame, const std::string& notice = {});
    Status status() const;
  private:
    SDL_Window* window_;
    std::filesystem::path preferences_;
    std::unique_ptr<Backend> backend_;
    Mode requested_{}, effective_{};
    bool persist_;
    std::uint64_t notice_until_{};
    std::string failure_;
    void fallback(const std::string& error);
};
} // namespace presentation
