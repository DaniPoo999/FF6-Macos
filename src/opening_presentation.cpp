#include "ff6/opening_presentation.hpp"
#include "rom-materials/opening_song_choice.macros.inc"

namespace ff6 {
OpeningPresentation::OpeningPresentation(OpeningAssets assets, std::uint8_t random_index)
    : animation_(std::move(assets), random_index) {}
CutscenePresentationTick OpeningPresentation::advance(bool held_a, bool audio_ready) {
    CutscenePresentationTick result;
    if (complete())
        return result;
    if (phase_ == OpeningPresentationPhase::start) {
        // InitOpening uses the ordinary song caller at full volume. Readiness
        // must finish before InitRAM's presentation/input owners start ticking.
        result.audio.push_back(GameSong{FF6_ROM_VALUE_OPENING_SONG_CHOICE_0, 255, false});
        RasterScene blank;
        blank.intensity.resize(blank.height);
        visible_ = std::move(blank);
        result.image_changed = true;
        phase_ = OpeningPresentationPhase::loading;
        result.interval = true;
        return result;
    }
    if (phase_ == OpeningPresentationPhase::loading) {
        if (!audio_ready) {
            result.interval = true;
            return result;
        }
        phase_ = OpeningPresentationPhase::animation;
    }
    if (phase_ == OpeningPresentationPhase::animation) {
        result.interval = true;
        if (held_intervals_) {
            --held_intervals_;
            return result;
        }
        if (!animation_.advance(held_a))
            throw std::logic_error("Opening animation ended before its parent stopped sound");
        visible_ = animation_.scene();
        result.image_changed = true;
        result.input_sampled = true;
        if (animation_.finished()) {
            // SoundOff follows the last WaitVBlank; don't defer it by a held
            // interval or let the field resume while its command is pending.
            result.audio.push_back(StopPlayback{});
            phase_ = OpeningPresentationPhase::stopping;
        } else
            held_intervals_ = opening_publication_intervals - 1;
        return result;
    }
    if (!audio_ready) {
        result.interval = true;
        return result;
    }
    if (animation_.advance())
        throw std::logic_error("Opening requested sound stop before its final publication");
    phase_ = OpeningPresentationPhase::complete;
    RasterScene blank;
    blank.intensity.resize(blank.height);
    visible_ = std::move(blank);
    result.image_changed = true;
    return result;
}
bool OpeningPresentation::skipped() const {
    if (!complete())
        throw std::logic_error("Opening result is not ready");
    return animation_.state().skipped;
}
} // namespace ff6
