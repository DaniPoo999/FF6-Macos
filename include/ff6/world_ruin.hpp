#pragma once
#include "rom-materials/ruin_phase_waits.macros.inc"
#include "rom-materials/ruin_initial_fade_period.macros.inc"
#include "ff6/title.hpp"
#include "ff6/cutscene_animation.hpp"

namespace ff6 {
struct WorldRuinAssets {
    TileLayer terrain;
    std::array<CutscenePalette, 4> palettes{};
    std::vector<std::vector<CutsceneSprite>> sprites;
    // Two land plates, sparks, cracks, and the looping fire seam.
    std::array<std::vector<CutsceneAnimationFrame>, 5> animations;
    std::array<std::pair<std::uint8_t, std::uint8_t>, 10> fire_positions{};
    std::array<std::pair<std::uint8_t, std::uint8_t>, 4> crack_positions{};
    std::array<std::uint8_t, 256> random{};
};
WorldRuinAssets load_world_ruin_assets(const ContentPack& pack);
class WorldRuin {
  public:
    explicit WorldRuin(WorldRuinAssets assets);
    bool advance();
    // Three phase waits, the final fade, and two explicit state handoffs.
    bool complete() const {
        return updates_ >= FF6_ROM_VALUE_RUIN_PHASE_WAITS_0 +
            FF6_ROM_VALUE_RUIN_PHASE_WAITS_1 + FF6_ROM_VALUE_RUIN_PHASE_WAITS_2 +
            FF6_ROM_VALUE_RUIN_PHASE_WAITS_3 + 2;
    }
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    struct Effect {
        unsigned animation;
        int x, y; // Eight fractional bits, signed native coordinates.
        CutsceneAnimationCursor cursor;
    };
    WorldRuinAssets assets_;
    std::array<CutscenePalette, 3> colors_{};
    // Each original palette creator supplies this independently verified period.
    std::array<CutscenePaletteFade, 3> fades_{{
        {FF6_ROM_VALUE_RUIN_INITIAL_FADE_PERIOD_0},
        {FF6_ROM_VALUE_RUIN_INITIAL_FADE_PERIOD_0},
        {FF6_ROM_VALUE_RUIN_INITIAL_FADE_PERIOD_0}}};
    std::vector<Effect> effects_;
    unsigned updates_ = 0, fires_ = 0;
    std::uint8_t random_index_ = 0;
    std::uint8_t random();
    void spawn(unsigned animation, int x, int y);
};
} // namespace ff6
