#pragma once
#include "ff6/battle_display_clock.hpp"

namespace ff6 {
struct BattleScrollPoint {
    std::uint16_t x = 0, y = 0;
    bool operator==(const BattleScrollPoint&) const = default;
};
// The admitted display owner excludes concurrent animation writers. These are
// its stable source values, not a snapshot of an emulated address space.
struct BattleScrollPublicationInput {
    BattleScrollPoint foreground_origin, battlefield_origin;
    bool borrow_battlefield_origin = false;
    std::array<BattleScrollPoint, 64> foreground_offsets{};
    std::array<BattleScrollPoint, 32> battlefield_offsets{};
    std::array<std::uint16_t, 32> small_wave{}, large_wave{};
    std::uint8_t wave_phase = 0; // Logical sample, 0..31.
    // The original copy includes one row beyond the visible 151-row battlefield.
    std::array<BattleMaskSpan, 152> mask{};
};
enum class BattleScrollStoreKind { mask_release, mask_left, mask_right,
                                   foreground_x, foreground_y, battlefield_x, battlefield_y,
                                   first_character_cue_release };
struct BattleScrollPublicationStore {
    BattleScrollStoreKind kind;
    std::uint16_t row = 0, value = 0;
    bool high_byte = false;
    BattleScrollPublicationWork work; // Cumulative, at completion of this store.
};
struct BattleScrollPublicationTrace {
    std::vector<BattleScrollPublicationStore> stores;
    BattleScrollPublicationWork finish; // Final return marker, excluding RTS.
};
BattleScrollPublicationTrace battle_scroll_publication_trace(
    BattleScrollPublicationRequest request, const BattleScrollPublicationInput& input);
struct BattlePublishedScroll {
    std::array<BattleScrollPoint, 64> foreground{};
    std::array<BattleScrollPoint, 32> battlefield{};
    std::array<BattleMaskSpan, 152> mask{};
    bool mask_pending = false;
    bool first_character_alternate_cue = false;
};
// Retains progress through row publication/refresh and across picture epochs.
// A fixed calendar and actual display entry are inputs; neither is guessed.
class BattleScrollPreparation {
public:
    void begin(BattleScrollPublicationRequest request, const BattleScrollPublicationInput& input,
               BattleRasterTick start, const std::vector<BattleRasterStall>& pauses);
    bool advance(BattlePublishedScroll& state, BattleRasterTick until);
    bool pending() const { return active_; }
    BattleRasterTick finish() const { return finish_; }
private:
    BattleScrollPublicationTrace trace_;
    std::vector<BattleRasterTick> commits_;
    BattleRasterTick cursor_ = 0, finish_ = 0;
    std::size_t next_ = 0;
    bool active_ = false;
};
// Connect the same source-derived stores to the existing raster consumer. The
// invisible row still consumes work; foreground/battlefield work forms the tail.
struct BattleScrollMaskWriter {
    std::vector<BattleRasterWriterStep> steps;
    BattleRasterTick tail_work = 0;
};
BattleScrollMaskWriter battle_scroll_mask_writer(const BattleScrollPublicationTrace& trace);
} // namespace ff6
