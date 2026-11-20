#include "ff6/battle_monster_death.hpp"
#include "rom-materials/monster_death_color_step.macros.inc"
#include "rom-materials/monster_death_composite.macros.inc"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/audio_loading_choices.macros.inc"

namespace ff6 {
namespace {
// Flags identify the existing terrain/object consumers. RGB555 replication and
// the typed blend conversion describe formats; their chosen inputs are private.
static_assert((FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_0 & 2U) != 0);
static_assert((FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_1 & 0x3fU) == 2U);
constexpr auto death_blend = composite_blend(
    (FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_1 & 0x80U) != 0,
    (FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_1 & 0x40U) != 0,
    (FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_0 & 2U) == 0).covered;
} // namespace
BattleMonsterDeathPalette load_battle_monster_death_palette(const ContentPack& pack) {
    const auto data = pack.get("battle_monster_death_palette");
    if (data.size() != 32)
        throw std::invalid_argument("Invalid monster death palette extent");
    BattleMonsterDeathPalette colors;
    for (unsigned i = 0; i < colors.size(); ++i)
        colors[i] = read16(data, i * 2);
    if (std::ranges::any_of(colors, [](auto c) { return c > 32767; }))
        throw std::invalid_argument("Invalid monster death color");
    return colors;
}
BattleMonsterDeath::BattleMonsterDeath(BattleMonsterDeathPalette colors, std::bitset<6> targets,
                                       const std::array<BattleMonsterPosition, 6>& positions)
    : colors_(colors), targets_(targets) {
    if (std::ranges::any_of(colors, [](auto c) { return c > 32767; }))
        throw std::invalid_argument("Invalid monster death color");
    // MonsterDeathAnim visits ascending slots; the last dying monster supplies
    // the sound's horizontal position, not its center or the first target.
    for (unsigned i = 0; i < 6; ++i)
        if (targets[i])
            pan_ = wrap8(positions[i].x);
}
std::optional<GameSoundEffect> BattleMonsterDeath::advance() {
    if (complete())
        return {};
    ++ticks_;
    // One setup wait, two foreground clears and one overlay clear. The sound
    // precedes the first displayed death palette by one publication boundary.
    if (ticks_ == 4)
        return GameSoundEffect{FF6_ROM_VALUE_AUDIO_LOADING_CHOICES_9, pan_};
    // Each channel decreases independently. Subtracting a packed RGB555 word
    // would incorrectly borrow between channels. The extracted update count
    // reaches black before removing the additive silhouette.
    if (ticks_ > 5)
        for (auto& color : colors_)
            color = blend_color(color, FF6_ROM_VALUE_MONSTER_DEATH_COLOR_STEP_0 * (1U | (1U << 5) | (1U << 10)),
                                ColorBlend::subtract);
    return {};
}
void BattleMonsterDeath::apply_to(std::vector<BattleActorSpriteGroup>& actors) const {
    // Colors publish one frame before the prepared low-priority pieces.
    if (ticks_ < 6 && !complete())
        return;
    for (auto& actor : actors)
        if (actor.participant >= 4 && actor.participant < 10 && targets_[actor.participant - 4]) {
            if (complete())
                actor.sprites.clear();
            else
                for (auto& piece : actor.sprites) {
                    piece.image.colors = colors_;
                    piece.image.depth = 2;
                    piece.image.mix = {};
                    piece.color_math = false;
                }
        }
}
void BattleMonsterDeath::compose(RasterScene& scene) const {
    if (!fading())
        return;
    if (scene.layers.empty() || !scene.objects)
        throw std::invalid_argument("Monster death requires an ordinary battlefield scene");
    // Source C1/9435 uses BG2 as the affected surface and objects as its
    // additive operand. Keeping object ordering preserves foreground actors
    // and numerals; fading the whole final image would affect those too.
    scene.layers.front().mix = {death_blend, ColorBlend::none};
    scene.objects->operand = (FF6_ROM_VALUE_MONSTER_DEATH_COMPOSITE_2 & 0x10U) != 0;
}
} // namespace ff6
