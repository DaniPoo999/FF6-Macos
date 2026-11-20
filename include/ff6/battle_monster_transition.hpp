#pragma once
#include "ff6/battle_effects.hpp"
#include "rom-materials/monster_dissolve_controls.macros.inc"
#include "rom-materials/monster_dissolve_origin.macros.inc"

namespace ff6 {
// DoMonsterEntryExit has two native routines outside its animation directory.
// Their FF directory entries do not mean an instant visibility change.
class BattleMonsterTransition {
  public:
    BattleMonsterTransition(const BattleEffectLibrary& library, unsigned kind,
                            bool entering, const BattleEffectContext& context);
    std::optional<PlaybackVolumeRamp> advance(BattleEffectContext& context);
    void compose(RasterScene& scene) const;
    bool complete() const { return complete_; }
    unsigned elapsed() const { return age_; }
    const auto& remaining_pixels() const { return remaining_; }
    const auto& falling_pixels() const { return falling_; }

  private:
    unsigned kind_, age_ = 0, flash_step_ = 0, flash_wait_ = 0;
    bool entering_, complete_ = false;
    std::bitset<6> selected_, original_shown_;
    std::array<std::uint8_t, 64> flash_delays_{};
    std::array<std::uint16_t, 56> masks_{};
    std::array<unsigned, 16> band_age_{};
    Bytes remaining_, falling_;
    std::array<Rgb555, 16> colors_{};
    int x_ = 0x10000 - FF6_ROM_VALUE_MONSTER_DISSOLVE_ORIGIN_0;
    int y_ = 0x10000 - FF6_ROM_VALUE_MONSTER_DISSOLVE_ORIGIN_0, fall_offset_ = 0;
    unsigned scroll_divider_ = FF6_ROM_VALUE_MONSTER_DISSOLVE_CONTROLS_4;
    void dissolve(unsigned iteration);
};
} // namespace ff6
