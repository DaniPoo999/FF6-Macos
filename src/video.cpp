#include "ff6/video.hpp"
#include <limits>

namespace ff6 {
FrameBuffer::FrameBuffer(int w, int h, std::uint32_t clear) : width(w), height(h) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
        throw std::runtime_error("Invalid framebuffer dimensions");
    rgba.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), clear);
}
void FrameBuffer::pixel(int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    rgba[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
         static_cast<std::size_t>(x)] = color;
}
void draw_tile(FrameBuffer& output, const TileDraw& draw) {
    // Early rejection also makes subsequent small coordinate additions safe.
    if (draw.x < -8 || draw.y < -8 || draw.x >= output.width || draw.y >= output.height)
        return;
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = 0; x < 8; ++x) {
            const auto index =
                draw.tile[(draw.flip_y ? 7U - y : y) * 8U + (draw.flip_x ? 7U - x : x)];
            if (draw.transparent_zero && index == 0)
                continue;
            if (index >= draw.palette.size())
                throw std::runtime_error("Tile palette index outside resource");
            output.pixel(draw.x + static_cast<int>(x), draw.y + static_cast<int>(y),
                         rgba8(decode_color(draw.palette[index])));
        }
}
void draw_glyph(FrameBuffer& output, const Glyph& glyph, int x, int y, std::uint32_t color) {
    if (x < -16 || y < -11 || x >= output.width || y >= output.height)
        return;
    for (unsigned row = 0; row < 11; ++row)
        for (unsigned column = 0; column < 16; ++column)
            if ((glyph[row] & (1U << (15U - column))) != 0)
                output.pixel(x + static_cast<int>(column), y + static_cast<int>(row), color);
}
} // namespace ff6
