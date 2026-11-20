#pragma once
#include "rom-materials/continent_phase_waits.macros.inc"
#include "rom-materials/continent_palette_period.macros.inc"
#include "rom-materials/continent_tint_period.macros.inc"
#include "rom-materials/continent_view.macros.inc"
#include "rom-materials/continent_initial_scale.macros.inc"

#include "ff6/title.hpp"

namespace ff6 {
struct FloatingContinentAssets {
    AffineLayer terrain;
    CutscenePalette palette{};
};
FloatingContinentAssets decode_floating_continent_assets(ByteView graphics, ByteView palette);
FloatingContinentAssets load_floating_continent_assets(const ContentPack& pack);

// The field visit and its music remain suspended/playing respectively. This
// owner supplies only the authored zoom, scroll, palette and final brightness.
enum class ContinentRisePhase { revealing, rising, receding, finishing, fading, complete };
class FloatingContinentRise {
  public:
    explicit FloatingContinentRise(FloatingContinentAssets assets);
    bool advance();
    bool complete() const { return phase_ == ContinentRisePhase::complete; }
    ContinentRisePhase phase() const { return phase_; }
    unsigned updates() const { return updates_; }
    RasterScene scene() const;

  private:
    FloatingContinentAssets assets_;
    CutscenePalette colors_{};
    CutscenePaletteFade fade_{FF6_ROM_VALUE_CONTINENT_PALETTE_PERIOD_0};
    ContinentRisePhase phase_ = ContinentRisePhase::revealing;
    unsigned remaining_ = FF6_ROM_VALUE_CONTINENT_PHASE_WAITS_0, updates_ = 0;
    std::int16_t scale_ = FF6_ROM_VALUE_CONTINENT_INITIAL_SCALE_0;
    std::int16_t visible_scale_ = FF6_ROM_VALUE_CONTINENT_INITIAL_SCALE_0;
    std::int16_t scroll_y_ = static_cast<std::int16_t>(FF6_ROM_VALUE_CONTINENT_VIEW_3);
    std::uint8_t intensity_ = 15;
    std::uint8_t tint_wait_ = FF6_ROM_VALUE_CONTINENT_TINT_PERIOD_0, tint_steps_ = 0;
    Rgb555 tint_ = 0;
    bool tint_enabled_ = false;
    void next_phase();
};
} // namespace ff6
