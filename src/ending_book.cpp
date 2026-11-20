#include "ff6/ending_book.hpp"
#include "rom-materials/ending_book_background_fade_periods.macros.inc"
#include "rom-materials/ending_book_phase_waits.macros.inc"
#include "rom-materials/ending_large_text_waits.macros.inc"
#include "rom-materials/ending_large_text_vertical_origin.macros.inc"
#include "rom-materials/ending_book_text_horizontal_origin.macros.inc"
#include "rom-materials/ending_large_text_vertical_speed.macros.inc"
#include "rom-materials/ending_book_scroll_duration.macros.inc"
#include "rom-materials/ending_book_scroll_mask.macros.inc"
#include "rom-materials/ending_book_mosaic_period.macros.inc"
#include "rom-materials/ending_book_palette_routes.macros.inc"
#include "rom-materials/ending_book_window_edges.macros.inc"
#include "rom-materials/ending_character_composite_modes.macros.inc"
#include "rom-materials/ending_character_operand_layers.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
// Extracted ending arithmetic/source flags choose typed native composition.
// Lower enable bits select the illustration and eligible sprite consumers;
// palette eligibility itself is a sprite-format rule.
static_assert((FF6_ROM_VALUE_ENDING_CHARACTER_COMPOSITE_MODES_1 & 0x11U) == 0x11U);
constexpr auto ending_composition = composite_blend(
    (FF6_ROM_VALUE_ENDING_CHARACTER_COMPOSITE_MODES_1 & 0x80U) != 0,
    (FF6_ROM_VALUE_ENDING_CHARACTER_COMPOSITE_MODES_1 & 0x40U) != 0,
    (FF6_ROM_VALUE_ENDING_CHARACTER_COMPOSITE_MODES_0 & 2U) == 0);
