#include "ff6/battle_raster_timeline.hpp"
#include <algorithm>
#include <stdexcept>
#include "rom-materials/battle_gradient_geometry.macros.inc"
#include "rom-materials/battle_gradient_rainbow_extent.macros.inc"

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
void validate(const BattleRasterPicturePlan& plan) {
    require(plan.duration != 0, "Empty raster picture duration");
    for (const auto* samples : {&plan.color_samples, &plan.left_samples, &plan.right_samples}) {
        require(std::is_sorted(samples->begin(), samples->end()), "Backwards raster samples");
        require(samples->back() < plan.duration, "Raster sample outside picture");
    }
    BattleRasterTick end = 0;
    for (const auto stall : plan.stalls) {
        require(stall.begin >= end && stall.begin < stall.end && stall.end <= plan.duration,
                "Overlapping or invalid raster stall");
        end = stall.end;
    }
}
BattleRasterDeadline deadline(const std::array<BattleRasterTick, battle_effect_rows>& samples,
                              BattleRasterTick clock) {
    // Stores at the sampling deadline are visible. Different mask edges may
    // have different deadlines even though they belong to the same row.
    return {static_cast<std::uint16_t>(std::lower_bound(samples.begin(), samples.end(), clock) -
                                       samples.begin())};
}
} // namespace
BattleGradientSourceWork battle_gradient_source_work(bool narrow, std::uint8_t phase,
    std::uint8_t amount, const std::array<std::uint8_t, 32>& ramp) {
    for (const auto value : ramp)
        require(value <= 31, "Gradient work requires a five-bit ramp");
    BattleGradientSourceWork result;
    std::uint32_t clock = narrow ? 33U : 18U;
    // In the reviewed FastROM profile, working data and stack bytes add two
    // master clocks over the six-clock program/internal work baseline. Count
    // semantic setup/loop data accesses, not decoded instructions or a bus.
    std::uint32_t slow_bytes = narrow ? 8U : 6U;
    const auto store = [&](unsigned row) {
        result.rows.push_back({static_cast<std::uint16_t>(row), clock,
                               6U * clock + 2U * slow_bytes});
    };
    store(0);
    clock += narrow ? 21U : 45U;
    slow_bytes += 5;
    auto position = unsigned{phase} & 31U;
    // The writer's loop/copy bounds are selected by the same source operands
    // as its visual gradient. Work coefficients below describe the path costs.
    for (unsigned row = 1; row < (narrow ? FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_2
                                       : FF6_ROM_VALUE_BATTLE_GRADIENT_RAINBOW_EXTENT_0); ++row) {
        // Byte-sign clamp and color rollover have independent branch work.
        clock += 16U + (((unsigned{ramp[position]} - amount) & 128U) != 0 ? 1U : 0U);
        slow_bytes += 2; // Intensity and selected components.
        for (unsigned group = 0; group < (narrow ? 4U : 1U); ++group) {
            clock += 5;
            ++slow_bytes;
            store(row + group * FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_3);
        }
        position = (position + 1U) & 31U;
        clock += 25;
        if (!position) {
            clock += narrow ? 5U : 27U;
            slow_bytes += narrow ? 2U : 4U;
        }
    }
    --clock; // The final row exits rather than taking the loop branch.
    if (narrow) {
        clock += 5;
        slow_bytes += 2; // Restore the repeated tail's row cursor.
        for (unsigned row = FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_1 + 1U;
             row < FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_1 +
                       FF6_ROM_VALUE_BATTLE_GRADIENT_GEOMETRY_6; ++row) {
            clock += 10;
            slow_bytes += 2;
            store(row);
            clock += 14;
        }
        clock += 5; // Final untaken branch, then retained phase update.
        slow_bytes += 2;
    } else {
        clock += 27; // Script pointer, phase, color and task restoration.
        slow_bytes += 10;
    }
    result.finish = clock;
    result.fast_rom_finish = 6U * clock + 2U * slow_bytes;
    return result;
}
void BattleRasterTimeline::reset(const BattleEffectRaster::Colors& colors,
                                 const BattleEffectRaster::Mask& mask) {
    BattleEffectRaster next;
    next.reset(colors, mask);
    raster_ = std::move(next);
    steps_.clear();
    plan_ = {};
    clock_ = remaining_ = tail_ = 0;
    next_ = stall_ = 0;
    picture_ = pending_ = false;
}
void BattleRasterTimeline::begin_picture(BattleRasterPicturePlan plan) {
    validate(plan);
    require(!picture_ || clock_ == plan_.duration, "Previous raster picture is unfinished");
    plan_ = std::move(plan);
    clock_ = 0;
    stall_ = 0;
    picture_ = true;
    raster_.begin_picture();
}
void BattleRasterTimeline::begin_writer(std::vector<BattleRasterWriterStep> steps,
                                       BattleRasterTick tail_work) {
    require(picture_ && clock_ < plan_.duration, "Writer needs an active raster picture");
    require(!pending_, "Cannot replace an unfinished raster writer");
    require(!steps.empty(), "Empty raster writer");
    for (const auto& step : steps) {
        require(step.row < battle_effect_rows, "Raster writer row outside battlefield");
        require(static_cast<unsigned>(step.kind) <= static_cast<unsigned>(BattleRasterWriteKind::mask_right),
                "Unknown raster writer operation");
        if (step.kind == BattleRasterWriteKind::gradient)
            require(step.color.components <= 7 && step.color.value <= 31, "Invalid gradient writer value");
        else
            require(step.edge <= 256, "Invalid mask writer edge");
    }
    steps_ = std::move(steps);
    next_ = 0;
    remaining_ = steps_[0].work;
    tail_ = tail_work;
    pending_ = true;
}
void BattleRasterTimeline::begin_writer_at(BattleRasterTick start,
                                           std::vector<BattleRasterWriterStep> steps,
                                           BattleRasterTick tail_work) {
    require(picture_ && start >= clock_ && start < plan_.duration,
            "Writer entry must belong to the active picture");
    for (const auto stall : plan_.stalls)
        require(start < stall.begin || start >= stall.end,
                "Writer entry cannot lie inside display work");
    begin_writer(std::move(steps), tail_work);
    clock_ = start;
}
BattleRasterWriterProgress BattleRasterTimeline::advance_writer(std::size_t max_writes) {
    require(picture_ && pending_, "No retained raster writer to advance");
    require(max_writes != 0, "Raster writer needs a nonzero progress budget");
    std::size_t writes = 0;
    while (clock_ < plan_.duration && pending_ && writes < max_writes) {
        while (stall_ < plan_.stalls.size() && plan_.stalls[stall_].end <= clock_)
            ++stall_;
        if (stall_ < plan_.stalls.size() && plan_.stalls[stall_].begin <= clock_) {
            clock_ = plan_.stalls[stall_++].end;
            continue;
        }
        const auto boundary = stall_ < plan_.stalls.size() ? plan_.stalls[stall_].begin : plan_.duration;
        const auto work = std::min(remaining_, boundary - clock_);
        remaining_ -= work;
        clock_ += work;
        // A store exactly at a picture boundary belongs to the following
        // picture. An already completed store at a stall start precedes that
        // stall; the next operation will wait through it.
        if (remaining_ || clock_ == plan_.duration)
            continue;
        if (stall_ < plan_.stalls.size() && clock_ == plan_.stalls[stall_].begin &&
            plan_.stalls[stall_].defer_boundary_store)
            continue;
        if (next_ == steps_.size()) {
            pending_ = false;
            break;
        }
        const auto& step = steps_[next_++];
        if (step.kind == BattleRasterWriteKind::gradient)
            raster_.write_gradient({step.row, step.color}, deadline(plan_.color_samples, clock_));
        else {
            const bool right = step.kind == BattleRasterWriteKind::mask_right;
            raster_.write_mask_edge(step.row, right, step.edge,
                                   deadline(right ? plan_.right_samples : plan_.left_samples, clock_));
        }
        ++writes;
        remaining_ = next_ < steps_.size() ? steps_[next_].work : tail_;
        if (next_ == steps_.size() && tail_ == 0)
            pending_ = false;
    }
    return {clock_, writes, !pending_, clock_ == plan_.duration};
}
void BattleRasterTimeline::finish_picture() {
    require(picture_ && !pending_, "Cannot idle past an unfinished raster writer");
    clock_ = plan_.duration;
}
} // namespace ff6
