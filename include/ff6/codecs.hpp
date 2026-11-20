#pragma once
#include "ff6/bytes.hpp"
#include <array>

namespace ff6 {
Bytes decompress_lzss(ByteView source, std::size_t maximum = 65536);
using Tile = std::array<std::uint8_t, 64>;
std::vector<Tile> decode_tiles(ByteView source, unsigned bits_per_pixel);
Bytes encode_tiles(std::span<const Tile> tiles, unsigned bits_per_pixel);
using PortraitOrder = std::array<std::uint8_t, 25>;
PortraitOrder decode_portrait_order(ByteView source);
using Glyph = std::array<std::uint16_t, 11>;
std::vector<Glyph> decode_glyphs(ByteView source);
struct Color {
    std::uint8_t red, green, blue;
    bool high_bit;
};
Color decode_color(std::uint16_t value);
std::uint32_t rgba8(Color color);
struct BrrBlock {
    std::uint8_t shift, filter;
    bool loop, end;
    std::array<std::int8_t, 16> residuals;
};
std::vector<BrrBlock> decode_brr(ByteView bytes);
Bytes encode_brr(std::span<const BrrBlock> blocks);
// A structural inventory entry, not a PCM-decoding/playback guarantee.
struct Sample {
    std::uint32_t index;
    Bytes brr;
};
std::vector<Sample> split_instrument_samples(ByteView data, ByteView far_pointers);
struct FormationSlot {
    std::uint16_t monster_id;
    std::uint8_t x, y;
    bool present, background_layer;
};
struct Formation {
    std::uint8_t layout_id, extension_high_bits;
    std::array<FormationSlot, 6> slots;
};
Formation decode_formation(ByteView record);
Bytes encode_formation(const Formation& formation);
} // namespace ff6
