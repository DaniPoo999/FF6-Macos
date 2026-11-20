#include "ff6/battle_effects.hpp"
#include "rom-materials/battle_entrance_reveal_rows.macros.inc"
#include "rom-materials/battle_curved_overlay_center_radius.macros.inc"
#include "rom-materials/battle_curved_overlay_row_bounds.macros.inc"
#include "rom-materials/battle_tornado_radius_limit.macros.inc"
#include "rom-materials/battle_tornado_twist_amplitude.macros.inc"
#include "rom-materials/battle_tornado_priority_distance.macros.inc"
#include "rom-materials/battle_tornado_priority_base.macros.inc"
#include <stdexcept>

namespace ff6 {
BattleAnimationPoint BattleAttackAnimation::background_origin(unsigned layer) const {
    if (layer != 0 && layer != 2)
        throw std::invalid_argument("Effect image origin needs foreground or overlay");
    const unsigned image = layer == 0 ? 1 : 2;
    if (!images_[image])
        throw std::invalid_argument("Scrolling needs an owned background image");
    const auto geometry = images_[image]->geometry;
    const auto position = background_positions_[image - 1];
    const unsigned x =
        background_flipped_[image - 1] ? 256U - geometry.width * 8U : geometry.width * 8U;
    const auto anchor = background_anchors_[image - 1].value_or(BattleAnimationPoint{
        static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(unsigned{geometry.height} * 8U)});
    return {wrap16(unsigned{anchor.x} - position.x), wrap16(unsigned{anchor.y} - position.y)};
}
void BattleAttackAnimation::init_scroll_wave(std::uint8_t selection, std::uint8_t axes) {
    for (unsigned layer = 0; layer < 3; ++layer)
        if (selection & (128U >> layer))
            for (unsigned axis = 0; axis < 2; ++axis)
                if (axes & (64U << axis)) {
                    auto& wave = scrolling_[layer].axes[axis];
                    wave.amplitude = axes & 63U;
                    wave.speed = selection & 31U;
                    // AC changes parameters only: phase and published samples
                    // survive until AE explicitly recalculates the curve.
                }
}
void BattleAttackAnimation::update_scroll_wave(std::uint8_t selection) {
    for (unsigned layer = 0; layer < 3; ++layer)
        if (selection & (4U >> layer)) {
            for (unsigned axis = 0; axis < 2; ++axis)
                if (selection & (64U << axis)) {
                    auto& wave = scrolling_[layer].axes[axis];
                    for (unsigned row = 0; row < 32; ++row) {
                        // _c10d7d leaves A=64 after setting its byte extent;
                        // _0d91 uses that as the initial phase (a cosine crest).
                        // It samples every eight phases with amplitudes
                        // 0,2,...14 and an unconditional -2.
                        // An amplitude of zero consequently is not a flat zero.
                        const auto phase = wrap8(((unsigned{wave.phase} + row) & 31U) * 8U + 64U);
                        const int value =
                            scale_battle_effect_sine(*library_, phase, wave.amplitude & 14U) - 2;
                        wave.offsets[row] = wrap16(static_cast<std::uint32_t>(value));
                        if (layer == 2) {
                            // Overlay stores absolute samples when AE runs;
                            // later image movement must not move this curve.
                            const auto origin = background_origin(2);
                            auto& p = scrolling_[2].projection[row];
                            auto& coordinate = axis == 0 ? p.x : p.y;
                            coordinate = wrap16(unsigned{axis == 0 ? origin.x : origin.y} +
                                                wave.offsets[row]);
                            scrolling_[2].projection_owned = true;
                        }
                    }
                    wave.byte_displacement = false;
                    wave.phase = wrap8(unsigned{wave.phase} + wave.speed);
                }
        }
}
void BattleAttackAnimation::select_scroll(std::uint8_t selection) {
    const unsigned layer = selection >> 6U;
    const unsigned mode = selection & 63U;
    if (layer >= 3)
        throw std::invalid_argument("Invalid effect scroll layer");
    if ((layer == 0 && mode == 3) || (layer == 2 && mode == 5))
        scrolling_[layer].pattern = BattleScrollPattern::fixed;
    else if ((layer == 0 && mode == 6) || (layer == 1 && mode == 4) || (layer == 2 && mode == 7))
        scrolling_[layer].pattern = BattleScrollPattern::wave32;
    else if (layer == 0 && mode == 8)
        scrolling_[layer].pattern = BattleScrollPattern::wave64;
    else if ((layer == 0 && mode == 0) || (layer == 2 && mode == 2))
        scrolling_[layer].pattern = BattleScrollPattern::full;
    else
        throw std::invalid_argument("Unimplemented effect scroll pattern " + std::to_string(mode));
    context_.scroll_programs[layer] = static_cast<std::uint8_t>(mode);
}
void BattleAttackAnimation::update_entrance_scroll(std::uint8_t operation) {
    if (operation == 0) {
        entrance_lines_ = FF6_ROM_VALUE_BATTLE_ENTRANCE_REVEAL_ROWS_0;
        return;
    }
    if (!entrance_lines_ || *entrance_lines_ == 0)
        throw std::invalid_argument("Entrance scroll needs initialization and at most 128 updates");
    const auto origin = background_origin(0);
    auto& projection = scrolling_[0].projection;
    scrolling_[0].projection_owned = true;
    for (unsigned row = 0; row < projection.size(); ++row) {
        projection[row].x = origin.x;
        projection[row].y =
            wrap16(unsigned{origin.y} + (row < *entrance_lines_ ? *entrance_lines_ - row : 0U));
    }
    --*entrance_lines_;
}
void BattleAttackAnimation::update_unfolding_scroll(std::uint8_t operation) {
    // C1/CD72: retain the origin for N rows, then decrease the vertical
    // displacement each row. The lower image thus samples a single source row.
    if (!operation) { unfolding_lines_ = 1; return; }
    if (!unfolding_lines_ || *unfolding_lines_ >= 151)
        throw std::invalid_argument("Unfolding scroll needs initialization and at most 150 updates");
    const auto origin = background_origin(0);
    auto& scroll = scrolling_[0];
    for (unsigned row = 0; row < scroll.projection.size(); ++row)
        scroll.projection[row] = {origin.x, wrap16(unsigned{origin.y} -
            (row > *unfolding_lines_ ? row - *unfolding_lines_ : 0U))};
    scroll.projection_owned = true;
    ++*unfolding_lines_;
}
void BattleAttackAnimation::initialize_curved_overlay(bool horizontal) {
    auto& scroll = scrolling_[2];
    if (horizontal) {
        // The curve comes from the left edge of the shared circular outline.
        // Preserve the selected shape, but retain the new circle coordinates
        // and publication just as the command does for later mask users.
        const auto shape = mask_.shape;
        mask_.radius = mask_.y = FF6_ROM_VALUE_BATTLE_CURVED_OVERLAY_CENTER_RADIUS_1;
        mask_.x = FF6_ROM_VALUE_BATTLE_CURVED_OVERLAY_CENTER_RADIUS_0;
        mask_.shape = 0;
        update_mask();
        mask_.shape = shape;
        for (unsigned row = 0; row < scroll.projection.size(); ++row)
            scroll.projection[row].x = mask_publication_rows_[row].left;
    } else {
        if (!library_->curved_scroll_repeats)
            throw std::invalid_argument("Curved overlay requires its repeat table");
        const auto& repeats = *library_->curved_scroll_repeats;
        auto position = background_origin(2).y;
        unsigned row = 0, group = 0;
        for (const unsigned end : {FF6_ROM_VALUE_BATTLE_CURVED_OVERLAY_ROW_BOUNDS_0 / 4U, FF6_ROM_VALUE_BATTLE_CURVED_OVERLAY_ROW_BOUNDS_1 / 4U}) {
            while (row < end) {
                if (group >= repeats.size() || repeats[group] == 0)
                    throw std::invalid_argument("Curved overlay repeat leaves its table");
                const unsigned count = std::min<unsigned>(repeats[group], end - row);
                for (unsigned i = 0; i < count; ++i) {
                    scroll.projection[row++].y = position;
                    position = wrap16(unsigned{position} - 1U);
                }
                // Skip one source row between groups, including the central
                // split. The second half starts with the same group index.
                position = wrap16(unsigned{position} - 1U);
                if (row < end)
                    group = end == FF6_ROM_VALUE_BATTLE_CURVED_OVERLAY_ROW_BOUNDS_0 / 4U ? group + 1 : group - 1;
            }
        }
    }
    scroll.projection_owned = true;
}
void BattleAttackAnimation::advance_curved_overlay() {
    auto& scroll = scrolling_[2];
    if (!scroll.projection_owned)
        throw std::invalid_argument("Curved overlay requires an initialized projection");
    for (auto& row : scroll.projection)
        row.y = wrap16(unsigned{row.y} - 1U);
}
void BattleAttackAnimation::reset_tornado() {
    tornado_ = {};
    tornado_.initialized = true;
    // The original advances graphics RNG, then discards the value. Its orbit
    // starts at the ROM-selected horizontal phase and vertical phase zero.
    random();
    for (auto& axis : scrolling_[0].axes)
        axis.offsets.fill(0);
}
void BattleAttackAnimation::move_tornado(BattleEffectTask& task, std::uint8_t horizontal,
                                         std::uint8_t vertical) {
    if (!tornado_.initialized)
        throw std::invalid_argument("Tornado movement requires initialization");
    auto& motion = tornado_;
    const auto translate = [](std::uint16_t origin, int offset) {
        return wrap16(static_cast<std::uint32_t>(int{origin} + offset));
    };
    // Sample the old radius/phase before growing the ellipse. Position and the
    // independent 64-row twisting curve retain separate phases.
    task.script.offset = {
        translate(motion.origin.x,
                  scale_battle_effect_sine(*library_, motion.horizontal_phase, motion.radius)),
        translate(motion.origin.y,
                  scale_battle_effect_sine(*library_, motion.vertical_phase, motion.radius / 2))};
    motion.horizontal_phase = wrap8(unsigned{motion.horizontal_phase} + horizontal);
    motion.vertical_phase = wrap8(unsigned{motion.vertical_phase} + vertical);
    motion.radius = static_cast<std::uint8_t>(std::min(unsigned{motion.radius} + 1U, static_cast<unsigned>(FF6_ROM_VALUE_BATTLE_TORNADO_RADIUS_LIMIT_0)));
    for (unsigned row = 0; row < 64; ++row) {
        const auto phase = wrap8((unsigned{motion.row_phase} + row) * 4U);
        scrolling_[0].axes[0].offsets[row] = translate(
            0, scale_battle_effect_sine(*library_, phase, FF6_ROM_VALUE_BATTLE_TORNADO_TWIST_AMPLITUDE_0) - 2);
        scrolling_[0].axes[1].offsets[row] = 0;
    }
    motion.row_phase = wrap8(unsigned{motion.row_phase} + 2U);
}
void BattleAttackAnimation::prioritize_tornado(const BattleEffectTask& task) {
    // Actors below the moving funnel remain in front. Equality belongs behind
    // it; compare complete wrapped coordinates, not truncated screen bytes.
    const auto bottom = wrap16(unsigned{task.script.position.y} + task.script.offset.y + static_cast<unsigned>(FF6_ROM_VALUE_BATTLE_TORNADO_PRIORITY_DISTANCE_0));
    for (unsigned actor = 0; actor < 10; ++actor) {
        const std::uint8_t priority = bottom >= context_.anchors[actor].bottom_y ? FF6_ROM_VALUE_BATTLE_TORNADO_PRIORITY_BASE_0 : (FF6_ROM_VALUE_BATTLE_TORNADO_PRIORITY_BASE_0 + 1);
        if (actor < 4)
            context_.characters[actor].appearance.priority = priority;
        else
            context_.monster_styles[actor - 4].priority = priority;
    }
}
} // namespace ff6
