#include "ff6/battle_scroll_publication.hpp"
#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::invalid_argument(why);
}
// Work is grouped by FFVI's copying and coordinate algorithms. This is not an
// instruction executor. Slow source/destination bytes add two master clocks.
class ScrollWork {
public:
    explicit ScrollWork(BattleScrollPublicationTrace* output) : output_(output) {}
    void add(unsigned cycles, unsigned slow = 0) {
        work_.cycles += cycles;
        work_.fast_rom_clocks += cycles * 6U + slow * 2U;
    }
    void store(BattleScrollStoreKind kind, unsigned row, unsigned value, bool high = false) {
        if (output_)
            output_->stores.push_back({kind, static_cast<std::uint16_t>(row),
                static_cast<std::uint16_t>(value), high, work_});
    }
    void word(BattleScrollStoreKind kind, unsigned row, std::uint16_t value, bool indexed) {
        add(indexed ? 5U : 4U, 1);
        store(kind, row, value & 255U);
        add(1, 1);
        store(kind, row, value >> 8U, true);
    }
    BattleScrollPublicationWork finish() const { return work_; }
private:
    BattleScrollPublicationWork work_;
    BattleScrollPublicationTrace* output_;
};
BattleScrollPublicationWork prepare(BattleScrollPublicationRequest request,
    const BattleScrollPublicationInput& input, BattleScrollPublicationTrace* output) {
    require(request.foreground == BattleScrollCadence::fixed || request.foreground == BattleScrollCadence::full ||
                request.foreground == BattleScrollCadence::wave32 || request.foreground == BattleScrollCadence::wave64,
            "Unreviewed foreground scroll publication");
    require(static_cast<unsigned>(request.battlefield) <= static_cast<unsigned>(BattleFieldWave::dream),
            "Unknown battlefield wave publication");
    require(input.wave_phase < 32, "Invalid battlefield wave phase");
    const bool wave32 = request.foreground == BattleScrollCadence::wave32;
    const bool wave64 = request.foreground == BattleScrollCadence::wave64;
    const bool full = request.foreground == BattleScrollCadence::full;
    const bool field_wave = request.battlefield != BattleFieldWave::none;
    ScrollWork work(output);
    if (request.mask_copy || request.first_character_alternate_cue) {
        work.add(27, 6);
        work.store(BattleScrollStoreKind::mask_release, 0, 0);
        work.add(1, 1);
        // The source's word clear also resets the adjacent PlayerSfx selector.
        // Preserve that gameplay quirk as named state, not an aliased RAM byte.
        work.store(BattleScrollStoreKind::first_character_cue_release, 0, 0);
        work.add(4); // Initialize the row cursor after releasing both flags.
        for (unsigned row = 0; row < 38; ++row) {
            for (unsigned group = 0; group < 4; ++group) {
                const auto index = row + group * 38;
                const auto span = input.mask[index];
                require(span.left <= 255 && span.right >= 1 && span.right <= 256,
                        "Mask source needs byte edges and a half-open right edge");
                const bool crossing = (group == 1 && row >= 18) || (group == 3 && row >= 6);
                work.add(5U + crossing, 2); // Read this pair before either edge is published.
                work.add(5, 1);
                work.store(BattleScrollStoreKind::mask_left, index, span.left);
                work.add(1, 1);
                work.store(BattleScrollStoreKind::mask_right, index, span.right);
            }
            work.add(row == 37 ? 12U : 13U);
        }
    } else work.add(24, 5);
    work.add(wave32 ? 29U : wave64 ? 31U : 27U, wave32 ? 5U : wave64 ? 4U : 3U);
    auto foreground = input.foreground_origin;
    if (input.borrow_battlefield_origin) foreground = input.battlefield_origin;
    else foreground.y = wrap16(unsigned{foreground.y} - 1U);
    if (full) work.add(32, 10);
    else {
        work.add(input.borrow_battlefield_origin ? 41U : 42U, 12);
        work.add(9, 4);
        work.word(BattleScrollStoreKind::foreground_x, 0, foreground.x, false);
        work.add(input.borrow_battlefield_origin ? 9U : 11U, 4);
        work.word(BattleScrollStoreKind::foreground_y, 0, foreground.y, false);
        if (input.borrow_battlefield_origin) work.add(3);
    }
    const auto battlefield = request.ignore_battlefield_origin ? BattleScrollPoint{} : input.battlefield_origin;
    work.add(request.ignore_battlefield_origin ? 18U : 11U, request.ignore_battlefield_origin ? 6U : 2U);
    // Four interleaved 16-row groups for a 64-row foreground, two for 32 rows.
    // Each coordinate is a word whose two byte stores can straddle a row read.
    const auto coordinates = [&](bool front, unsigned row, BattleScrollPoint offset) {
        const auto origin = front ? foreground : battlefield;
        const bool crossing = front && row >= 20 && row < 32;
        for (const bool vertical : {false, true}) {
            work.add(11U + crossing, 4);
            const auto value = wrap16(unsigned{vertical ? origin.y : origin.x} +
                                     (vertical ? offset.y : offset.x));
            const auto kind = front ? (vertical ? BattleScrollStoreKind::foreground_y : BattleScrollStoreKind::foreground_x)
                                    : (vertical ? BattleScrollStoreKind::battlefield_y : BattleScrollStoreKind::battlefield_x);
            work.word(kind, row, value, true);
        }
    };
    for (unsigned row = 0; row < 16; ++row) {
        work.add(wave64 ? 6U : wave32 ? 9U : 10U, 2);
        for (unsigned group = 0; group < (wave64 ? 4U : wave32 ? 2U : 0U); ++group) {
            const auto index = row + group * 16;
            coordinates(true, index, input.foreground_offsets[index]);
        }
        if (wave64) work.add(3);
        work.add(field_wave ? 7U : 6U, 2);
        if (!field_wave)
            for (unsigned group = 0; group < 2; ++group) {
                const auto index = row + group * 16;
                coordinates(false, index, input.battlefield_offsets[index]);
            }
        work.add(row == 15 ? 13U : 15U);
    }
    if (!field_wave) work.add(25, 2);
    else {
        const bool large = request.battlefield == BattleFieldWave::dream;
        const bool both = request.battlefield == BattleFieldWave::desert;
        work.add(large ? 33U : both ? 37U : 36U, 3);
        for (unsigned row = 0; row < 32; ++row) {
            const auto phase = (unsigned{input.wave_phase} + row) & 31U;
            work.add(5U + (!large && phase != 0), 2);
            const auto value = large ? input.large_wave[phase] : input.small_wave[phase];
            if (both) work.word(BattleScrollStoreKind::battlefield_x, row, value, true);
            work.word(BattleScrollStoreKind::battlefield_y, row, value, true);
            work.add(row == 31 ? 23U : 24U);
        }
        work.add(both ? 8U : 11U);
    }
    return work.finish();
}
} // namespace
BattleScrollPublicationWork battle_scroll_publication_work(BattleScrollPublicationRequest request) {
    BattleScrollPublicationInput input;
    input.mask.fill({0, 1});
    return prepare(request, input, nullptr);
}
BattleScrollPublicationTrace battle_scroll_publication_trace(
    BattleScrollPublicationRequest request, const BattleScrollPublicationInput& input) {
    BattleScrollPublicationTrace result;
    result.finish = prepare(request, input, &result);
    return result;
}
void BattleScrollPreparation::begin(BattleScrollPublicationRequest request,
    const BattleScrollPublicationInput& input, BattleRasterTick start,
    const std::vector<BattleRasterStall>& pauses) {
    require(!active_, "Cannot replace pending scroll preparation");
    auto trace = battle_scroll_publication_trace(request, input);
    std::vector<BattleRasterTick> commits;
    commits.reserve(trace.stores.size());
    for (const auto& store : trace.stores)
        commits.push_back(battle_elapsed_work(start, store.work.fast_rom_clocks, pauses));
    const auto finish = battle_elapsed_work(start, trace.finish.fast_rom_clocks, pauses);
    trace_ = std::move(trace);
    commits_ = std::move(commits);
    cursor_ = start;
    finish_ = finish;
    next_ = 0;
    active_ = true;
}
bool BattleScrollPreparation::advance(BattlePublishedScroll& state, BattleRasterTick until) {
    require(until >= cursor_, "Backwards scroll preparation deadline");
    while (next_ < commits_.size() && commits_[next_] <= until) {
        const auto& store = trace_.stores[next_++];
        switch (store.kind) {
        case BattleScrollStoreKind::mask_release: state.mask_pending = false; break;
        case BattleScrollStoreKind::first_character_cue_release:
            state.first_character_alternate_cue = false;
            break;
        case BattleScrollStoreKind::mask_left: state.mask[store.row].left = store.value; break;
        case BattleScrollStoreKind::mask_right: state.mask[store.row].right = store.value; break;
        default: {
            const bool foreground = store.kind == BattleScrollStoreKind::foreground_x ||
                                    store.kind == BattleScrollStoreKind::foreground_y;
            const bool vertical = store.kind == BattleScrollStoreKind::foreground_y ||
                                  store.kind == BattleScrollStoreKind::battlefield_y;
            auto& point = foreground ? state.foreground[store.row] : state.battlefield[store.row];
            auto& value = vertical ? point.y : point.x;
            value = store.high_byte ? static_cast<std::uint16_t>((value & 0xffU) | (unsigned{store.value} << 8U))
                                    : static_cast<std::uint16_t>((value & 0xff00U) | store.value);
            break;
        }
        }
    }
    cursor_ = until;
    if (until >= finish_) active_ = false;
    return !active_;
}
BattleScrollMaskWriter battle_scroll_mask_writer(const BattleScrollPublicationTrace& trace) {
    BattleScrollMaskWriter result;
    BattleRasterTick previous = 0;
    for (const auto& store : trace.stores) {
        if (store.row >= battle_effect_rows || (store.kind != BattleScrollStoreKind::mask_left &&
                                                store.kind != BattleScrollStoreKind::mask_right)) continue;
        require(store.work.fast_rom_clocks >= previous, "Backwards scroll mask work");
        result.steps.push_back({store.work.fast_rom_clocks - previous,
            store.kind == BattleScrollStoreKind::mask_left ? BattleRasterWriteKind::mask_left : BattleRasterWriteKind::mask_right,
            store.row, {}, store.value});
        previous = store.work.fast_rom_clocks;
    }
    require(trace.finish.fast_rom_clocks >= previous, "Incomplete scroll mask work");
    result.tail_work = trace.finish.fast_rom_clocks - previous;
    return result;
}
} // namespace ff6
