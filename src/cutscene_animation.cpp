#include "ff6/cutscene_animation.hpp"

namespace ff6 {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::invalid_argument(why); }
}
std::vector<CutsceneSprite> decode_cutscene_sprite_group(ByteView data, std::size_t& cursor,
                                        std::span<const Tile> tiles) {
    const unsigned count = slice(data, cursor++, 1)[0];
    require(count <= 128, "Invalid cutscene sprite group");
    std::vector<CutsceneSprite> result;
    for (unsigned i = 0; i < count; ++i) {
        const auto descriptor = slice(data, cursor, 4);
        cursor += 4;
        CutsceneSprite sprite;
        auto& image = sprite.image;
        image.x = descriptor[0] & 127U;
        image.y = descriptor[1];
        image.width = image.height = (descriptor[0] & 128U) ? 16 : 8;
        image.flip_x = (descriptor[3] & 64U) != 0;
        image.flip_y = (descriptor[3] & 128U) != 0;
        sprite.palette = (descriptor[3] >> 1U) & 7U;
        constexpr std::array<std::uint16_t, 4> depths{2, 5, 8, 12};
        image.depth = depths[(descriptor[3] >> 4U) & 3U];
        const unsigned first = descriptor[2] + (descriptor[3] & 1U) * 256U;
        for (unsigned y = 0; y < image.height; ++y)
            for (unsigned x = 0; x < image.width; ++x) {
                const auto tile = (first & ~15U) + ((first + x / 8) & 15U) + (y / 8) * 16;
                require(tile < tiles.size(), "Cutscene sprite exceeds prepared graphics");
                image.pixels.push_back(tiles[tile][(y % 8) * 8 + x % 8]);
            }
        result.push_back(std::move(sprite));
    }
    return result;
}

void advance_cutscene_animation(CutsceneAnimationCursor& cursor,
                                std::span<const CutsceneAnimationFrame> frames) {
    auto next = cursor;
    for (std::size_t redirects = 0; redirects <= frames.size(); ++redirects) {
        if (next.frame >= frames.size())
            throw std::invalid_argument("Cutscene animation exceeded its data");
        if (next.wait == 254) {
            cursor = next;
            return;
        }
        if (next.wait == 255) {
            next.frame = 0;
            next.wait = frames[0].duration;
        } else if (next.wait == 0) {
            if (++next.frame == frames.size())
                throw std::invalid_argument("Unterminated cutscene animation");
            next.wait = frames[next.frame].duration;
        } else {
            --next.wait;
            cursor = next;
            return;
        }
    }
    throw std::invalid_argument("Cutscene animation has no timed frame");
}
} // namespace ff6
