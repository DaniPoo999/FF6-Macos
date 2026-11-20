#pragma once
#include "ff6/raster.hpp"

namespace ff6 {
struct CutsceneSprite {
    RasterSprite image;
    std::uint8_t palette = 0;
};
std::vector<CutsceneSprite> decode_cutscene_sprite_group(ByteView data, std::size_t& cursor,
                                                       std::span<const Tile> tiles);
struct CutsceneAnimationFrame {
    std::size_t resource = 0;
    std::uint8_t duration = 0;
};
struct CutsceneAnimationCursor {
    std::size_t frame = 0;
    std::uint8_t wait = 0;
};
// Advance the game's data language: zero advances, 254 holds, 255 loops.
// A positive ordinary duration is decremented before the selected frame draws.
// Reject a malformed stream that cannot reach a timed or held frame.
void advance_cutscene_animation(CutsceneAnimationCursor& cursor,
                                std::span<const CutsceneAnimationFrame> frames);
} // namespace ff6
