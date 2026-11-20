#pragma once
#include "ff6/battle_raster_timeline.hpp"
#include <optional>

namespace ff6 {
// Explicit NTSC/noninterlaced picture epoch. Absolute clocks preserve the
// eight-clock refresh/transfer phase when the alternating short line occurs.
// This is a timing profile for native publication work, not a device interface.
struct BattleDisplayEpoch {
    BattleRasterTick start = 0;
    bool short_picture = false;
};
BattleRasterTick battle_display_duration(BattleDisplayEpoch epoch);
BattleRasterTick battle_display_line(BattleDisplayEpoch epoch, unsigned line);
std::vector<BattleRasterStall> battle_refresh_pauses(BattleDisplayEpoch epoch);
// Advance useful work through an independently generated pause calendar. A
// completion on a refresh boundary is observed after that refresh finishes.
BattleRasterTick battle_elapsed_work(BattleRasterTick start, BattleRasterTick work,
                                    const std::vector<BattleRasterStall>& pauses);
struct BattleTransferTiming {
    BattleRasterTick pause = 0, first = 0, last = 0, resume = 0;
    BattleRasterTick refresh = 0;
};
// FFVI's single-owner general publication profile: FastROM next fetch, six-
// clock continuation, row consumption disabled throughout the transfer. The
// first/last payload endpoints are diagnostic profile conventions; hardware
// research establishes the total work more firmly than individual byte phases.
BattleTransferTiming battle_publication_timing(BattleRasterTick request,
    unsigned bytes, const std::vector<BattleRasterStall>& refresh);
enum class BattleScrollCadence { fixed, wave32, wave64, full, chunky8, train, dream };
struct BattleRowPublication {
    BattleScrollCadence foreground = BattleScrollCadence::fixed;
    // Callers select any authored wave; an unconfigured request has no wave.
    BattleScrollCadence battlefield = BattleScrollCadence::fixed;
    BattleScrollCadence overlay = BattleScrollCadence::fixed;
    bool mask = true;
};
struct BattleRowWork {
    // Aggregate work after initial alignment, including all eight publication
    // owners and their row-boundary bookkeeping. No table/register execution.
    std::uint32_t clocks = 0;
    std::uint16_t payload_bytes = 0, renewed_groups = 0;
    // Source-read completion offsets from that aligned start. No CPU writer
    // can progress during this publication; byte phases are a reference profile.
    std::uint32_t color_sample = 0;
    std::optional<std::uint32_t> left_sample, right_sample;
    // Foreground, menu/background and text coordinates, in publication order.
    // Absent means the previously sampled coordinate remains in effect.
    std::array<std::optional<std::array<std::uint32_t, 4>>, 3> scroll_samples{};
};
BattleRowWork battle_row_work(BattleRowPublication request, unsigned row);
// Admission is explicit: the surrounding display/task workload must supply it.
// This derives sample positions and a whole row-consumption stall without
// treating observed per-attack timestamps as production inputs.
struct BattleRowTiming {
    BattleRasterStall stall;
    BattleRasterTick color = 0;
    std::optional<BattleRasterTick> left, right;
};
BattleRowTiming battle_row_timing(BattleRowPublication request, unsigned row,
                                 BattleRasterTick admission, unsigned continuation_clocks);
enum class BattleFieldWave { none, desert, tentacles, dream };
struct BattleScrollPublicationRequest {
    BattleScrollCadence foreground = BattleScrollCadence::fixed;
    BattleFieldWave battlefield = BattleFieldWave::none;
    bool mask_copy = false, ignore_battlefield_origin = false;
    // The original word-sized mask gate also reads the first character's
    // alternate-player menu cue selector. Either flag requests a copy.
    bool first_character_alternate_cue = false;
};
struct BattleScrollPublicationWork {
    std::uint32_t cycles = 0, fast_rom_clocks = 0;
};
// Complete shared scroll/mask preparation, entry to its final return marker.
// Includes the otherwise invisible 152nd copied mask row and page-crossing work.
BattleScrollPublicationWork battle_scroll_publication_work(BattleScrollPublicationRequest request);
// Compose independently derived display intervals, row consumption and refresh
// into the plan already accepted by the live animation caller. Overlapping
// pauses are unioned: an interrupt containing a transfer is never counted twice.
BattleRasterPicturePlan battle_display_picture_plan(BattleDisplayEpoch epoch,
    const std::array<BattleRowTiming, battle_effect_rows>& rows,
    const std::vector<BattleRasterStall>& display_work);
} // namespace ff6
