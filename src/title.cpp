#include "ff6/title.hpp"
#include "rom-materials/title_terminal_wait.macros.inc"
#include "rom-materials/title_caption_x.macros.inc"
#include "rom-materials/title_caption_y.macros.inc"
#include "rom-materials/title_caption_palette_destination.macros.inc"
#include "rom-materials/title_tile_adjustments.macros.inc"
#include "rom-materials/title_map_graphics_directory.macros.inc"
#include "rom-materials/title_splash_horizontal_origin.macros.inc"
#include "rom-materials/title_composite_modes.macros.inc"
#include "rom-materials/title_blend_layer_mask.macros.inc"
#include "rom-materials/title_operand_layer_mask.macros.inc"
#include "rom-materials/title_splash_caption_palette_source.macros.inc"
#include "rom-materials/title_splash_background_destinations.macros.inc"
#include "rom-materials/title_first_empty_page_address.macros.inc"
#include "rom-materials/title_second_empty_page_address.macros.inc"
#include "rom-materials/title_logo_layout_destination.macros.inc"
#include "rom-materials/title_effect_layout_offset.macros.inc"
#include <algorithm>
#include <bit>

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
CutscenePalette palette(ByteView data) {
    require(data.size() == 32, "Invalid cutscene palette extent");
    CutscenePalette result{};
    for (std::size_t i = 0; i < 16; ++i)
        result[i] = read16(data, i * 2) & 0x7fffU;
    return result;
}
void install(Bytes& target, std::size_t offset, ByteView source) {
    require(offset <= target.size() && source.size() <= target.size() - offset,
            "Title graphics exceed their typed resource");
    std::ranges::copy(source, target.begin() + static_cast<std::ptrdiff_t>(offset));
}
TileLayer background(ByteView encoded, std::vector<Tile> tiles, unsigned index) {
    TileLayer layer;
    layer.columns = index == 0 ? 64 : 32;
    layer.rows = layer.columns;
    layer.tiles = std::move(tiles);
    layer.palettes.resize(8);
    layer.lines.resize(224);
    for (auto& line : layer.lines) {
        line.offset_x = index == 0 ? FF6_ROM_VALUE_TITLE_SPLASH_HORIZONTAL_ORIGIN_0 : 0;
        line.offset_y = 1; // First canonical row is the first visible scanline.
    }
    layer.cells.resize(std::size_t{layer.columns} * layer.rows);
    require(encoded.size() == layer.cells.size() * 2, "Invalid title layout extent");
    // Source pages contain 32x32 cells. Expose ordinary row-major semantic cells.
    for (unsigned y = 0; y < layer.rows; ++y)
        for (unsigned x = 0; x < layer.columns; ++x) {
            const auto page = index == 0 ? (y / 32) * 2 + x / 32 : 0;
            const auto raw = read16(encoded, (page * 1024 + (y % 32) * 32 + x % 32) * 2);
            const auto palette_index = (raw >> 10U) & 7U;
            layer.cells[y * layer.columns + x] = {
                raw & 1023U, palette_index,
                static_cast<std::uint16_t>(index == 0   ? ((raw & 8192U) ? 11 : 7)
                                           : index == 1 ? ((raw & 8192U) ? 10 : 6)
                                                        : ((raw & 8192U) ? 3 : 1)),
                (raw & 16384U) != 0, (raw & 32768U) != 0};
        }
    // Selected flags become semantic composition choices, never register writes.
    constexpr auto composition = composite_blend(
        (FF6_ROM_VALUE_TITLE_COMPOSITE_MODES_1 & 0x80U) != 0,
        (FF6_ROM_VALUE_TITLE_COMPOSITE_MODES_1 & 0x40U) != 0,
        (FF6_ROM_VALUE_TITLE_COMPOSITE_MODES_0 & 2U) == 0);
    if (index == std::countr_zero(static_cast<unsigned>(FF6_ROM_VALUE_TITLE_BLEND_LAYER_MASK_0)))
        layer.mix = {composition.covered, composition.uncovered};
    if (index == std::countr_zero(static_cast<unsigned>(FF6_ROM_VALUE_TITLE_OPERAND_LAYER_MASK_0)))
        layer.operand = true;
    return layer;
}
} // namespace

