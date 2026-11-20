#pragma once
#include "ff6/battle_effect_raster.hpp"
#include <vector>
#include <limits>

namespace ff6 {
// All work, stalls and sampling points in a plan must use the same caller-owned
// integer clock. CPU cycle counts alone are not elapsed master clocks. There is
// deliberately no inferred/default SNES timing profile.
using BattleRasterTick = std::uint64_t;
struct BattleRasterStall {
    BattleRasterTick begin = 0, end = 0; // Half-open; already includes its setup.
    // Refresh can suspend the access that would complete at this boundary.
    // Interrupt/row admission instead follows the already completed access.
    bool defer_boundary_store = false;
};
struct BattleRasterPicturePlan {
    BattleRasterTick duration = 0;
    std::array<BattleRasterTick, battle_effect_rows> color_samples{}, left_samples{}, right_samples{};
    std::vector<BattleRasterStall> stalls;
};
enum class BattleRasterWriteKind { gradient, mask_left, mask_right };
struct BattleRasterWriterStep {
    // Work since the preceding store, excluding stalls and picture boundaries.
    BattleRasterTick work = 0;
    BattleRasterWriteKind kind = BattleRasterWriteKind::gradient;
    std::uint16_t row = 0;
    BattleGradientColor color{};
    std::uint16_t edge = 0;
};
struct BattleRasterWriterProgress {
    BattleRasterTick clock = 0;
    std::size_t writes = 0;
    bool complete = false, picture_complete = false;
};
struct BattleGradientRowWork {
    std::uint16_t row = 0;
    std::uint32_t cycles = 0;
    // Reviewed FastROM source/operand profile, excluding all external stalls.
    std::uint32_t fast_rom_clocks = 0;
};
struct BattleGradientSourceWork {
    std::vector<BattleGradientRowWork> rows;
    std::uint32_t finish = 0;
    std::uint32_t fast_rom_finish = 0;
};
// Reviewed narrow-blue/rainbow algorithm work, measured in source CPU cycles.
// The optional FastROM accounting distinguishes slow working/stack data from
// fast program/parameter reads. It excludes refresh, publication, row sampling
// and synchronization stalls; it is not a complete elapsed caller schedule.
BattleGradientSourceWork battle_gradient_source_work(bool narrow, std::uint8_t phase,
    std::uint8_t amount, const std::array<std::uint8_t, 32>& ramp);
// A retained row writer, separate from animation command decoding. Beginning a
// new picture resumes remaining work; it cannot rerun script/RNG/sound owners.
// Production callers must supply a complete, independently justified plan.
class BattleRasterTimeline {
  public:
    void reset(const BattleEffectRaster::Colors& colors, const BattleEffectRaster::Mask& mask);
    void begin_picture(BattleRasterPicturePlan plan);
    void begin_writer(std::vector<BattleRasterWriterStep> steps, BattleRasterTick tail_work = 0);
    // Entry already derived by the caller, in elapsed picture clocks. Earlier
    // stalls are already included in that entry and must not be counted twice.
    void begin_writer_at(BattleRasterTick start, std::vector<BattleRasterWriterStep> steps,
                         BattleRasterTick tail_work = 0);
    BattleRasterWriterProgress advance_writer(std::size_t max_writes = std::numeric_limits<std::size_t>::max());
    // Idle work before the next command. Cannot discard an unfinished writer.
    void finish_picture();
    bool writer_pending() const { return pending_; }
    BattleRasterTick clock() const { return clock_; }
    const BattleEffectRaster& raster() const { return raster_; }

  private:
    BattleEffectRaster raster_;
    BattleRasterPicturePlan plan_;
    std::vector<BattleRasterWriterStep> steps_;
    BattleRasterTick clock_ = 0, remaining_ = 0, tail_ = 0;
    std::size_t next_ = 0, stall_ = 0;
    bool picture_ = false, pending_ = false;
};
} // namespace ff6
