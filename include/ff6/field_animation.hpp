#pragma once
#include "ff6/field_map.hpp"

namespace ff6 {
struct BackgroundAnimationGroup {
    std::uint16_t speed = 0;
    std::array<std::array<Tile, 4>, 4> frames{};
};
struct ThirdBackgroundAnimation {
    std::uint16_t speed = 0;
    std::array<std::vector<Tile>, 8> frames;
};
enum class PaletteAnimationKind { inactive, counter, cycle, colors, pulse };
struct PaletteAnimationChannel {
    PaletteAnimationKind kind = PaletteAnimationKind::inactive;
    std::uint8_t speed = 0, count = 0;
    std::uint16_t first_color = 0, color_count = 0;
    std::vector<std::vector<Rgb555>> frames;
};
struct FieldAnimationAssets {
    bool backgrounds_enabled = false;
    std::array<BackgroundAnimationGroup, 8> backgrounds;
    std::optional<ThirdBackgroundAnimation> third;
    std::array<PaletteAnimationChannel, 2> palettes;
    std::vector<std::uint8_t> pulse_curve;
};
FieldAnimationAssets load_field_animation(const ContentPack& pack,
                                          const FieldMapDefinition& definition);
struct PaletteAnimationClock {
    std::uint8_t tick = 0, index = 0;
    bool operator==(const PaletteAnimationClock&) const = default;
};
// Returns true only on the complete index wrap, not every tick reset.
bool advance_palette_clock(PaletteAnimationClock& clock, std::uint8_t speed, std::uint8_t count);
struct FieldAnimationState {
    std::array<std::uint16_t, 8> backgrounds{};
    std::uint16_t third = 0;
    std::array<PaletteAnimationClock, 2> palettes;
    bool operator==(const FieldAnimationState&) const = default;
};
enum class BackgroundAnimationPhase { first_and_second, third };
class FieldAnimation {
  public:
    // Validates all active windows once. Content and counters are then owned here.
    explicit FieldAnimation(FieldAnimationAssets assets);
    const FieldAnimationState& state() const {
        return state_;
    }
    // Event 65 uses signed magnitude, with sixteen fractional steps per unit.
    // Changing speed preserves the group phase; zero freezes its current frame.
    void set_background_rate(std::uint8_t group, std::uint8_t rate);
    void advance_backgrounds(FieldMap& map, BackgroundAnimationPhase phase);
    // Base colors are separately owned; pulses never feed back their prior output.
    void advance_palette(const std::array<Rgb555, 128>& base, std::array<Rgb555, 128>& destination);

  private:
    FieldAnimationAssets assets_;
    FieldAnimationState state_;
};
} // namespace ff6