Rgb555 step_cutscene_fade(Rgb555 current, Rgb555 target, std::uint8_t threshold) {
    require(current <= 0x7fff && target <= 0x7fff && threshold <= 31,
            "Invalid cutscene fade color or threshold");
    unsigned result = 0;
    for (unsigned shift : {0U, 5U, 10U}) {
        unsigned from = (current >> shift) & 31U, to = (target >> shift) & 31U;
        if (from > to)
            --from;
        else if (to > from && to - from >= threshold)
            ++from;
        result |= from << shift;
    }
    return static_cast<Rgb555>(result);
}
void CutscenePaletteFade::advance(CutscenePalette& current, const CutscenePalette& target) {
    require(threshold <= 31, "Invalid cutscene palette fade state");
    if (finished)
        return;
    if (!initialized) {
        initialized = true;
        threshold = 31;
        return;
    }
    if (wait == 0) {
        auto next = current;
        for (std::size_t i = 0; i < 16; ++i)
            next[i] = step_cutscene_fade(current[i], target[i], threshold);
        current = next;
        wait = period;
        if (threshold == 0 || --threshold == 0) {
            finished = true;
            return;
        }
    }
    // The original byte counter treats a zero period as 256 visits.
    wait = wrap8(unsigned{wait} + 255U);
}
TitleAssets load_title_assets(const ContentPack& pack) {
    TitleAssets assets{};
    const auto data = decompress_lzss(pack.get("cutscene_assets_title_opening_bin_lz"));
    const auto neutral = pack.get("cutscene_neutral_palettes"),
               palettes = pack.get("title_palettes");
    require(neutral.size() == 64 && palettes.size() == 288, "Invalid title palette resources");
    assets.splash = palette(pack.get("title_splash_palette"));
    assets.white = palette(neutral.first(32));
    assets.black = palette(neutral.last(32));
    for (std::size_t i = 0; i < 9; ++i)
        assets.palettes[i] = palette(palettes.subspan(i * 32, 32));

    // ClearVRAM supplies zero to unwritten tiles; these are typed graphics pools,
    // not a memory bus. Source spans preserve overlapping/partial tile writes.
    Bytes town_tiles(1024 * 32), effect_tiles(256 * 32), object_tiles(512 * 32);
    const auto map_graphics = pack.get("map_graphics_map_gfx_4bpp");
    const auto pointers = pack.get("map_graphics_map_gfx_ptr_bin");
    require(pointers.size() == 82 * 3, "Invalid map graphics directory for title");
    install(town_tiles, 0, slice(map_graphics, read24(pointers, FF6_ROM_VALUE_TITLE_MAP_GRAPHICS_DIRECTORY_0 - 0xda00U), 8192));
    install(town_tiles, 256 * 32, slice(data, 0x33c0, 0x4200));
    install(town_tiles, 887 * 32, slice(data, 0x8de0, 0x1110));
    install(effect_tiles, 0, slice(data, 0x7b30, 0x6b0));
    install(effect_tiles, 128 * 32, slice(data, 0x83e0, 0x800));
    install(object_tiles, 0, ByteView(town_tiles).subspan(768 * 32, 256 * 32));
    install(object_tiles, 256 * 32, slice(data, 0x9ef0, 0xfb8));
    Bytes town_layout(4096 * 2), flames_layout(1024 * 2), haze_layout(1024 * 2);
    install(town_layout, 0, slice(data, 0x1bc0, 0x1800));
    for (unsigned page : {FF6_ROM_VALUE_TITLE_FIRST_EMPTY_PAGE_ADDRESS_0 / 1024U,
                          FF6_ROM_VALUE_TITLE_SECOND_EMPTY_PAGE_ADDRESS_0 / 1024U})
        for (unsigned cell = 0; cell < 1024; ++cell) {
            town_layout[(page * 1024 + cell) * 2] = wrap8(FF6_ROM_VALUE_TITLE_TILE_ADJUSTMENTS_0);
            town_layout[(page * 1024 + cell) * 2 + 1] = wrap8(FF6_ROM_VALUE_TITLE_TILE_ADJUSTMENTS_0 >> 8U);
        }
    for (unsigned i = 0; i < 256; ++i) {
        const auto logo = wrap16(read16(data, 0x8be0 + i * 2) + static_cast<unsigned>(FF6_ROM_VALUE_TITLE_TILE_ADJUSTMENTS_0));
        town_layout[FF6_ROM_VALUE_TITLE_LOGO_LAYOUT_DESTINATION_0 * 2U + i * 2] = wrap8(logo);
        town_layout[FF6_ROM_VALUE_TITLE_LOGO_LAYOUT_DESTINATION_0 * 2U + 1U + i * 2] = wrap8(logo >> 8U);
        const auto flames = wrap16(read16(data, 0x81e0 + i * 2) + static_cast<unsigned>(FF6_ROM_VALUE_TITLE_TILE_ADJUSTMENTS_1));
        flames_layout[FF6_ROM_VALUE_TITLE_EFFECT_LAYOUT_OFFSET_0 * 2U + i * 2] = wrap8(flames);
        flames_layout[FF6_ROM_VALUE_TITLE_EFFECT_LAYOUT_OFFSET_0 * 2U + 1U + i * 2] = wrap8(flames >> 8U);
        const auto haze = wrap16(read16(data, 0x7930 + i * 2) + static_cast<unsigned>(FF6_ROM_VALUE_TITLE_TILE_ADJUSTMENTS_2));
        haze_layout[FF6_ROM_VALUE_TITLE_EFFECT_LAYOUT_OFFSET_0 * 2U + i * 2] = wrap8(haze);
        haze_layout[FF6_ROM_VALUE_TITLE_EFFECT_LAYOUT_OFFSET_0 * 2U + 1U + i * 2] = wrap8(haze >> 8U);
    }
    assets.backgrounds[0] = background(town_layout, decode_tiles(town_tiles, 4), 0);
    assets.backgrounds[1] = background(flames_layout, decode_tiles(effect_tiles, 4), 1);
    assets.backgrounds[2] = background(haze_layout, decode_tiles(effect_tiles, 2), 2);
    assets.sprite_tiles = decode_tiles(object_tiles, 4);
    const auto& tiles = assets.sprite_tiles;
    const auto descriptors = pack.get("title_legal_sprites");
    std::size_t cursor = 0;
    for (const unsigned base_x : {static_cast<unsigned>(FF6_ROM_VALUE_TITLE_CAPTION_X_0), static_cast<unsigned>(FF6_ROM_VALUE_TITLE_CAPTION_X_1)}) {
        const auto count = slice(descriptors, cursor++, 1)[0];
        require(count > 0 && count <= 128 && assets.legal_text.size() + count <= 128,
                "Invalid title sprite group count");
        for (unsigned i = 0; i < count; ++i) {
            const auto item = slice(descriptors, cursor, 4);
            cursor += 4;
            const unsigned side = (item[0] & 128U) != 0 ? 32 : 16;
            const unsigned first = item[2] + ((item[3] & 1U) * 256U);
            require(((item[3] >> 1U) & 7U) == (FF6_ROM_VALUE_TITLE_CAPTION_PALETTE_DESTINATION_0 - 0x3100U) / 32U, "Title legal sprite uses an unowned palette");
            RasterSprite sprite;
            sprite.x = wrap8((item[0] & 127U) + base_x);
            // Object Y denotes the first visible row; unlike background sampling,
            // its scanline convention does not require the +1 source-row offset.
            sprite.y = wrap8(item[1] + static_cast<unsigned>(FF6_ROM_VALUE_TITLE_CAPTION_Y_0));
            sprite.width = static_cast<std::uint16_t>(side);
            sprite.height = sprite.width;
            constexpr std::array<std::uint16_t, 4> depths{2, 5, 8, 12};
            sprite.depth = depths[(item[3] >> 4U) & 3U];
            sprite.flip_x = (item[3] & 64U) != 0;
            sprite.flip_y = (item[3] & 128U) != 0;
            sprite.pixels.resize(side * side);
            for (unsigned y = 0; y < side; ++y)
                for (unsigned x = 0; x < side; ++x) {
                    const auto tile = (first & ~15U) + ((first + x / 8) & 15U) + (y / 8) * 16;
                    require(tile < tiles.size(), "Title sprite exceeds its graphics");
                    sprite.pixels[y * side + x] = tiles[tile][(y % 8) * 8 + x % 8];
                }
            assets.legal_text.push_back(std::move(sprite));
        }
        if (base_x == FF6_ROM_VALUE_TITLE_CAPTION_X_0)
            assets.first_legal_group_size = assets.legal_text.size();
    }
    require(cursor == descriptors.size(), "Trailing title sprite descriptors");
    return assets;
}
TitleSplash::TitleSplash(TitleAssets assets) : assets_(std::move(assets)) {
    state_.colors.fill(assets_.black);
}
bool TitleSplash::advance() {
    if (state_.phase == SplashPhase::complete)
        return false;
    if (state_.phase == SplashPhase::fading && state_.remaining == 0) {
        state_.phase = SplashPhase::complete;
        state_.intensity = 0;
        return false;
    }
    if (state_.phase == SplashPhase::terminating) {
        state_.phase = SplashPhase::fading;
        state_.remaining = FF6_ROM_VALUE_TITLE_TERMINAL_WAIT_0;
    } else if (state_.phase == SplashPhase::holding && state_.remaining == 0)
        state_.phase = SplashPhase::terminating;
    if (state_.phase == SplashPhase::fading)
        state_.intensity = static_cast<std::uint8_t>(state_.remaining);
    fades_[0].advance(state_.colors[0], assets_.white);
    fades_[1].advance(state_.colors[1], assets_.splash);
    fades_[2].advance(state_.colors[2], assets_.palettes[(FF6_ROM_VALUE_TITLE_SPLASH_CAPTION_PALETTE_SOURCE_0 - 0x7b43U) / 32U]);
    if (state_.remaining)
        --state_.remaining;
    state_.frame_counter = wrap8(state_.frame_counter + 1U);
    ++state_.visits;
    return true;
}
RasterScene TitleSplash::scene() const {
    require(state_.visits > 0 && state_.phase != SplashPhase::complete,
            "Splash has no published frame");
    RasterScene scene;
    scene.intensity.assign(224, state_.intensity);
    scene.backdrop = state_.colors[0][0];
    scene.layers.assign(assets_.backgrounds.begin(), assets_.backgrounds.end());
    for (unsigned layer = 0; layer < 3; ++layer) {
        auto& palettes = scene.layers[layer].palettes;
        require(palettes.size() == 8, "Invalid title background palette ownership");
        palettes[(FF6_ROM_VALUE_TITLE_SPLASH_BACKGROUND_DESTINATIONS_0 - 0x3000U) / 32U] = state_.colors[0];
        palettes[(FF6_ROM_VALUE_TITLE_SPLASH_BACKGROUND_DESTINATIONS_1 - 0x3000U) / 32U] = state_.colors[1];
        if (layer == 2)
            for (unsigned group = 0; group < 8; ++group) {
                for (unsigned color = 0; color < 4; ++color)
                    palettes[group][color] = state_.colors[group / 4][(group % 4) * 4 + color];
            }
    }
    scene.objects = SpriteLayer{};
    scene.objects->lines.resize(224);
    scene.objects->sprites = assets_.legal_text;
    for (auto& sprite : scene.objects->sprites)
        sprite.colors = state_.colors[2];
    return scene;
}
} // namespace ff6
