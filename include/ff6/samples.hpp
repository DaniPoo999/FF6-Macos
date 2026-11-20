#pragma once
#include "ff6/audio.hpp"
#include "ff6/content.hpp"
#include <memory>
#include <optional>

namespace ff6 {
// Immutable storage and active-loop identity, independent of voices or clocks.
class BrrSample {
    std::vector<BrrBlock> blocks_;
    std::uint16_t stored_loop_offset_;
    std::optional<std::size_t> loop_block_;

  public:
    BrrSample(ByteView encoded, std::uint16_t loop_offset);
    std::span<const BrrBlock> blocks() const {
        return blocks_;
    }
    std::uint16_t stored_loop_offset() const {
        return stored_loop_offset_;
    }
    std::optional<std::size_t> loop_block() const {
        return loop_block_;
    }
};

struct SampleEnvelope {
    std::uint8_t attack_rate, decay_rate, sustain_level, sustain_rate;
};
struct SampleAsset {
    std::shared_ptr<const BrrSample> sample;
    // Encoded driver coefficients; interpretation belongs to native CalcFreq.
    std::array<std::uint8_t, 2> pitch_adjustment;
    SampleEnvelope envelope;
};
struct SampleBank {
    std::array<SampleAsset, 63> instruments;
    std::array<SampleAsset, 8> effects;
    std::array<std::array<std::optional<std::uint8_t>, 16>, 85> song_instruments;
};
SampleBank load_sample_bank(const ContentPack& pack);
// Stable identity in the extracted bank; independent of temporary song slots.
enum class SampleKind : std::uint8_t { instrument, effect };
struct SampleIdentity {
    SampleKind kind = SampleKind::instrument;
    std::uint8_t index = 0;
    bool operator==(const SampleIdentity&) const = default;
};
// An unassigned song slot is explicit absence, never a fallback sample.
const SampleAsset& sample_asset(const SampleBank& bank, SampleIdentity identity);
std::optional<SampleIdentity>
resolve_sample_slot(const SampleBank& bank, std::optional<std::uint8_t> song, std::uint8_t slot);

// Decoded storage sequence, NOT an audible voice. A non-looping terminal block
// is decoded in full here; voice-level silence/lookahead is a separate contract.
// Zero history describes an isolated traversal, not the original key-on rule.
// Initial history must contain even decoded values (the format restores bit 0).
class BrrStream {
    std::shared_ptr<const BrrSample> sample_;
    BrrHistory history_;
    std::array<std::int16_t, 16> pending_{};
    std::size_t next_block_ = 0, pending_offset_ = 16;
    std::size_t pending_block_ = 0;

  public:
    explicit BrrStream(std::shared_ptr<const BrrSample> sample, BrrHistory initial = {});
    // No allocation. Leaves the unwritten suffix untouched at a finite end.
    std::size_t read(std::span<std::int16_t> output);
    bool finished() const;
    // Block containing the next unread decoded value; absent at a finite end.
    std::optional<std::size_t> next_source_block() const;
};
} // namespace ff6
