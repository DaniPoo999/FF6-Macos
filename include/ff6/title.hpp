#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"
#include "rom-materials/title_phase_waits.macros.inc"
#include "rom-materials/title_splash_fade_period.macros.inc"

namespace ff6 {
using CutscenePalette = std::array<Rgb555, 16>;
// Original fade steps use a descending threshold when brightening, but move
// down one component value on every darkening update. This is not interpolation.
Rgb555 step_cutscene_fade(Rgb555 current, Rgb555 target, std::uint8_t threshold);
struct CutscenePaletteFade {
    std::uint8_t period = 1, wait = 0, threshold = 31;
    bool initialized = false, finished = false;
    void advance(CutscenePalette& current, const CutscenePalette& target);
};
struct TitleAssets {
    std::array<TileLayer, 3> backgrounds;
    std::vector<RasterSprite> legal_text;
    std::vector<Tile> sprite_tiles;
    std::size_t first_legal_group_size = 0;
    CutscenePalette splash, white, black;
    std::array<CutscenePalette, 9> palettes;
};
TitleAssets load_title_assets(const ContentPack& pack);
enum class SplashPhase { holding, terminating, fading, complete };
struct TitleSplashState {
    SplashPhase phase = SplashPhase::holding;
    std::uint16_t remaining = FF6_ROM_VALUE_TITLE_PHASE_WAITS_0;
    std::uint32_t visits = 0;
    std::uint8_t frame_counter = 0, intensity = 15;
    std::array<CutscenePalette, 3> colors; // Background 0/1 and legal-text colors.
};
// Owns the unskippable English splash and its final fade. The next title owner
// must consume completion; this never acknowledges/skips the field cutscene.
class TitleSplash {
  public:
    explicit TitleSplash(TitleAssets assets);
    // One logical presentation visit. False means no new frame at completion.
    // The original splash has no input gate, independent of held buttons.
    bool advance();
    const TitleSplashState& state() const {
        return state_;
    }
    RasterScene scene() const;

  private:
    TitleAssets assets_;
    TitleSplashState state_{};
    // SplashState00 creates three fades with an authenticated common period.
    std::array<CutscenePaletteFade, 3> fades_{{
        {FF6_ROM_VALUE_TITLE_SPLASH_FADE_PERIOD_0},
        {FF6_ROM_VALUE_TITLE_SPLASH_FADE_PERIOD_0},
        {FF6_ROM_VALUE_TITLE_SPLASH_FADE_PERIOD_0}}};
};
} // namespace ff6
