#pragma once
#include "ff6/title.hpp"
#include "rom-materials/ending_flight_shared_fade_periods.macros.inc"
#include "ff6/cutscene_animation.hpp"

namespace ff6 {
// Resolved tile rectangles from the ending's data language. Original source
// pointers are checked at load time; playback owns ordinary layers and cells.
struct EndingTilePatch {
    unsigned column = 0, row = 0, columns = 0, rows = 0;
    std::vector<RasterCell> cells;
};
EndingTilePatch decode_ending_tile_patch(ByteView graphics, unsigned bytes, unsigned rows,
                                       unsigned source, unsigned destination);
void apply_ending_tile_patch(TileLayer& layer, const EndingTilePatch& patch);
struct EndingBookAssets {
    TileLayer illustration, shading;
    std::array<CutscenePalette, 8> colors{};
    CutscenePalette lettering{};
    std::vector<EndingTilePatch> patches;
    // Opening book, looping closing-credit book, final resting pages.
    std::array<std::vector<CutsceneAnimationFrame>, 3> animations;
    std::vector<CutsceneSprite> closing_text;
};
EndingBookAssets load_ending_book_assets(const ContentPack& pack);
enum class EndingBookSegment { opening, closing };
// BA's other families get their own native owners. Never acknowledge an
// unimplemented segment as a completed book presentation.
bool is_ending_book_segment(std::uint8_t selector);
EndingBookSegment ending_book_segment(std::uint8_t selector);
class EndingBook {
  public:
    EndingBook(EndingBookAssets assets, EndingBookSegment segment);
    bool advance();
    bool complete() const;
    unsigned updates() const { return updates_; }
    RasterScene scene() const;
  private:
    EndingBookAssets assets_;
    EndingBookSegment segment_;
    std::array<CutscenePalette, 8> colors_{};
    std::array<CutscenePaletteFade, 8> fades_{};
    CutscenePalette text_colors_{};
    CutscenePaletteFade text_fade_{FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0};
    CutsceneAnimationCursor cursor_{};
    unsigned animation_ = 0, updates_ = 0;
    bool animating_ = false;
    void start_animation(unsigned identity);
    void apply_patch(std::size_t identity);
};
} // namespace ff6
