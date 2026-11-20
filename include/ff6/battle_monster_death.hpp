#pragma once
#include "ff6/battle_actor_scene.hpp"
#include "ff6/music_player.hpp"
#include <bitset>
#include "rom-materials/monster_death_palette_steps.macros.inc"

namespace ff6 {
using BattleMonsterDeathPalette = std::array<Rgb555, 16>;
BattleMonsterDeathPalette load_battle_monster_death_palette(const ContentPack& pack);

// Ordinary deaths share one palette and one sound, even when several monsters
// die together. The caller selects shown, eligible, newly dead monsters. Boss,
// cleave, escape and flashback presentation require their separate owners.
class BattleMonsterDeath {
  public:
    BattleMonsterDeath(BattleMonsterDeathPalette colors, std::bitset<6> targets,
                       const std::array<BattleMonsterPosition, 6>& positions);
    std::optional<GameSoundEffect> advance();
    bool complete() const {
        return targets_.none() || ticks_ >= FF6_ROM_VALUE_MONSTER_DEATH_PALETTE_STEPS_0 + 4;
    }
    bool fading() const {
        return targets_.any() && ticks_ >= 5 && ticks_ < FF6_ROM_VALUE_MONSTER_DEATH_PALETTE_STEPS_0 + 4;
    }
    std::bitset<6> hidden() const {
        return complete() ? targets_ : std::bitset<6>{};
    }
    unsigned elapsed() const {
        return ticks_;
    }
    const auto& colors() const {
        return colors_;
    }
    // Apply to actor pieces BEFORE adding numerals: those retain their own
    // palette, foreground priority and preceding-frame publication.
    void apply_to(std::vector<BattleActorSpriteGroup>& actors) const;
    // Fresh ordinary battlefield scene; layer zero is its terrain surface.
    void compose(RasterScene& scene) const;

  private:
    BattleMonsterDeathPalette colors_;
    std::bitset<6> targets_;
    unsigned ticks_ = 0;
    std::uint8_t pan_ = 0;
};
} // namespace ff6
