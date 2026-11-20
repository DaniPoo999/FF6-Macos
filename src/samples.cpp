#include "ff6/samples.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
SampleEnvelope envelope(ByteView data) {
    require(data.size() == 2 && (data[0] & 0x80U) != 0, "Expected sample ADSR envelope");
    return {static_cast<std::uint8_t>(data[0] & 15U),
            static_cast<std::uint8_t>((data[0] >> 4U) & 7U),
            static_cast<std::uint8_t>(data[1] >> 5U), static_cast<std::uint8_t>(data[1] & 31U)};
}
SampleAsset asset(ByteView data, std::uint16_t loop, ByteView pitch, ByteView adsr) {
    require(pitch.size() == 2, "Expected two sample pitch-adjustment bytes");
    return {std::make_shared<const BrrSample>(data, loop), {pitch[0], pitch[1]}, envelope(adsr)};
}
// Transfer prefixes describe these three tables, not individual effect samples.
ByteView table(ByteView data, std::size_t offset, std::size_t length) {
    require(read16(data, offset) == length, "Invalid effect property table length");
    return slice(data, offset + 2, length);
}
} // namespace

BrrSample::BrrSample(ByteView encoded, std::uint16_t loop_offset)
    : stored_loop_offset_(loop_offset) {
    require(encoded.size() <= 65535, "Sample exceeds its stored length field");
    blocks_ = decode_brr(encoded);
    if (blocks_.back().loop) {
        require(loop_offset % 9 == 0 && loop_offset < encoded.size(), "Invalid active sample loop");
        loop_block_ = loop_offset / 9;
    }
    // Inactive offsets can point past the sample; preserve them without reads.
}

SampleBank load_sample_bank(const ContentPack& pack) {
    SampleBank bank{};
    const auto properties = pack.get("data_c53d1c_sample_properties_and_song_tables");
    require(properties.size() == 3353, "Invalid instrument property extent");
    const auto raw =
        split_instrument_samples(pack.get("brr_sample_brr_bin"), pack.get("brr_tables_pointers"));
    for (std::size_t i = 0; i < bank.instruments.size(); ++i)
        bank.instruments[i] =
            asset(raw[i].brr, read16(properties, i * 2), slice(properties, 126 + i * 2, 2),
                  slice(properties, 252 + i * 2, 2));
    // Three 63-word property tables and 85 far song-script pointers precede
    // the 85 x 16 word instrument lists. Script pointers have another consumer.
    constexpr std::size_t song_lists = 3 * 63 * 2 + 85 * 3;
    for (std::size_t song = 0; song < bank.song_instruments.size(); ++song)
        for (std::size_t slot = 0; slot < 16; ++slot) {
            const auto id = read16(properties, song_lists + (song * 16 + slot) * 2);
            require(id <= bank.instruments.size(), "Song references an unknown instrument");
            if (id != 0)
                bank.song_instruments[song][slot] = static_cast<std::uint8_t>(id - 1);
        }

    const auto effects = pack.get("brr_sfx_brr_bin");
    const auto header = pack.get("brr_tables_sfx_header");
    require(header.size() == 2 && read16(header, 0) == effects.size(),
            "Invalid effect sample transfer length");
    const auto effect_properties = pack.get("data_c52016_sfx_scripts_and_properties");
    require(effect_properties.size() == 7224, "Invalid effect property extent");
    const auto directory = table(effect_properties, 0, 32);
    const auto adsr = table(effect_properties, 34, 16);
    const auto pitch = table(effect_properties, 52, 16);
    const auto base = read16(directory, 0);
    for (std::size_t i = 0; i < bank.effects.size(); ++i) {
        const std::size_t start = read16(directory, i * 4);
        const std::size_t end = i + 1 == bank.effects.size() ? std::size_t{base} + effects.size()
                                                             : read16(directory, (i + 1) * 4);
        const auto loop = read16(directory, i * 4 + 2);
        require(start >= base && end > start && end - base <= effects.size() && end <= 65536 &&
                    loop >= start,
                "Invalid effect sample directory");
        const auto data = slice(effects, start - base, end - start);
        bank.effects[i] = asset(data, static_cast<std::uint16_t>(loop - start),
                                slice(pitch, i * 2, 2), slice(adsr, i * 2, 2));
    }
    return bank;
}

BrrStream::BrrStream(std::shared_ptr<const BrrSample> sample, BrrHistory initial)
    : sample_(std::move(sample)), history_(initial) {
    require(sample_ != nullptr, "Missing BRR sample");
    require(initial.previous % 2 == 0 && initial.older % 2 == 0,
            "BRR history must contain decoded even samples");
}
bool BrrStream::finished() const {
    return pending_offset_ == pending_.size() && next_block_ == sample_->blocks().size();
}
std::optional<std::size_t> BrrStream::next_source_block() const {
    if (finished())
        return std::nullopt;
    return pending_offset_ == pending_.size() ? next_block_ : pending_block_;
}
std::size_t BrrStream::read(std::span<std::int16_t> output) {
    std::size_t written = 0;
    while (written < output.size() && !finished()) {
        if (pending_offset_ == pending_.size()) {
            pending_block_ = next_block_;
            pending_ = decode_brr_block(sample_->blocks()[next_block_], history_);
            pending_offset_ = 0;
            ++next_block_;
            if (next_block_ == sample_->blocks().size() && sample_->loop_block())
                next_block_ = *sample_->loop_block();
        }
        const auto count = std::min(output.size() - written, pending_.size() - pending_offset_);
        std::copy_n(pending_.begin() + static_cast<std::ptrdiff_t>(pending_offset_), count,
                    output.begin() + static_cast<std::ptrdiff_t>(written));
        written += count;
        pending_offset_ += count;
    }
    return written;
}
const SampleAsset& sample_asset(const SampleBank& bank, SampleIdentity identity) {
    if (identity.kind == SampleKind::instrument && identity.index < bank.instruments.size())
        return bank.instruments[identity.index];
    if (identity.kind == SampleKind::effect && identity.index < bank.effects.size())
        return bank.effects[identity.index];
    throw std::invalid_argument("Unknown sample resource identity");
}
std::optional<SampleIdentity>
resolve_sample_slot(const SampleBank& bank, std::optional<std::uint8_t> song, std::uint8_t slot) {
    if (slot < 8)
        return SampleIdentity{SampleKind::effect, slot};
    if (slot < 32 || slot >= 48 || !song || *song >= bank.song_instruments.size())
        throw std::invalid_argument("Unresolved sample selection slot");
    const auto instrument = bank.song_instruments[*song][slot - 32U];
    if (!instrument)
        return std::nullopt;
    if (*instrument >= bank.instruments.size())
        throw std::invalid_argument("Unassigned or invalid song instrument");
    return SampleIdentity{SampleKind::instrument, *instrument};
}
} // namespace ff6
