#pragma once
#include "ff6/cutscene_presentation.hpp"
#include "ff6/title_animation.hpp"

namespace ff6 {
enum class TitlePresentationPhase {
    start,
    loading,
    splash,
    animation,
    restarting,
    stopping,
    complete
};
using TitlePresentationTick = CutscenePresentationTick;
// TitleMain's native parent. Audio readiness is a handler-completion signal,
// never a device-buffer clock or a reproduction of ROM upload duration.
class TitlePresentation {
  public:
    explicit TitlePresentation(TitleAnimationAssets assets, std::uint8_t random_index = 0);
    TitlePresentationTick advance(bool held_a, bool audio_ready);
    TitlePresentationPhase phase() const {
        return phase_;
    }
    bool complete() const {
        return phase_ == TitlePresentationPhase::complete;
    }
    bool skipped() const;
    std::uint32_t visits() const;
    const std::optional<RasterScene>& visible() const {
        return visible_;
    }

  private:
    TitleAnimationAssets assets_;
    TitleSplash splash_;
    std::optional<TitleAnimation> animation_;
    std::uint8_t random_index_;
    CutsceneInput input_{};
    TitlePresentationPhase phase_ = TitlePresentationPhase::start;
    std::optional<RasterScene> visible_;
};
} // namespace ff6
