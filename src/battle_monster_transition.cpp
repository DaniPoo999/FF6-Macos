#include "ff6/battle_monster_transition.hpp"
#include "rom-materials/monster_dissolve_timeline.macros.inc"
#include "rom-materials/monster_dissolve_checkpoints.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
BattleMonsterTransition::BattleMonsterTransition(const BattleEffectLibrary& library,
    unsigned kind, bool entering, const BattleEffectContext& context)
    : kind_(kind), entering_(entering), original_shown_(context.shown_monsters) {
    for (unsigned i = 0; i < 6; ++i) selected_[i] = context.targets[i + 4];
    if (selected_.none() || (kind != 13 && kind != 17))
        throw std::invalid_argument("Invalid special monster transition");
    if (kind == 13) {
        if (!library.monster_flash_delays || std::ranges::find(*library.monster_flash_delays, 0) != library.monster_flash_delays->end())
            throw std::invalid_argument("Missing monster flash durations");
        for (unsigned i = 0; i < 6; ++i)
            if (!context.control.monster_entry_visible[i]) original_shown_.reset(i);
        flash_delays_ = *library.monster_flash_delays;
        flash_wait_ = flash_delays_[0];
        return;
    }
    if (entering || selected_.count() != 1 || !context.monster_images || !library.monster_dissolve_masks)
        throw std::invalid_argument("Final dissolve requires one exiting monster and its image");
    masks_ = *library.monster_dissolve_masks;
    unsigned slot = 0;
    while (!selected_[slot]) ++slot;
    // KefkaDeathAnim_far reloads the full sixteen-column monster image. Work
    // on palette indices, retaining each plane's bits, without a graphics bus.
    const auto& source = *context.monster_images;
    if (!source.first_tiles[slot] || *source.first_tiles[slot] != 0)
        throw std::invalid_argument("Final dissolve requires the full monster image owner");
    remaining_.resize(128 * 128); falling_.resize(128 * 128);
    for (unsigned y = 0; y < 128; ++y)
        for (unsigned x = 0; x < 128; ++x)
            remaining_[y * 128 + x] = source.tiles[(y / 8) * 16 + x / 8][(y % 8) * 8 + x % 8];
    colors_ = context.monster_colors.at(context.monster_styles[slot].palette);
    x_ = context.monsters[slot].x; y_ = context.monsters[slot].y;
}
void BattleMonsterTransition::dissolve(unsigned iteration) {
    // C2/AA70 visits bottom-to-top tile bands. The original stagger and
    // mask-step limit are extracted; both plane pairs share the same mask.
    for (unsigned band = 0; band < 16; ++band) {
        if (iteration < band * FF6_ROM_VALUE_MONSTER_DISSOLVE_CONTROLS_0 ||
            band_age_[band] >= FF6_ROM_VALUE_MONSTER_DISSOLVE_CONTROLS_1) continue;
        const auto age = band_age_[band]++;
        for (unsigned row = 0; row < 8; ++row) {
            const auto mask = masks_[age + row];
            const unsigned y = (15 - band) * 8 + row;
            for (unsigned x = 0; x < 128; ++x) {
                const unsigned bit = 7 - x % 8;
                const auto keep = static_cast<std::uint8_t>(((mask >> bit) & 1U) * 5U |
                                                          ((mask >> (bit + 8)) & 1U) * 10U);
                const auto index = y * 128 + x;
                falling_[index] |= static_cast<std::uint8_t>(remaining_[index] & (keep ^ 15U));
                remaining_[index] &= keep;
            }
        }
    }
}
std::optional<PlaybackVolumeRamp> BattleMonsterTransition::advance(BattleEffectContext& context) {
    if (complete_) return {};
    ++age_;
    if (kind_ == 13) {
        // C2/E88E alternates the OLD full visibility set with only arrivals.
        // This intentionally flashes stationary monsters on an exit as well.
        context.shown_monsters = (flash_step_ & 1U) ? (entering_ ? selected_ : std::bitset<6>{})
                                                    : original_shown_;
        context.control.monster_entry_visible.fill(true);
        for (unsigned i = 0; i < 6; ++i)
            if (selected_[i]) context.control.monster_visible[i] = true;
        if (--flash_wait_ == 0) {
            if (++flash_step_ == 64) {
                context.shown_monsters = entering_ ? original_shown_ | selected_ : original_shown_ & ~selected_;
                complete_ = true;
            } else flash_wait_ = flash_delays_[(flash_step_ & 1U ? 32U : 0U) + flash_step_ / 2];
        }
        return {};
    }
    context.shown_monsters &= ~selected_;
    if (age_ > FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_0 &&
        age_ <= FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_0 + FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_1) {
        // Each original loop contains four explicit frame publications. The
        // extracted iteration count uses that same native frame-unit conversion.
        const unsigned frame = age_ - (FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_0 + 1);
        const unsigned iteration = frame / 4;
        if (iteration >= FF6_ROM_VALUE_MONSTER_DISSOLVE_CHECKPOINTS_0 && frame % 2 == 0) {
            if (--scroll_divider_ == 0) scroll_divider_ = FF6_ROM_VALUE_MONSTER_DISSOLVE_CONTROLS_2;
            else ++fall_offset_;
        }
        // C2/AAF8 carries each bottom pixel row into the next tile, once per
        // displayed frame. The transparent top row is explicitly initialized.
        std::move_backward(falling_.begin(), falling_.end() - 128, falling_.end());
        std::fill_n(falling_.begin(), 128, 0);
        if (frame % 4 == 3) dissolve(iteration);
        if (iteration == FF6_ROM_VALUE_MONSTER_DISSOLVE_CHECKPOINTS_1 - 1 && frame % 4 == 3)
            return PlaybackVolumeRamp{PlaybackGroup::effects, 0, FF6_ROM_VALUE_MONSTER_DISSOLVE_CONTROLS_3};
    }
    complete_ = age_ >= FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_0 +
                        FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_1 + FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_2;
    return {};
}
void BattleMonsterTransition::compose(RasterScene& scene) const {
    if (kind_ != 17 || age_ == 0 || complete_) return;
    if (!scene.objects) scene.objects.emplace();
    // Original sprites retain the dissolving image while BG1 owns the falling
    // fragments at the extracted origin. Their shared adjustment preserves overlap.
    const auto append = [&](const Bytes& pixels, int x, int y) {
        // The ordinary compositor accepts bounded pieces. Splitting the native
        // image here preserves that contract without changing its pixel layout.
        for (unsigned row = 0; row < 2; ++row) for (unsigned col = 0; col < 2; ++col) {
            RasterSprite part;
            part.x = x + int(col * 64); part.y = y + int(row * 64);
            part.width = part.height = 64; part.depth = 8; part.colors = colors_;
            part.pixels.resize(64 * 64);
            for (unsigned py = 0; py < 64; ++py)
                std::copy_n(pixels.begin() + (row * 64 + py) * 128 + col * 64, 64,
                            part.pixels.begin() + py * 64);
            scene.objects->sprites.push_back(std::move(part));
        }
    };
    append(remaining_, x_, y_ + fall_offset_);
    if (age_ > FF6_ROM_VALUE_MONSTER_DISSOLVE_TIMELINE_0)
        append(falling_, 0x10000 - FF6_ROM_VALUE_MONSTER_DISSOLVE_ORIGIN_0,
               0x10000 - FF6_ROM_VALUE_MONSTER_DISSOLVE_ORIGIN_0 + fall_offset_);
}
} // namespace ff6
