#include "game_presenter.hpp"
#include <iostream>
#include <stdexcept>

namespace presentation {
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error(SDL_GetError()); }
class SDLBackend final : public Backend {
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer_{nullptr, SDL_DestroyRenderer};
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture_{nullptr, SDL_DestroyTexture};
    std::string overlay_;
    Status status_;
  public:
    explicit SDLBackend(SDL_Window* window) {
        renderer_.reset(SDL_CreateRenderer(window, nullptr));
        check(renderer_ != nullptr);
        check(SDL_SetRenderLogicalPresentation(renderer_.get(),256,224,SDL_LOGICAL_PRESENTATION_LETTERBOX));
        texture_.reset(SDL_CreateTexture(renderer_.get(),SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STREAMING,256,224));
        check(texture_ != nullptr);
        check(SDL_SetTextureScaleMode(texture_.get(),SDL_SCALEMODE_NEAREST));
    }
    void overlay(const std::string& text) override { overlay_ = text; }
    Status status() const override { return status_; }
    void present(const ff6::FrameBuffer& frame, Mode requested) override {
        status_.requested = requested;
        check(SDL_UpdateTexture(texture_.get(),nullptr,frame.rgba.data(),frame.width*4));
        check(SDL_SetRenderDrawColor(renderer_.get(),0,0,0,255));
        check(SDL_RenderClear(renderer_.get()));
        check(SDL_RenderTexture(renderer_.get(),texture_.get(),nullptr,nullptr));
        if (!overlay_.empty()) {
            check(SDL_SetRenderDrawColor(renderer_.get(),255,255,255,255));
            check(SDL_RenderDebugText(renderer_.get(),8,8,overlay_.c_str()));
        }
        check(SDL_RenderPresent(renderer_.get()));
        ++status_.presented;
    }
};
}
GamePresenter::GamePresenter(SDL_Window* window, const std::filesystem::path& library,
                             std::filesystem::path preferences, std::optional<Mode> initial,
                             bool persist, std::unique_ptr<Backend> backend)
    : window_(window), preferences_(std::move(preferences)),
      backend_(std::move(backend)),
      requested_(initial.value_or(persist ? load_preference(preferences_) : Mode::off)), persist_(persist) {
#ifdef __APPLE__
    if (!backend_ && std::string_view(SDL_GetCurrentVideoDriver()) == "cocoa") {
        try { backend_ = make_metal_backend(window,library); }
        catch (const std::exception& e) { failure_ = e.what(); }
    }
#else
    (void)library;
#endif
    if (!backend_) fallback(failure_);
    if (requested_ != Mode::off || !failure_.empty()) notice_until_ = SDL_GetTicks() + 4000;
}
GamePresenter::~GamePresenter() = default;
void GamePresenter::fallback(const std::string& error) {
    failure_ = error;
    backend_.reset();
    backend_ = std::make_unique<SDLBackend>(window_);
    if (!error.empty()) std::cerr << "Presentation fallback: " << error << '\n';
    notice_until_ = SDL_GetTicks() + 4000;
}
bool GamePresenter::handle_event(const SDL_Event& event) {
    if (event.type != SDL_EVENT_KEY_DOWN || event.key.scancode != SDL_SCANCODE_1) return false;
    if (!event.key.repeat) select(next(requested_),true);
    return true;
}
void GamePresenter::select(Mode mode, bool save) {
    if (mode == requested_) return;
    requested_ = mode;
    notice_until_ = SDL_GetTicks() + 3000;
    if (save && persist_) {
        try { save_preference(preferences_,mode); }
        catch (const std::exception& e) {
            // A failed host preference write must never stop a gameplay session.
            std::cerr << "Cannot save CRT preference: " << e.what() << '\n';
        }
    }
}
Status GamePresenter::status() const {
    auto result = backend_->status();
    result.requested = requested_;
    if (result.error.empty()) result.error = failure_;
    return result;
}
void GamePresenter::present(const ff6::FrameBuffer& frame, const std::string& notice) {
    if (frame.width != 256 || frame.height != 224 || frame.rgba.size() != 256U*224U)
        throw std::invalid_argument("Expected a complete 256x224 presentation frame");
    auto state = backend_->status();
    if (state.metal && !state.error.empty()) fallback(state.error);
    // Backend display queries and rendering never receive a game-state reference.
    try { backend_->present(frame,requested_); }
    catch (const std::exception& e) {
        if (!backend_->status().metal) throw;
        fallback(e.what());
        backend_->present(frame,requested_);
    }
    state = backend_->status();
    if (state.effective != effective_) {
        effective_ = state.effective;
        notice_until_ = SDL_GetTicks() + 3000;
    }
    std::string message = notice;
    if (SDL_GetTicks() < notice_until_) {
        if (!message.empty()) message += '\n';
        message += requested_ == Mode::off ? "CRT Off" : requested_ == Mode::sdr ? "CRT SDR" : "CRT HDR";
        if (requested_ != Mode::off && !state.metal) message += " - unavailable; unfiltered";
        else if (requested_ == Mode::hdr && effective_ != Mode::hdr) message += " - SDR until HDR is available";
    }
    backend_->overlay(message);
}
} // namespace presentation
