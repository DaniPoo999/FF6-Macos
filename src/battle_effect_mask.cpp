#include "rom-materials/battle_mask_blob_phase_small.macros.inc"
#include "rom-materials/battle_mask_blob_phase_other.macros.inc"
#include "rom-materials/battle_mask_blob_wave_rates.macros.inc"
#include "rom-materials/battle_mask_blob_amplitude_big.macros.inc"
#include "rom-materials/battle_mask_blob_amplitude_small.macros.inc"
#include "rom-materials/battle_mask_blob_amplitude_slimer.macros.inc"
#include "rom-materials/battle_mask_bio_phase_step.macros.inc"
#include "rom-materials/battle_mask_bio_row_step.macros.inc"
#include "rom-materials/battle_mask_left_move_guard.macros.inc"
#include "rom-materials/battle_mask_left_edge.macros.inc"
#include "rom-materials/battle_mask_right_edge.macros.inc"
#include "rom-materials/battle_mask_aperture_half_rows.macros.inc"
#include "ff6/battle_effects.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include "rom-materials/battle_triangle_flat_phases.macros.inc"
#include "rom-materials/battle_triangle_spatial_phases.macros.inc"

namespace ff6 {
namespace {
struct CircleOutline {
    std::vector<std::uint8_t> octant, rows;
    unsigned contractions = 0, extensions = 0;
};
CircleOutline circle_outline(std::uint8_t radius) {
    CircleOutline result;
    std::uint8_t error = radius, previous = 255, extent = radius, row = 0;
    while (extent >= row) {
        result.octant.push_back(extent);
        const auto delta = wrap8(unsigned{row} * 2U - 1U);
        ++row;
        error = wrap8(unsigned{error} - delta);
        if (error >= previous) {
            --extent;
            error = wrap8(unsigned{error} + unsigned{extent} * 2U);
            ++result.contractions;
        }
        previous = error;
    }
    result.rows = result.octant;
    extent = wrap8(unsigned{result.octant.back()} - 1U);
    for (std::size_t i = result.octant.size() - 1; i > 0; --i) {
        if (result.octant[i] != result.octant[i - 1]) {
            result.rows.push_back(extent);
            ++result.extensions;
        }
        extent = wrap8(unsigned{extent} - 1U);
    }
    return result;
}
}
void BattleAttackAnimation::accrue_update_work(unsigned cycles, unsigned slow_bytes) {
    if (!presentation_timing_ || picture_work_ || raster_timing_)
        return;
    constexpr unsigned line_clocks = 1364, boundary = 225 * line_clocks;
    const unsigned start = std::max(update_work_floor_, unsigned{raster_floor_} * line_clocks);
    auto end = std::uint64_t{start} + 6U * std::uint64_t{cycles} + 2U * slow_bytes;
    // Exclude display interruptions, transfers and sampling. Even this
    // optimistic bound can require a presentation boundary; a shorter bound
    // never proves that the original work actually fitted in one picture.
    update_work_floor_ = static_cast<unsigned>(std::min(end, std::uint64_t{boundary + 1}));
}
bool BattleAttackAnimation::update_crosses_picture(unsigned cycles, unsigned slow_bytes) {
    accrue_update_work(cycles, slow_bytes);
    return presentation_timing_ && !picture_work_ && !raster_timing_ &&
           update_work_floor_ > 225U * 1364U;
}
bool BattleAttackAnimation::mask_crosses_picture() {
    if (!presentation_timing_ || picture_work_ || raster_timing_)
        return false;
    const auto radius = static_cast<std::uint8_t>(mask_.radius & 127U);
    if (!radius)
        return false;
    const unsigned copied = std::min(256U, 279U - unsigned{mask_.y});
    const unsigned padding = mask_.y > 128 ? unsigned{mask_.y} - 128U : 0U;
    unsigned cycles = 32U * 57U - 1U + (copied - 1U) * 35U +
                      (padding ? (padding - 1U) * 20U : 0U);
    unsigned slow = 512U + (copied - 1U) * 4U;
    if (mask_.shape == 0 || mask_.shape == 1) {
        // C2/D925 constructs the first octant; C2/D777/D7AD generate paired
        // edges; D879 completes the outline. Count semantic loop iterations,
        // using the shortest clipping/sine paths and omitting setup/tails.
        // Other shapes retain the clear/projection bound until reviewed.
        const auto outline = circle_outline(radius);
        const auto count = static_cast<unsigned>(outline.octant.size());
        cycles += count * 53U + outline.contractions * 20U +
                  count * (mask_.shape == 1 ? 158U : 60U) +
                  (count - 1U) * 21U + outline.extensions * 49U;
        slow += count * (mask_.shape == 1 ? 30U : 10U) + outline.extensions * 8U;
    }
    return update_crosses_picture(cycles, slow);
}
void BattleAttackAnimation::update_triangle(bool spatial) {
    if (!triangle_.configured)
        throw std::invalid_argument("Triangle mask requires initialized geometry");
    // The spatial variant follows three unequal orbits, including an elliptical
    // third vertex. Both variants use the same integer edge rasterizer.
    constexpr std::array<std::array<unsigned, 2>, 3> flat{{
        {0, FF6_ROM_VALUE_BATTLE_TRIANGLE_FLAT_PHASES_0},
        {FF6_ROM_VALUE_BATTLE_TRIANGLE_FLAT_PHASES_1, FF6_ROM_VALUE_BATTLE_TRIANGLE_FLAT_PHASES_2},
        {FF6_ROM_VALUE_BATTLE_TRIANGLE_FLAT_PHASES_3, FF6_ROM_VALUE_BATTLE_TRIANGLE_FLAT_PHASES_4}
    }};
    constexpr std::array<std::array<unsigned, 2>, 3> tilted{{
        {0, FF6_ROM_VALUE_BATTLE_TRIANGLE_SPATIAL_PHASES_0},
        {FF6_ROM_VALUE_BATTLE_TRIANGLE_SPATIAL_PHASES_1, FF6_ROM_VALUE_BATTLE_TRIANGLE_SPATIAL_PHASES_2},
        {FF6_ROM_VALUE_BATTLE_TRIANGLE_SPATIAL_PHASES_3, FF6_ROM_VALUE_BATTLE_TRIANGLE_SPATIAL_PHASES_4}
    }};
    const auto& offsets = spatial ? tilted : flat;
    const auto phase = spatial ? wrap8(unsigned{triangle_.phase} ^ 255U) : triangle_.phase;
    struct Vertex {
        std::uint8_t x, y;
    };
    std::array<Vertex, 3> vertices{};
    for (unsigned i = 0; i < vertices.size(); ++i) {
        const auto coordinate = [&](unsigned offset) {
            return wrap8(static_cast<unsigned>(128 + scale_battle_effect_sine8(
                *library_, wrap8(unsigned{phase} + offset), triangle_.size)));
        };
        vertices[i] = {coordinate(offsets[i][0]), coordinate(offsets[i][1])};
    }
    // Preserve tie selection before ordering the two remaining vertices by X.
    // A generic polygon sort changes the one-pixel edges of small triangles.
    unsigned first = 2;
    if (vertices[0].y <= vertices[1].y && vertices[0].y <= vertices[2].y)
        first = 0;
    else if (vertices[0].y > vertices[1].y && vertices[1].y < vertices[2].y)
        first = 1;
    const auto top = vertices[first];
    auto left = vertices[(first + 1) % 3], right = vertices[(first + 2) % 3];
    if (left.x > right.x)
        std::swap(left, right);
    std::array<BattleMaskSpan, 256> outline;
    outline.fill({255, 0}); // Encoded inclusive edges, initially closed.
    const auto edge = [&](Vertex from, Vertex to, bool left_edge) {
        const unsigned height = unsigned{to.y} - from.y;
        if (!height)
            return;
        const int direction = to.x < from.x ? -1 : 1;
        const unsigned width = static_cast<unsigned>(std::abs(int{to.x} - from.x));
        auto x = from.x;
        auto remainder = static_cast<std::uint8_t>(width);
        for (unsigned row = from.y; row < to.y; ++row) {
            // Sample after the first horizontal increment. The retained
            // remainder is a byte; widening its accumulation changes steep
            // clipped outlines at large sizes.
            x = wrap8(static_cast<unsigned>(int{x} + direction * int(remainder / height)));
            remainder = wrap8(remainder % height + width);
            const int translated = int{x} + triangle_.x - 128;
            auto value = static_cast<std::uint8_t>(
                translated >= FF6_ROM_VALUE_BATTLE_MASK_LEFT_EDGE_0 + 1 && translated < FF6_ROM_VALUE_BATTLE_MASK_RIGHT_EDGE_0 + 1 ? translated : triangle_.x < 128 ? FF6_ROM_VALUE_BATTLE_MASK_LEFT_EDGE_0 : FF6_ROM_VALUE_BATTLE_MASK_RIGHT_EDGE_0);
            auto& span = outline[row];
            if (left_edge) {
                if (value == span.right)
                    value = wrap8(unsigned{value} + 1U);
                span.left = value;
            } else {
                if (value == span.left)
                    value = wrap8(unsigned{value} - 1U);
                span.right = value;
            }
        }
    };
    edge(top, left, true);
    edge(top, right, false);
    if (left.y > right.y)
        edge(right, left, false);
    else
        edge(left, right, true);

    raster_event(BattleEffectRasterOperation::mask);
    mask_.y = triangle_.y;
    mask_.published = true;
    mask_pending_ = true;
    for (unsigned row = 0; row < mask_.rows.size(); ++row) {
        const int source = int(row) + 128 - triangle_.y;
        auto span = BattleMaskSpan{255, 0};
        if (source >= 0 && source < int(outline.size()))
            span = outline[static_cast<unsigned>(source)];
        else if (source < 0 && mask_.shape == 3)
            span = outline.front();
        // Rendering uses an exclusive right edge; timed publication retains
        // even the closed edge values. A zero-size triangle publishes empty,
        // unlike a zero-radius circle, which leaves the old mask visible.
        ++span.right;
        mask_publication_rows_[row] = span;
        mask_.rows[row] = span.left < span.right ? span : BattleMaskSpan{};
    }
}
void BattleAttackAnimation::select_mask_shape(std::uint8_t shape) {
    if (shape > 8)
        throw std::invalid_argument("Unimplemented animation mask shape " + std::to_string(shape));
    mask_.shape = shape;
}
void BattleAttackAnimation::move_mask(std::uint8_t x, std::uint8_t y, std::uint8_t radius) {
    // A6/A9 use byte wrapping followed by unsigned upper limits. Only X has
    // the special negative-step guard; a mathematical clamp would differ.
    if (x < 128 || mask_.x >= FF6_ROM_VALUE_BATTLE_MASK_LEFT_MOVE_GUARD_0)
        mask_.x = std::min(wrap8(unsigned{mask_.x} + x), mask_.final_x);
    mask_.y = std::min(wrap8(unsigned{mask_.y} + y), mask_.final_y);
    mask_.radius = std::min(wrap8(unsigned{mask_.radius} + radius), mask_.final_radius);
}
void BattleAttackAnimation::update_mask() {
    auto radius = static_cast<std::uint8_t>(mask_.radius & 127U);
    // UpdateCircle clears its scratch outline but returns before publication
    // at zero. The previously displayed mask survives this particular update.
    if (!radius)
        return;
    select_mask_shape(mask_.shape);
    const bool blob = mask_.shape == 2 || mask_.shape == 5 || mask_.shape == 8;
    if (blob) {
        // Blob size pulses before outline construction; its horizontal wave
        // advances separately afterward and traverses even the empty rows.
        radius = wrap8(static_cast<unsigned>(int{radius} +
            scale_battle_effect_sine8(*library_, mask_.phase, radius)));
        mask_.phase = wrap8(unsigned{mask_.phase} + (mask_.shape == 5 ? FF6_ROM_VALUE_BATTLE_MASK_BLOB_PHASE_SMALL_0 : FF6_ROM_VALUE_BATTLE_MASK_BLOB_PHASE_OTHER_0));
    }
    // Keep geometry and its work bound on the same integer outline algorithm.
    // Byte wrapping is observable in the original edges at small radii.
    const auto construction = circle_outline(radius);
    const auto& octant = construction.octant;
    const auto& outline = construction.rows;
    struct OutlineRow {
        std::uint8_t width, center;
    };
    std::vector<OutlineRow> half;
    auto center = mask_.x;
    auto phase = mask_.phase;
    if (mask_.shape == 1) {
        mask_.phase = wrap8(unsigned{mask_.phase} + FF6_ROM_VALUE_BATTLE_MASK_BIO_PHASE_STEP_0);
        phase = mask_.phase;
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
        if (mask_.shape == 6 && i % 2 != 0)
            continue;
        // Bio Blast samples its wave only along the first octant. The outer
        // completion keeps the last center; it does not continue the sine.
        // Centers wrap as bytes before the edges saturate to the battlefield.
        if (mask_.shape == 1 && i < octant.size()) {
            phase = wrap8(unsigned{phase} + FF6_ROM_VALUE_BATTLE_MASK_BIO_ROW_STEP_0);
            center = wrap8(static_cast<std::uint32_t>(
                int{mask_.x} + scale_battle_effect_sine8(*library_, phase, mask_.speed)));
        }
        half.push_back({outline[i], center});
        if (mask_.shape == 4)
            half.push_back({outline[i], center});
    }
    // Vertical ovals can exceed 128 rows per half (Kirin). Their upward
    // outline uses a byte index: outer rows replace earlier inner rows after
    // wrapping. Only the first 128 downward rows reach the published outline.
    // Preserve that asymmetric shape rather than extending an ideal ellipse.
    std::array<std::optional<OutlineRow>, 128> above{}, below{};
    for (std::size_t i = 0; i < half.size(); ++i) {
        above[i & 127U] = half[i];
        // Ultima keeps every upper row but publishes only even outline rows
        // below the center. It is deliberately not a symmetric ellipse.
        const auto lower = mask_.shape == 7 ? i / 2 : i;
        if (lower < below.size() && (mask_.shape != 7 || i % 2 == 0))
            below[lower] = half[i];
    }
    raster_event(BattleEffectRasterOperation::mask);
    mask_.rows.fill({});
    mask_.published = true;
    mask_pending_ = true;
    const auto span = [&](OutlineRow row, unsigned y) -> BattleMaskSpan {
        if (blob) {
            if (!row.width)
                return {0, 1}; // The source skips horizontal translation for an empty width.
            const auto source_row = wrap8(y + 128U - mask_.y);
            const auto wave_phase = wrap8(unsigned{mask_.phase} +
                (unsigned{source_row} + 1U) * (mask_.shape == 5 ? FF6_ROM_VALUE_BATTLE_MASK_BLOB_WAVE_RATES_2 : FF6_ROM_VALUE_BATTLE_MASK_BLOB_WAVE_RATES_0));
            const std::uint8_t amplitude = mask_.shape == 2 ? FF6_ROM_VALUE_BATTLE_MASK_BLOB_AMPLITUDE_BIG_0 : mask_.shape == 5 ? FF6_ROM_VALUE_BATTLE_MASK_BLOB_AMPLITUDE_SMALL_0 : FF6_ROM_VALUE_BATTLE_MASK_BLOB_AMPLITUDE_SLIMER_0;
            row.center = wrap8(static_cast<unsigned>(int{mask_.x} +
                scale_battle_effect_sine8(*library_, wave_phase, amplitude)));
        }
        return {static_cast<std::uint16_t>(std::max(FF6_ROM_VALUE_BATTLE_MASK_LEFT_EDGE_0, int{row.center} - int{row.width})),
                static_cast<std::uint16_t>(std::min(unsigned{FF6_ROM_VALUE_BATTLE_MASK_RIGHT_EDGE_0}, unsigned{row.center} + row.width) + 1U)};
    };
    for (unsigned y = 0; y < mask_.rows.size(); ++y) {
        // Paired middle rows are y-1 and y, as in the original outline.
        const unsigned distance = y < mask_.y ? unsigned{mask_.y} - y - 1 : y - mask_.y;
        if (mask_.shape == 3 && y < mask_.y)
            mask_.rows[y] = span(half.front(), y);
        else if (distance < above.size()) {
            const auto width = (y < mask_.y ? above : below)[distance];
            if (width)
                mask_.rows[y] = span(*width, y);
        }
        auto& edge = mask_.rows[y];
        // Keep real stored edges for timed publication even when the complete
        // span is empty. A new left edge can expose the retained old right edge.
        mask_publication_rows_[y] = edge == BattleMaskSpan{} ? BattleMaskSpan{255, 1} : edge;
        if (edge.left > edge.right)
            edge = {};
    }
}
void BattleAttackAnimation::open_mask(std::uint8_t inset) {
    // 80/59 writes symmetric rows about the boundary between rows 75/76.
    // Zero deliberately means fully open. Values beyond 76 do not describe
    // the bounded loop used by the game and are rejected.
    if (inset > FF6_ROM_VALUE_BATTLE_MASK_APERTURE_HALF_ROWS_0)
        throw std::invalid_argument("Animation mask inset exceeds battlefield height");
    raster_event(BattleEffectRasterOperation::mask);
    mask_.published = true;
    mask_pending_ = true;
    for (unsigned y = 0; y < mask_.rows.size(); ++y) {
        mask_.rows[y] = y >= inset && y <= FF6_ROM_VALUE_BATTLE_MASK_APERTURE_HALF_ROWS_0 * 2U - 1U - inset ? BattleMaskSpan{FF6_ROM_VALUE_BATTLE_MASK_LEFT_EDGE_0, FF6_ROM_VALUE_BATTLE_MASK_RIGHT_EDGE_0 + 1U} : BattleMaskSpan{};
        mask_publication_rows_[y] =
            mask_.rows[y] == BattleMaskSpan{} ? BattleMaskSpan{255, 1} : mask_.rows[y];
    }
}
} // namespace ff6
