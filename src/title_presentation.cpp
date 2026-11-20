#include "ff6/title_presentation.hpp"
#include "rom-materials/title_song_choice.macros.inc"

namespace ff6 {
TitlePresentation::TitlePresentation(TitleAnimationAssets assets, std::uint8_t random_index)
    : assets_(std::move(assets)), splash_(assets_.base), random_index_(random_index) {}
TitlePresentationTick TitlePresentation::advance(bool held_a, bool audio_ready) {
    TitlePresentationTick result;
    if (complete())
        return result;
    if (phase_ == TitlePresentationPhase::start) {
        // InitTitle uses the ordinary CPU song caller. Only the later restart
        // explicitly invalidates the current identity to defeat suppression.
        result.audio.push_back(GameSong{FF6_ROM_VALUE_TITLE_SONG_CHOICE_0, 0, false});
        RasterScene blank;
        blank.intensity.resize(blank.height);
        visible_ = std::move(blank);
        result.image_changed = true;
        phase_ = TitlePresentationPhase::loading;
        result.interval = true;
        return result;
    }
    if (phase_ == TitlePresentationPhase::loading) {
        if (!audio_ready) {
            result.interval = true;
            return result;
        }
        phase_ = TitlePresentationPhase::splash;
    }
    if (phase_ == TitlePresentationPhase::splash) {
        if (splash_.advance()) {
            input_.sample(held_a);
            visible_ = splash_.scene();
            result.image_changed = true;
            result.interval = result.input_sampled = true;
            return result;
        }
        animation_.emplace(std::move(assets_), splash_.state(), random_index_, input_);
        phase_ = TitlePresentationPhase::animation;
    }
    if (phase_ == TitlePresentationPhase::restarting) {
        if (!audio_ready) {
            result.interval = true;
            return result;
        }
        phase_ = TitlePresentationPhase::animation;
    }
    if (phase_ == TitlePresentationPhase::animation) {
        if (!animation_->advance(held_a))
            throw std::logic_error("Title animation ended without its sound-stop request");
        visible_ = animation_->scene();
        result.image_changed = true;
        result.interval = result.input_sampled = true;
        for (auto request : animation_->take_sound_requests()) {
            if (request == TitleSoundRequest::restart_theme) {
                result.audio.push_back(SongLoad{FF6_ROM_VALUE_TITLE_SONG_CHOICE_0, 255, false, false});
                phase_ = TitlePresentationPhase::restarting;
            } else {
                result.audio.push_back(StopPlayback{});
                phase_ = TitlePresentationPhase::stopping;
            }
        }
        return result;
    }
    if (phase_ == TitlePresentationPhase::stopping) {
        if (!audio_ready) {
            result.interval = true;
            return result;
        }
        if (animation_->advance())
            throw std::logic_error("Title requested sound stop before its final publication");
        phase_ = TitlePresentationPhase::complete;
        RasterScene blank;
        blank.intensity.resize(blank.height);
        visible_ = std::move(blank);
        result.image_changed = true;
    }
    return result;
}
bool TitlePresentation::skipped() const {
    if (!complete())
        throw std::logic_error("Title result is not ready");
    return animation_->state().skipped;
}
std::uint32_t TitlePresentation::visits() const {
    return splash_.state().visits + (animation_ ? animation_->state().visits : 0);
}
} // namespace ff6
