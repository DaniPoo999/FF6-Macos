#pragma once
#include "ff6/raster.hpp"
#include <array>

namespace ff6 {
inline constexpr unsigned battle_effect_rows = 151;
// A semantic color operation, not a resolved RGB row. Unselected components
// survive from the preceding *displayed* row, including across a late write.
struct BattleGradientColor {
    std::uint8_t components = 7, value = 0;
    bool operator==(const BattleGradientColor&) const = default;
};
struct BattleGradientWrite {
    std::uint16_t row;
    BattleGradientColor color;
};
struct BattleMaskSpan {
    std::uint16_t left = 0, right = 0; // Half-open; equal/reversed edges cover no pixels.
    bool operator==(const BattleMaskSpan&) const = default;
};
// First row whose sample has not yet occurred. A write at its row's deadline
// is visible; one after it is retained for the next picture. This boundary is
// supplied by the caller, never inferred from host time. A lower bound can
// establish already-passed rows without specifying an exact write timestamp.
struct BattleRasterDeadline {
    std::uint16_t first_unread_row = 0;
};
class BattleEffectRaster {
  public:
    using Colors = std::array<BattleGradientColor, battle_effect_rows>;
    using Mask = std::array<BattleMaskSpan, battle_effect_rows>;
    void reset(const Colors& colors, const Mask& mask);
    void begin_picture();
    void write_gradient(BattleGradientWrite write, BattleRasterDeadline deadline);
    // Separate edges preserve publication interrupted between the two stores.
    void write_mask_edge(unsigned row, bool right, std::uint16_t edge,
                         BattleRasterDeadline deadline);
    std::array<Rgb555, battle_effect_rows> colors() const;
    const Mask& mask() const {
        return sampled_mask_;
    }

  private:
    Colors retained_colors_{}, sampled_colors_{};
    Mask retained_mask_{}, sampled_mask_{};
    std::array<std::uint16_t, 3> last_deadline_{};
};
} // namespace ff6
