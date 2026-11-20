#include "ff6/battle_display_clock.hpp"
#include "rom-materials/battle_row_full_length.macros.inc"
#include "rom-materials/battle_row_mode_bounds.macros.inc"
#include "rom-materials/battle_row_selection_bounds.macros.inc"
#include "rom-materials/battle_row_mask_length.macros.inc"
#include "rom-materials/battle_row_fixed_length.macros.inc"
#include "rom-materials/battle_row_wave_length.macros.inc"
#include "rom-materials/battle_row_chunk_length.macros.inc"
#include "rom-materials/battle_row_train_lengths.macros.inc"
#include "rom-materials/battle_row_dream_first_length.macros.inc"
#include "rom-materials/battle_row_dream_second_length.macros.inc"
#include "rom-materials/battle_row_special_bounds.macros.inc"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
BattleRasterTick add(BattleRasterTick left, BattleRasterTick right) {
    require(right <= std::numeric_limits<BattleRasterTick>::max() - left,
            "Display clock overflow");
    return left + right;
}
}
BattleRasterTick battle_display_duration(BattleDisplayEpoch epoch) {
    return 262U * 1364U - (epoch.short_picture ? 4U : 0U);
}
BattleRasterTick battle_display_line(BattleDisplayEpoch epoch, unsigned line) {
    require(line <= 262, "Display line outside NTSC picture");
    return add(epoch.start, BattleRasterTick{line} * 1364U -
               (epoch.short_picture && line > 240 ? 4U : 0U));
}
std::vector<BattleRasterStall> battle_refresh_pauses(BattleDisplayEpoch epoch) {
    std::vector<BattleRasterStall> result;
    result.reserve(262);
    for (unsigned line = 0; line < 262; ++line) {
        const auto begin = battle_display_line(epoch, line);
        // Hardware measurements place the pause on an eight-clock phase near
        // horizontal clock 536: 538 on the first reset-aligned line, then the
        // same global phase on subsequent lines. Do not restart it each frame.
        const auto refresh = add(begin, 538U - (begin & 7U));
        result.push_back({refresh, add(refresh, 40), true});
    }
    return result;
}
BattleRasterTick battle_elapsed_work(BattleRasterTick start, BattleRasterTick work,
                                    const std::vector<BattleRasterStall>& pauses) {
    BattleRasterTick previous = 0;
    for (const auto pause : pauses) {
        require(pause.begin < pause.end && pause.begin >= previous,
                "Unordered display pauses");
        previous = pause.end;
    }
    auto finish = add(start, work);
    for (const auto pause : pauses) {
        if (pause.end <= start) continue;
        if (pause.begin > finish || (pause.begin == finish && !pause.defer_boundary_store)) break;
        // A caller may enter while a pause is already in progress. Only the
        // unelapsed portion is added, never the portion preceding its entry.
        finish = add(finish, pause.end - std::max(start, pause.begin));
    }
    return finish;
}
BattleTransferTiming battle_publication_timing(BattleRasterTick request,
    unsigned bytes, const std::vector<BattleRasterStall>& refresh) {
    require(bytes > 0 && bytes <= 2048, "Unsupported battle publication extent");
    BattleTransferTiming result;
    result.pause = battle_elapsed_work(request, 6, refresh);
    const auto alignment = 8U - (result.pause & 7U);
    const auto work = alignment + 16U + BattleRasterTick{bytes} * 8U;
    result.first = battle_elapsed_work(result.pause, alignment + 24U, refresh);
    result.last = battle_elapsed_work(result.pause, work, refresh);
    // Refresh suspends the continuation clock as well as useful transfer work.
    // Its forty clocks therefore do not advance the six-clock return phase.
    const auto return_work = 6U - work % 6U;
    result.resume = battle_elapsed_work(result.last, return_work, refresh);
    result.refresh = result.resume - request - 6U - work - return_work;
    return result;
}
BattleRowWork battle_row_work(BattleRowPublication request, unsigned row) {
    require(row < 224, "Row publication outside canonical picture");
    // These row counts are selected display data. Decode them from the ROM;
    // the timing arithmetic below remains a native, bounded publication profile.
    constexpr unsigned full_run = FF6_ROM_VALUE_BATTLE_ROW_FULL_LENGTH_0;
    constexpr unsigned region_split = FF6_ROM_VALUE_BATTLE_ROW_MODE_BOUNDS_0;
    constexpr unsigned menu_begin = FF6_ROM_VALUE_BATTLE_ROW_MODE_BOUNDS_1;
    constexpr unsigned selection_first = FF6_ROM_VALUE_BATTLE_ROW_SELECTION_BOUNDS_0;
    constexpr unsigned selection_last = FF6_ROM_VALUE_BATTLE_ROW_SELECTION_BOUNDS_1;
    constexpr unsigned mask_run = FF6_ROM_VALUE_BATTLE_ROW_MASK_LENGTH_0;
    // Raw run-count sums include one repeat flag per record. Remove the format
    // bits after summing; authenticate both layouts wherever lowering shares a bound.
    constexpr unsigned special_begin = FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_0 - 2U * 128U;
    constexpr unsigned special_tail = FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_2 - 4U * 128U;
    constexpr unsigned train_middle = FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_4 - 3U * 128U;
    constexpr unsigned dream_middle = FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_5 - 3U * 128U;
    static_assert(FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_0 == FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_1);
    static_assert(FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_2 == FF6_ROM_VALUE_BATTLE_ROW_SPECIAL_BOUNDS_3);
    // Source-defined scroll runs. Only run boundaries and repetition affect
    // the publication workload; source addresses and values are irrelevant.
    const auto scroll = [row](BattleScrollCadence cadence) {
        require(static_cast<unsigned>(cadence) <= static_cast<unsigned>(BattleScrollCadence::dream),
                "Unknown battle scroll cadence");
        if (cadence == BattleScrollCadence::full)
            return std::pair{true, row == full_run - 1U || row == 223};
        if (row >= menu_begin) return std::pair{true, row == 223};
        unsigned length = 0, position = row;
        bool repeated = true;
        switch (cadence) {
        case BattleScrollCadence::fixed: repeated = false; [[fallthrough]];
        case BattleScrollCadence::wave64: length = FF6_ROM_VALUE_BATTLE_ROW_FIXED_LENGTH_0; break;
        case BattleScrollCadence::wave32: length = FF6_ROM_VALUE_BATTLE_ROW_WAVE_LENGTH_0; break;
        case BattleScrollCadence::full: length = full_run; break;
        case BattleScrollCadence::chunky8: length = FF6_ROM_VALUE_BATTLE_ROW_CHUNK_LENGTH_0; repeated = false; break;
        case BattleScrollCadence::train:
            if (row < special_begin) length = FF6_ROM_VALUE_BATTLE_ROW_WAVE_LENGTH_0;
            else if (row < train_middle) { length = FF6_ROM_VALUE_BATTLE_ROW_TRAIN_LENGTHS_0; position -= special_begin; }
            else if (row < special_tail) { length = FF6_ROM_VALUE_BATTLE_ROW_TRAIN_LENGTHS_1; position -= train_middle; }
            else { length = FF6_ROM_VALUE_BATTLE_ROW_TRAIN_LENGTHS_0; position -= special_tail; }
            break;
        case BattleScrollCadence::dream:
            if (row < special_begin) length = FF6_ROM_VALUE_BATTLE_ROW_WAVE_LENGTH_0;
            else if (row < dream_middle) { length = FF6_ROM_VALUE_BATTLE_ROW_DREAM_FIRST_LENGTH_0; position -= special_begin; }
            else if (row < special_tail) { length = FF6_ROM_VALUE_BATTLE_ROW_DREAM_SECOND_LENGTH_0; position -= dream_middle; }
            else { length = FF6_ROM_VALUE_BATTLE_ROW_TRAIN_LENGTHS_0; position -= special_tail; }
            break;
        default: throw std::invalid_argument("Unknown battle scroll cadence");
        }
        return std::pair{repeated || position % length == 0,
                         row == menu_begin - 1U || (position + 1) % length == 0};
    };
    const auto fg = scroll(request.foreground), bg = scroll(request.battlefield), ov = scroll(request.overlay);
    const bool region = row == 0 || row == region_split || row == menu_begin;
    const bool selection = row == 0 || row == selection_first || row == region_split || row == menu_begin || row == selection_last;
    const unsigned before_color = 4U * (unsigned(fg.first) + unsigned(bg.first) + unsigned(ov.first) +
                                        unsigned(region));
    const unsigned payload = before_color + 4U + (request.mask ? 4U : 2U) +
                             4U * unsigned(region) + 4U * unsigned(selection);
    // Each owner renews after its current row data; extracted boundaries
    // preserve their independently selected bands and their coincident endpoints.
    const unsigned renewals = unsigned(fg.second) + unsigned(bg.second) + unsigned(ov.second) +
        2U * unsigned(row == region_split - 1U || row == menu_begin - 1U || row == 223) + unsigned(row == full_run - 1U || row == 223) +
        unsigned(request.mask ? row == mask_run - 1U || row == 2U * mask_run - 1U || row == 223 : row == full_run - 1U || row == 223) +
        unsigned(row == selection_first - 1U || row == region_split - 1U || row == menu_begin - 1U || row == selection_last - 1U || row == 223);
    BattleRowWork result;
    result.payload_bytes = static_cast<std::uint16_t>(payload);
    result.renewed_groups = static_cast<std::uint16_t>(renewals);
    // At the final row all owners end. The final owner's terminal bookkeeping
    // has one fewer eight-clock access in this explicitly bounded profile.
    result.clocks = 8U + payload * 8U + 8U * 8U + renewals * 16U - (row == 223 ? 8U : 0U);
    unsigned position = 12;
    unsigned plane = 0;
    for (const bool sampled : {fg.first, bg.first, ov.first}) {
        if (sampled) {
            result.scroll_samples[plane] = std::array<std::uint32_t, 4>{position, position + 8,
                                                                      position + 16, position + 24};
            position += 32;
        }
        ++plane;
    }
    result.color_sample = 8U + before_color * 8U + 28U;
    if (request.mask) {
        result.left_sample = result.color_sample + 24U;
        result.right_sample = result.color_sample + 32U;
    }
    return result;
}
BattleRowTiming battle_row_timing(BattleRowPublication request, unsigned row,
                                 BattleRasterTick admission, unsigned continuation_clocks) {
    require(continuation_clocks == 6 || continuation_clocks == 8,
            "Unsupported native publication continuation profile");
    const auto work = battle_row_work(request, row);
    const auto alignment = 8U - (admission & 7U);
    const auto aligned = add(admission, alignment);
    const auto elapsed = alignment + work.clocks;
    BattleRowTiming result;
    result.stall = {admission, add(admission, elapsed + continuation_clocks - elapsed % continuation_clocks)};
    result.color = add(aligned, work.color_sample);
    if (work.left_sample) result.left = add(aligned, *work.left_sample);
    if (work.right_sample) result.right = add(aligned, *work.right_sample);
    return result;
}
BattleRasterPicturePlan battle_display_picture_plan(BattleDisplayEpoch epoch,
    const std::array<BattleRowTiming, battle_effect_rows>& rows,
    const std::vector<BattleRasterStall>& display_work) {
    BattleRasterPicturePlan result;
    result.duration = battle_display_duration(epoch);
    const auto end = add(epoch.start, result.duration);
    auto pauses = battle_refresh_pauses(epoch);
    pauses.insert(pauses.end(), display_work.begin(), display_work.end());
    for (unsigned i = 0; i < battle_effect_rows; ++i) {
        const auto& row = rows[i];
        require(row.left && row.right && row.stall.begin >= battle_display_line(epoch, i) &&
                    row.stall.begin < battle_display_line(epoch, i + 1) &&
                    row.color >= row.stall.begin && row.color < *row.left &&
                    *row.left < *row.right && *row.right < row.stall.end && row.stall.end <= end,
                "Invalid or incomplete battle row sample schedule");
        result.color_samples[i] = row.color - epoch.start;
        result.left_samples[i] = *row.left - epoch.start;
        result.right_samples[i] = *row.right - epoch.start;
        require(i == 0 || result.color_samples[i] >= result.color_samples[i - 1],
                "Backwards row samples");
        pauses.push_back(row.stall);
    }
    for (const auto pause : pauses)
        require(pause.begin < pause.end, "Empty or backwards display interval");
    std::sort(pauses.begin(), pauses.end(), [](auto a, auto b) { return a.begin < b.begin; });
    for (const auto pause : pauses) {
        if (pause.end <= epoch.start || pause.begin >= end) continue;
        const BattleRasterStall local{std::max(pause.begin, epoch.start) - epoch.start,
                                     std::min(pause.end, end) - epoch.start, pause.defer_boundary_store};
        if (!result.stalls.empty() && local.begin <= result.stalls.back().end) {
            if (local.begin == result.stalls.back().begin)
                result.stalls.back().defer_boundary_store |= local.defer_boundary_store;
            result.stalls.back().end = std::max(result.stalls.back().end, local.end);
        } else result.stalls.push_back(local);
    }
    return result;
}
} // namespace ff6
