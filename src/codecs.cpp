#include "ff6/codecs.hpp"
#include <algorithm>

namespace ff6 {
Bytes decompress_lzss(ByteView source, std::size_t maximum) {
    if (source.size() < 3 || source.size() > 65535 || read16(source, 0) != source.size())
        throw std::runtime_error("LZSS length prefix mismatch");
    std::array<std::uint8_t, 2048> history{};
    std::size_t write = 2014, position = 2;
    Bytes output;
    const auto emit = [&](std::uint8_t value) {
        if (output.size() >= maximum)
            throw std::runtime_error("LZSS decoded size exceeds limit");
        output.push_back(value);
        history[write] = value;
        write = (write + 1) & 2047U;
    };
    while (position < source.size()) {
        const auto flags = source[position++];
        if (position == source.size())
            throw std::runtime_error("LZSS flag without token");
        for (unsigned bit = 0; bit < 8 && position < source.size(); ++bit) {
            if ((flags & (1U << bit)) != 0)
                emit(source[position++]);
            else {
                const auto reference = read16(source, position);
                position += 2;
                const auto offset = reference & 2047U, count = (reference >> 11U) + 3U;
                // Read after each write: references may overlap newly decoded data.
                for (unsigned i = 0; i < count; ++i)
                    emit(history[(offset + i) & 2047U]);
            }
        }
    }
    return output;
}
std::vector<Tile> decode_tiles(ByteView source, unsigned bpp) {
    if (bpp != 2 && bpp != 4 && bpp != 8)
        throw std::runtime_error("Unsupported planar tile depth");
    const std::size_t width = 8U * bpp;
    if (source.size() % width != 0)
        throw std::runtime_error("Partial planar tile");
    std::vector<Tile> tiles(source.size() / width);
    for (std::size_t t = 0; t < tiles.size(); ++t)
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x) {
                unsigned value = 0;
                for (unsigned plane = 0; plane < bpp; ++plane) {
                    const auto byte =
                        source[t * width + (plane / 2U) * 16U + y * 2U + (plane & 1U)];
                    value |= ((byte >> (7U - x)) & 1U) << plane;
                }
                tiles[t][y * 8U + x] = static_cast<std::uint8_t>(value);
            }
    return tiles;
}
Bytes encode_tiles(std::span<const Tile> tiles, unsigned bpp) {
    if (bpp != 2 && bpp != 4 && bpp != 8)
        throw std::runtime_error("Unsupported planar tile depth");
    if (tiles.size() > 65536)
        throw std::runtime_error("Tile inventory too large");
    const std::size_t width = 8U * bpp;
    Bytes output(tiles.size() * width, 0);
    for (std::size_t t = 0; t < tiles.size(); ++t)
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x) {
                const auto value = tiles[t][y * 8U + x];
                if (value >= (1U << bpp))
                    throw std::runtime_error("Pixel index exceeds tile depth");
                for (unsigned plane = 0; plane < bpp; ++plane) {
                    auto& byte = output[t * width + (plane / 2U) * 16U + y * 2U + (plane & 1U)];
                    byte = static_cast<std::uint8_t>(byte | (((value >> plane) & 1U) << (7U - x)));
                }
            }
    return output;
}
std::vector<Glyph> decode_glyphs(ByteView source) {
    if (source.size() % 22 != 0)
        throw std::runtime_error("Partial eleven-row font glyph");
    std::vector<Glyph> output(source.size() / 22);
    for (std::size_t i = 0; i < output.size(); ++i)
        for (std::size_t row = 0; row < 11; ++row)
            output[i][row] = read16(source, i * 22 + row * 2);
    return output;
}
Color decode_color(std::uint16_t value) {
    return {static_cast<std::uint8_t>(value & 31U), static_cast<std::uint8_t>((value >> 5U) & 31U),
            static_cast<std::uint8_t>((value >> 10U) & 31U), (value & 0x8000U) != 0};
}
std::uint32_t rgba8(Color c) {
    const auto expand = [](std::uint8_t v) { return (std::uint32_t{v} << 3U) | (v >> 2U); };
    return (expand(c.red) << 24U) | (expand(c.green) << 16U) | (expand(c.blue) << 8U) | 255U;
}
std::vector<BrrBlock> decode_brr(ByteView bytes) {
    if (bytes.empty() || bytes.size() % 9 != 0)
        throw std::runtime_error("BRR requires complete nonempty blocks");
    std::vector<BrrBlock> blocks;
    for (std::size_t p = 0; p < bytes.size(); p += 9) {
        const auto header = bytes[p];
        BrrBlock block{static_cast<std::uint8_t>(header >> 4U),
                       static_cast<std::uint8_t>((header >> 2U) & 3U),
                       (header & 2U) != 0,
                       (header & 1U) != 0,
                       {}};
        if (block.end != (p + 9 == bytes.size()))
            throw std::runtime_error("BRR end flag must occur only in final block");
        for (std::size_t i = 0; i < 16; ++i) {
            const auto raw = (bytes[p + 1 + i / 2] >> ((i % 2 == 0) ? 4U : 0U)) & 15U;
            block.residuals[i] = static_cast<std::int8_t>(raw < 8 ? static_cast<int>(raw)
                                                                  : static_cast<int>(raw) - 16);
        }
        blocks.push_back(block);
    }
    return blocks;
}
Bytes encode_brr(std::span<const BrrBlock> blocks) {
    if (blocks.empty() || blocks.size() > 65535 / 9)
        throw std::runtime_error("Invalid BRR block count");
    Bytes bytes;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        if (b.shift > 15 || b.filter > 3 || b.end != (i + 1 == blocks.size()))
            throw std::runtime_error("Invalid BRR header");
        bytes.push_back(static_cast<std::uint8_t>((std::uint32_t{b.shift} << 4U) |
                                                  (std::uint32_t{b.filter} << 2U) |
                                                  (b.loop ? 2U : 0U) | (b.end ? 1U : 0U)));
        for (std::size_t n = 0; n < 16; n += 2) {
            const auto nib = [](std::int8_t v) {
                if (v < -8 || v > 7)
                    throw std::runtime_error("BRR residual outside signed nibble");
                return static_cast<unsigned>(v < 0 ? int{v} + 16 : int{v});
            };
            bytes.push_back(
                static_cast<std::uint8_t>((nib(b.residuals[n]) << 4U) | nib(b.residuals[n + 1])));
        }
    }
    return bytes;
}
std::vector<Sample> split_instrument_samples(ByteView data, ByteView far_pointers) {
    if (far_pointers.size() != 63 * 3)
        throw std::runtime_error("Expected 63 instrument pointers");
    const auto base = read24(far_pointers, 0);
    std::vector<Sample> result;
    for (std::uint32_t i = 0; i < 63; ++i) {
        const auto pointer = read24(far_pointers, i * 3);
        const std::size_t end =
            i == 62 ? data.size() : std::size_t{read24(far_pointers, (i + 1) * 3) - base};
        if (pointer < base)
            throw std::runtime_error("Instrument pointer below resource base");
        const std::size_t begin = pointer - base;
        if (end <= begin || end > data.size())
            throw std::runtime_error("Invalid instrument span");
        const auto stored = slice(data, begin, end - begin);
        const auto length = read16(stored, 0);
        if (std::size_t{length} + 2 != stored.size())
            throw std::runtime_error("Instrument length prefix mismatch");
        const auto raw = stored.subspan(2);
        (void)decode_brr(raw);
        result.push_back({i, Bytes(raw.begin(), raw.end())});
    }
    return result;
}
PortraitOrder decode_portrait_order(ByteView source) {
    PortraitOrder order{};
    if (source.size() != order.size())
        throw std::runtime_error("Portrait order requires exactly 25 tile indices");
    std::array<bool, 25> seen{};
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto index = source[i];
        if (index >= order.size() || seen[index])
            throw std::runtime_error("Portrait order must be a permutation of its tiles");
        seen[index] = true;
        order[i] = index;
    }
    return order;
}
Formation decode_formation(ByteView record) {
    if (record.size() != 15)
        throw std::runtime_error("Formation requires exactly 15 stored bytes");
    Formation value{static_cast<std::uint8_t>(record[0] >> 4U),
                    static_cast<std::uint8_t>(record[14] >> 6U),
                    {}};
    const auto background_mask = ((record[0] & 15U) << 2U) | (record[1] >> 6U);
    for (unsigned i = 0; i < 6; ++i) {
        value.slots[i] = {
            static_cast<std::uint16_t>(record[2 + i] | (((record[14] >> i) & 1U) << 8U)),
            static_cast<std::uint8_t>(record[8 + i] >> 4U),
            static_cast<std::uint8_t>(record[8 + i] & 15U), (record[1] & (1U << i)) != 0,
            (background_mask & (1U << i)) != 0};
    }
    return value;
}
Bytes encode_formation(const Formation& value) {
    if (value.layout_id > 12 || value.extension_high_bits > 3)
        throw std::runtime_error("Invalid formation layout/flags");
    Bytes record(15, 0);
    record[0] = static_cast<std::uint8_t>(value.layout_id << 4U);
    record[14] = static_cast<std::uint8_t>(value.extension_high_bits << 6U);
    unsigned background_mask = 0;
    for (unsigned i = 0; i < 6; ++i) {
        const auto& slot = value.slots[i];
        if ((slot.monster_id >= 384 && slot.monster_id != 511) || slot.x > 15 || slot.y > 15)
            throw std::runtime_error("Invalid formation slot");
        record[2 + i] = static_cast<std::uint8_t>(slot.monster_id & 255U);
        record[8 + i] = static_cast<std::uint8_t>((slot.x << 4U) | slot.y);
        record[14] = static_cast<std::uint8_t>(record[14] | (((slot.monster_id >> 8U) & 1U) << i));
        if (slot.present)
            record[1] = static_cast<std::uint8_t>(record[1] | (1U << i));
        if (slot.background_layer)
            background_mask |= 1U << i;
    }
    record[0] = static_cast<std::uint8_t>(record[0] | (background_mask >> 2U));
    record[1] = static_cast<std::uint8_t>(record[1] | ((background_mask & 3U) << 6U));
    return record;
}
} // namespace ff6
