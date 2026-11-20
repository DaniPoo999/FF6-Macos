#pragma once
#include "ff6/codecs.hpp"

namespace ff6 {
// Native pixel composition. No video registers, device memories or bus writes.
struct FrameBuffer {
    int width, height;
    std::vector<std::uint32_t> rgba;
    FrameBuffer(int width, int height, std::uint32_t clear);
    void pixel(int x, int y, std::uint32_t color);
};
struct TileDraw {
    const Tile& tile;
    std::span<const std::uint16_t> palette;
    int x, y;
    bool flip_x = false, flip_y = false, transparent_zero = true;
};
void draw_tile(FrameBuffer& output, const TileDraw& draw);
void draw_glyph(FrameBuffer& output, const Glyph& glyph, int x, int y, std::uint32_t color);
} // namespace ff6
