#pragma once
#include "ff6/cutscene_presentation.hpp"
#include "ff6/opening.hpp"

namespace ff6 {
enum class OpeningPresentationPhase { start, loading, animation, stopping, complete };
// Native development cadence: one credits update per two 60 Hz simulation
// intervals. This is independent of monitor refresh and the audio callback.
// The source's workload-dependent early faster updates remain a documented
// timing difference, not a generic CPU/device scheduler. See OPENING.md.
inline constexpr unsigned opening_publication_intervals = 2;
class OpeningPresentation {
  public:
    explicit OpeningPresentation(OpeningAssets assets, std::uint8_t random_index = 0);
    CutscenePresentationTick advance(bool held_a, bool audio_ready);
    OpeningPresentationPhase phase() const {
        return phase_;
    }
    bool complete() const {
        return phase_ == OpeningPresentationPhase::complete;
    }
    bool skipped() const;
    std::uint32_t visits() const {
        return animation_.state().visits;
    }
    const std::optional<RasterScene>& visible() const {
        return visible_;
    }

  private:
    OpeningAnimation animation_;
    OpeningPresentationPhase phase_ = OpeningPresentationPhase::start;
    std::optional<RasterScene> visible_;
    unsigned held_intervals_ = 0;
};
} // namespace ff6