// The extractor validates the shared background period at every source site.
// Text/fade-out callers must also agree before native constants coalesce.
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_1);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_2);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_3);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_4);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_5);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_6);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_7);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_8);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_9);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_10);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_11);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0 == FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_12);
// Movement phases come from the shared large-letter task's ROM operands.
// Each counter phase includes the visit which observes zero before advancing.
static_assert(FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_4 == FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_5);
static_assert(FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_0 == FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_1);
constexpr unsigned lettering_initial_wait = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_0;
constexpr unsigned lettering_move_in = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_1 + 1U;
constexpr unsigned lettering_second_start = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_1 + FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_2 + 3U;
constexpr unsigned lettering_move_out = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_3 + 1U;
constexpr unsigned lettering_pixel_period = 256U / (65536U - FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_0);
// Authored phase waits come from the ROM; native deadlines sum those phases.
// The two book endings share the pre-text-fade wait and terminal fade owner.
static_assert(FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_3 == FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_8);
static_assert(FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_4 == FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_9);
constexpr unsigned book_opening_animation = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_0;
constexpr unsigned book_closing_animation = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_6;
constexpr unsigned book_opening_fade = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_0 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_1 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_2;
constexpr unsigned book_closing_fade = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_7;
constexpr unsigned book_opening_end = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_0 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_1 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_2 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_3 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_4;
constexpr unsigned book_closing_end = FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_7 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_8 + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_9;
void require(bool value, const char* reason) {
    if (!value) throw std::invalid_argument(reason);
}
RasterCell cell(std::uint16_t value, unsigned layer) {
    return {value & 1023U, (value >> 10U) & 7U,
            static_cast<std::uint16_t>((value & 0x2000U ? 11 : 7) - layer),
            (value & 0x4000U) != 0, (value & 0x8000U) != 0};
}
CutscenePalette palette(ByteView data, std::size_t offset) {
    CutscenePalette result{};
    for (unsigned c = 0; c < 16; ++c) result[c] = read16(data, offset + c * 2) & 0x7fffU;
    return result;
}
}
EndingTilePatch decode_ending_tile_patch(ByteView graphics, unsigned bytes, unsigned rows,
                                       unsigned source, unsigned destination) {
    require(bytes && bytes <= 64 && !(bytes & 1U) && rows && rows <= 32 &&
                destination >= 0x3849 && !((destination - 0x3849) & 1U),
            "Invalid ending tile rectangle");
    const unsigned at = (destination - 0x3849) / 2;
    EndingTilePatch patch{at % 32, at / 32, bytes / 2, rows, {}};
    require(patch.column + patch.columns <= 32 && patch.row + rows <= 32,
            "Ending tile rectangle exceeds illustration");
    for (unsigned y = 0; y < rows; ++y)
        for (unsigned x = 0; x < patch.columns; ++x) {
            const auto value = read16(graphics, 0x4000 + source + y * 64 + x * 2);
            require((value & 1023U) < 768, "Invalid ending illustration tile");
            patch.cells.push_back(cell(value, 0));
        }
    return patch;
}
void apply_ending_tile_patch(TileLayer& layer, const EndingTilePatch& patch) {
    require(patch.cells.size() == patch.columns * patch.rows &&
                patch.column + patch.columns <= layer.columns && patch.row + patch.rows <= layer.rows,
            "Invalid native ending tile patch");
    for (unsigned y = 0; y < patch.rows; ++y)
        for (unsigned x = 0; x < patch.columns; ++x)
            layer.cells[(patch.row + y) * layer.columns + patch.column + x] =
                patch.cells[y * patch.columns + x];
}
EndingBookAssets load_ending_book_assets(const ContentPack& pack) {
    EndingBookAssets result;
    const auto graphics = decompress_lzss(pack.get("cutscene_assets_ending1_bin_lz"));
    const auto data = pack.get("data_c28a70_ending_animation_and_credits");
    require(graphics.size() == 0x5040, "Invalid ending illustration extent");
    require(data.size() == 7146, "Invalid ending animation data extent");
    // LoadEndingBGGfx's three prepared tile ranges share authored tile IDs.
    // The gaps are transparent, as established by the cinematic initialization.
    std::vector<Tile> tiles(768);
    for (const auto& range : {std::array<unsigned, 3>{0, 0x1f60, 0},
                              {0x1f60, 0xb40, 256}, {0x2aa0, 0xe00, 512}}) {
        const auto decoded = decode_tiles(slice(graphics, range[0], range[1]), 4);
        std::copy(decoded.begin(), decoded.end(), tiles.begin() + range[2]);
    }
    for (unsigned layer = 0; layer < 2; ++layer) {
        auto& out = layer ? result.shading : result.illustration;
        out.columns = 64; out.rows = layer ? 32 : 64;
        out.cells.resize(out.columns * out.rows);
        out.tiles = tiles;
        out.palettes.resize(8);
        out.lines.resize(224);
        for (auto& line : out.lines) {
            line.offset_y = 1;
            line.left = FF6_ROM_VALUE_ENDING_BOOK_WINDOW_EDGES_0;
            line.right = FF6_ROM_VALUE_ENDING_BOOK_WINDOW_EDGES_1 + 1;
        }
        const auto map = slice(graphics, layer ? 0x4448 : 0x38a0, 0x780);
        for (unsigned i = 0; i < 960; ++i) {
            const auto value = read16(map, i * 2);
            require((value & 1023U) < tiles.size(), "Invalid ending illustration tile");
            out.cells[(i / 32) * 64 + i % 32] = cell(value, layer);
            // Both pages of the scrolling illustration start with the same map.
            if (!layer) out.cells[(i / 32) * 64 + 32 + i % 32] = cell(value, layer);
        }
    }
    result.illustration.mix = {ending_composition.covered, ending_composition.uncovered};
    result.shading.operand = (FF6_ROM_VALUE_ENDING_CHARACTER_OPERAND_LAYERS_0 & 2U) != 0;
    result.shading.clip_operand = false;
    // Palette assignments come from C3/E192. Buffer origin/stride and palette
    // resource addresses describe their formats; unused colors remain black.
    for (const auto [index, offset] : {
            std::pair{(FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_0 - 0x3049U) / 32U, 0x967cU},
            {(FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_1 - 0x3049U) / 32U, 0x969cU},
            {(FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_2 - 0x3049U) / 32U, 0x96dcU},
            {(FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_3 - 0x3049U) / 32U, 0x96fcU}})
        result.colors[index] = palette(data, offset - 0x8a70);
    result.lettering = palette(data, 0x9754 - 0x8a70);
    // The six book frames are rectangles, not executable transfer commands.
    for (unsigned i = 0; i < 6; ++i) {
        const auto record = slice(data, 0x8b81 - 0x8a70 + i * 8, 8);
        const unsigned bytes = read16(record, 0), rows = read16(record, 2);
        const unsigned source = read16(record, 4), destination = read16(record, 6);
        result.patches.push_back(decode_ending_tile_patch(graphics, bytes, rows, source, destination));
    }
    constexpr std::array<unsigned, 3> streams{0x8b63, 0x8b45, 0x8b54};
    for (unsigned i = 0; i < streams.size(); ++i) {
        std::size_t at = streams[i] - 0x8a70;
        for (unsigned count = 0; count < 85; ++count, at += 3) {
            const unsigned pointer = read16(data, at);
            const auto duration = slice(data, at + 2, 1)[0];
            require(pointer >= 0x8b81 && pointer <= 0x8ba9 && (pointer - 0x8b81) % 8 == 0,
                    "Unresolved ending book frame");
            result.animations[i].push_back({(pointer - 0x8b81) / 8, duration});
            if (duration >= 254) break;
        }
        require(result.animations[i].back().duration >= 254, "Unterminated ending book animation");
    }
    const auto font = decompress_lzss(pack.get("cutscene_assets_ending_font_2bpp_lz"));
    // The authored font contains 138 tiles. The original transfer rounds up
    // to 144; the selected letters never use the six extra scratch-data tiles.
    require(font.size() == 0x8a0, "Invalid ending font extent");
    const auto letters = decode_tiles(font, 2);
    std::vector<Tile> objects(512);
    std::copy(letters.begin(), letters.end(), objects.begin() + 256);
    const auto names = pack.get("data_cff450_ending_animation");
    const unsigned pointer = read16(names, 0xf8ab - 0xf450);
    require(pointer >= 0xf450 && pointer < 0xfb29, "Invalid ending closing-text group");
    std::size_t at = pointer - 0xf450;
    // Preserve the ROM's count, including its trailing descriptors. Blank
    // graphics/palettes make these harmless; do not silently shorten the data.
    result.closing_text = decode_cutscene_sprite_group(names, at, objects);
    return result;
}
bool is_ending_book_segment(std::uint8_t selector) { return selector == 45 || selector == 74; }
EndingBookSegment ending_book_segment(std::uint8_t selector) {
    require(is_ending_book_segment(selector), "Ending segment is not an implemented book cinematic");
    return selector == 45 ? EndingBookSegment::opening : EndingBookSegment::closing;
}
EndingBook::EndingBook(EndingBookAssets assets, EndingBookSegment segment)
    : assets_(std::move(assets)), segment_(segment) {
    for (const auto& animation : assets_.animations) {
        require(!animation.empty() && animation.back().duration >= 254,
                "Unterminated ending book animation");
        for (const auto& frame : animation)
            require(frame.resource < assets_.patches.size(), "Invalid ending book frame identity");
    }
    for (auto& fade : fades_) fade = CutscenePaletteFade{FF6_ROM_VALUE_ENDING_BOOK_BACKGROUND_FADE_PERIODS_0};
    apply_patch(0);
    if (segment_ == EndingBookSegment::closing) start_animation(1);
}
void EndingBook::apply_patch(std::size_t identity) {
    apply_ending_tile_patch(assets_.illustration, assets_.patches.at(identity));
}
void EndingBook::start_animation(unsigned identity) {
    animation_ = identity;
    cursor_ = {0, assets_.animations.at(identity).front().duration};
    animating_ = true;
}
bool EndingBook::complete() const {
    return updates_ >= (segment_ == EndingBookSegment::opening ? book_opening_end : book_closing_end);
}
bool EndingBook::advance() {
    if (complete()) return false;
    const bool opening = segment_ == EndingBookSegment::opening;
    const unsigned fade_start = opening ? book_opening_fade : book_closing_fade;
    if (opening && updates_ == book_opening_animation) start_animation(0);
    if (!opening && updates_ == book_closing_animation) start_animation(2);
    if (updates_ == fade_start) for (auto& fade : fades_) fade = CutscenePaletteFade{FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0};
    if (updates_ == fade_start + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_3) text_fade_ = CutscenePaletteFade{FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0};
    const CutscenePalette black{};
    for (unsigned p = 0; p < colors_.size(); ++p)
        fades_[p].advance(colors_[p], updates_ < fade_start ? assets_.colors[p] : black);
    if (!opening && updates_ >= book_closing_animation)
        text_fade_.advance(text_colors_, updates_ < fade_start + FF6_ROM_VALUE_ENDING_BOOK_PHASE_WAITS_3 ? assets_.lettering : black);
    if (animating_) {
        advance_cutscene_animation(cursor_, assets_.animations[animation_]);
        apply_patch(assets_.animations[animation_][cursor_.frame].resource);
    }
    ++updates_;
    return true;
}
RasterScene EndingBook::scene() const {
    RasterScene out;
    out.layers = {assets_.illustration, assets_.shading};
    const bool opening = segment_ == EndingBookSegment::opening;
    const unsigned fade_start = opening ? book_opening_fade : book_closing_fade;
    for (auto& layer : out.layers) {
        std::copy(colors_.begin(), colors_.end(), layer.palettes.begin());
        for (unsigned y = 0; y < layer.lines.size(); ++y) {
            auto& line = layer.lines[y];
            if (updates_ > fade_start) {
                // The extracted reload includes the task visit which observes zero.
                line.sample_width = static_cast<std::uint8_t>(1 + ((updates_ - fade_start - 1) / FF6_ROM_VALUE_ENDING_BOOK_MOSAIC_PERIOD_0) % 16);
                line.sample_row = static_cast<int>(y - y % line.sample_width);
            }
        }
    }
    // Only the opening book scrolls while fading; the closing book stays still.
    if (opening && updates_ > fade_start)
        for (auto& line : out.layers[0].lines)
            line.offset_x = static_cast<int>(std::min(updates_ - fade_start, unsigned{FF6_ROM_VALUE_ENDING_BOOK_SCROLL_DURATION_0}) /
                (FF6_ROM_VALUE_ENDING_BOOK_SCROLL_MASK_0 + 1U));
    out.objects.emplace();
    out.objects->lines.resize(224);
    if (!opening) {
        // Shared large-letter movement uses extracted waits and fixed-point speed.
        // Preserve counter-zero visits and negative subpixel rounding.
        const unsigned motion = std::min(updates_ > lettering_initial_wait + 1U ?
            updates_ - lettering_initial_wait - 1U : 0U, lettering_move_in) +
            std::min(updates_ > lettering_initial_wait + lettering_second_start ?
            updates_ - lettering_initial_wait - lettering_second_start : 0U, lettering_move_out);
        const int top = FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_ORIGIN_2 -
            static_cast<int>((motion + lettering_pixel_period - 1U) / lettering_pixel_period);
        for (const auto& piece : assets_.closing_text) {
            auto image = piece.image;
            image.x = wrap8(static_cast<unsigned>(image.x) + FF6_ROM_VALUE_ENDING_BOOK_TEXT_HORIZONTAL_ORIGIN_0);
            image.y = wrap8(static_cast<unsigned>(image.y + top)) - 1;
            // Original sprites wrap at the bottom of the 256-row object space.
            if (image.y >= 224) image.y -= 256;
            image.colors = piece.palette == 0 ? text_colors_ : CutscenePalette{};
            out.objects->sprites.push_back(std::move(image));
        }
    }
    out.intensity.assign(224, complete() ? 0 : 15);
    return out;
}
} // namespace ff6
