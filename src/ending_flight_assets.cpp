#include "ff6/ending_flight.hpp"
#include "rom-materials/ending_flight_add_arithmetic.macros.inc"
#include "rom-materials/ending_flight_subtract_arithmetic.macros.inc"
#include "rom-materials/ending_flight_text_palette_banks.macros.inc"
#include "rom-materials/ending_flight_background_palette_routes.macros.inc"
#include "rom-materials/ending_flight_sprite_palette_routes.macros.inc"
#include "rom-materials/ending_credit_horizon.macros.inc"
#include <algorithm>
#include <map>
#include "rom-materials/ending_credit_page_starts.macros.inc"
#include "rom-materials/ending_credit_page_intervals.macros.inc"
#include "rom-materials/ending_credit_font_attributes.macros.inc"

namespace ff6 {
namespace {
using Kind = EndingFlightScene;
using Object = EndingFlightObject;
// Original palette addresses identify banks; origin/stride are format metadata.
constexpr unsigned text_palette_first = (FF6_ROM_VALUE_ENDING_FLIGHT_TEXT_PALETTE_BANKS_0 - 0x3149U) / 32U;
constexpr unsigned text_palette_second = (FF6_ROM_VALUE_ENDING_FLIGHT_TEXT_PALETTE_BANKS_1 - 0x3149U) / 32U;
// The compiler shares these destinations with the base palette list and sky.
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_6 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_8);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_7 ==
              FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_0);
void require(bool value, const char* why) { if (!value) throw std::invalid_argument(why); }
constexpr std::array<std::uint8_t, 10> selectors{2, 4, 5, 9, 12, 16, 20, 24, 28, 32};
CutscenePalette palette(ByteView data, unsigned address) {
    CutscenePalette out{};
    for (unsigned i = 0; i < 16; ++i) out[i] = read16(data, address - 0x8a70 + i * 2) & 0x7fffU;
    return out;
}
EndingSpriteAnimation animation(ByteView data, unsigned base, unsigned address, std::span<const Tile> tiles) {
    require(address >= base, "Invalid flight animation pointer");
    EndingSpriteAnimation out;
    for (unsigned n = 0; n < 85; ++n) {
        const unsigned at = address - base + n * 3;
        const unsigned pointer = read16(data, at);
        const auto duration = slice(data, at + 2, 1)[0];
        require(pointer >= base, "Invalid flight image pointer");
        std::size_t cursor = pointer - base;
        out.groups.push_back(decode_cutscene_sprite_group(data, cursor, tiles));
        out.frames.push_back({n, duration});
        if (duration >= 254) return out;
    }
    throw std::invalid_argument("Unterminated flight animation");
}
struct PageRange { unsigned address, bytes; };
// Page ranges are source metadata; words, glyphs and positions come from the pack.
const std::array<std::vector<PageRange>, 7> page_ranges{{
    {{0x9894, 12}},
    {{0x98a0, 20}, {0x98b4, 24}, {0x98cc, 24}, {0x98e4, 24}, {0x98fc, 12}, {0x9908, 16}, {0x9918, 24}},
    {{0x9930, 32}, {0x9950, 24}, {0x9968, 24}, {0x9980, 16}},
    {{0x9990, 16}, {0x99a0, 20}, {0x99b4, 36}, {0x99d8, 36}, {0x99fc, 36}, {0x9a20, 20}, {0x9a34, 20}},
    {{0x9a48, 16}, {0x9a58, 24}, {0x9a70, 24}, {0x9a88, 36}, {0x9aac, 24}, {0x9ac4, 24}, {0x9adc, 24}},
    {{0x9af4, 40}, {0x9b1c, 40}, {0x9b44, 36}, {0x9b68, 32}, {0x9b88, 24}, {0x9ba0, 20}, {0x9bb4, 36}},
    {{0x9bd8, 24}, {0x9bf0, 24}, {0x9c08, 20}, {0x9c1c, 20}, {0x9c30, 20}},
}};
std::vector<EndingFlightPage> lettering(ByteView data, unsigned scene, std::span<const Tile> tiles) {
    constexpr std::array<unsigned, 7> small{0x9dc0, 0x9dc4, 0x9dec, 0x9e04, 0x9e24, 0x9e48, 0x9f2c};
    constexpr std::array<unsigned, 7> small_count{1, 10, 6, 8, 9, 57, 27};
    constexpr std::array<unsigned, 5> large{0x9c44, 0x9c4c, 0x9cac, 0x9cec, 0x9d50};
    constexpr std::array<unsigned, 5> large_count{2, 24, 16, 25, 28};
    std::map<unsigned, std::vector<CutsceneSprite>> words;
    auto load = [&](unsigned table, unsigned count, bool big) {
        for (unsigned word = 0; word < count; ++word) {
            const auto at = table - 0x8a70 + word * 4;
            unsigned pointer = read16(data, at);
            const unsigned identity = read16(data, at + 2);
            require(pointer >= 0x8a70, "Invalid credits word pointer");
            Bytes descriptors{0};
            for (unsigned letter = 0; letter < 32; ++letter) {
                const auto glyph = slice(data, pointer++ - 0x8a70, 1)[0];
                if (!glyph) break;
                require(letter < 16, "Credit word exceeds sprite-group horizontal range");
                ++descriptors[0];
                descriptors.push_back(static_cast<std::uint8_t>((big ? 128 : 0) + letter * 8));
                descriptors.push_back(0); descriptors.push_back(glyph);
                descriptors.push_back(big ? FF6_ROM_VALUE_ENDING_CREDIT_FONT_ATTRIBUTES_1
                                          : FF6_ROM_VALUE_ENDING_CREDIT_FONT_ATTRIBUTES_0);
            }
            std::size_t cursor = 0;
            require(words.emplace(identity, decode_cutscene_sprite_group(descriptors, cursor, tiles)).second,
                    "Duplicate credits word identity");
        }
    };
    load(small[scene], small_count[scene], false);
    if (scene < 5) load(large[scene], large_count[scene], true);
    // Validate every authored page deadline before sharing native cadence branches.
    // These compile-time inputs do not create a runtime copy of the schedules.
    static_assert([] {
        constexpr std::array<unsigned, 7> starts{
#include "rom-materials/ending_credit_page_starts.inc"
        };
        constexpr std::array<unsigned, 31> intervals{
#include "rom-materials/ending_credit_page_intervals.inc"
        };
        if (starts[0] != starts[6]) return false;
        for (unsigned i = 2; i < 5; ++i)
            if (starts[i] != starts[1]) return false;
        for (unsigned i = 1; i < 25; ++i)
            if (intervals[i] != intervals[0]) return false;
        for (unsigned i = 26; i < intervals.size(); ++i)
            if (intervals[i] != intervals[25]) return false;
        return true;
    }());
    std::vector<EndingFlightPage> out;
    for (const auto range : page_ranges[scene]) {
        EndingFlightPage page;
        const unsigned first = scene == 0 || scene == 6
            ? FF6_ROM_VALUE_ENDING_CREDIT_PAGE_STARTS_0
            : scene == 5 ? FF6_ROM_VALUE_ENDING_CREDIT_PAGE_STARTS_5
                         : FF6_ROM_VALUE_ENDING_CREDIT_PAGE_STARTS_1;
        page.start = first + static_cast<unsigned>(out.size()) *
            (scene == 5 ? FF6_ROM_VALUE_ENDING_CREDIT_PAGE_INTERVALS_25
                        : FF6_ROM_VALUE_ENDING_CREDIT_PAGE_INTERVALS_0);
        for (unsigned at = range.address - 0x8a70; at < range.address - 0x8a70 + range.bytes; at += 4) {
            const auto found = words.find(read16(data, at));
            require(found != words.end(), "Credits page references unloaded word");
            const auto pos = slice(data, at + 2, 2);
            for (auto glyph : found->second) {
                glyph.image.x += pos[0]; glyph.image.y += pos[1];
                page.lettering.push_back(std::move(glyph));
            }
        }
        out.push_back(std::move(page));
    }
    return out;
}
}
bool is_ending_flight_segment(std::uint8_t selector) {
    return std::find(selectors.begin(), selectors.end(), selector) != selectors.end();
}
EndingFlightScene ending_flight_segment(std::uint8_t selector) {
    const auto found = std::find(selectors.begin(), selectors.end(), selector);
    require(found != selectors.end(), "Ending segment is not a flight cinematic");
    return static_cast<Kind>(found - selectors.begin());
}
std::vector<EndingCameraStep> decode_ending_camera(ByteView data) {
    std::vector<EndingCameraStep> out;
    for (unsigned at = 0; at < data.size(); at += 3) {
        const auto bits = read16(data, at);
        require((bits & 0x300fU) == 0, "Unknown ending camera motion");
        const auto duration = slice(data, at + 2, 1)[0];
        auto delta = [bits](unsigned positive, unsigned negative) {
            return static_cast<int>((bits & positive) != 0) - static_cast<int>((bits & negative) != 0);
        };
        out.push_back({delta(0x100, 0x200), delta(0x400, 0x800), delta(0x10, 0x20),
                       delta(0x40, 0x4000), 256 * delta(0x80, 0x8000), duration});
        if (duration >= 254) return out;
    }
    throw std::invalid_argument("Unterminated ending camera motion");
}
EndingFlightAssets load_ending_flight_assets(const ContentPack& pack, Kind kind) {
    require(static_cast<unsigned>(kind) < selectors.size(), "Invalid ending flight kind");
    EndingFlightAssets out; out.kind = kind;
    const auto data = pack.get("data_c28a70_ending_animation_and_credits");
    const auto later = pack.get("data_cff450_ending_animation");
    const auto graphics = decompress_lzss(pack.get("cutscene_assets_credits_bin_lz"));
    const bool clouds = kind <= Kind::distant_airship || kind == Kind::jet;
    const bool land = kind == Kind::land_birds || kind == Kind::landscape || kind == Kind::big_airship;
    const unsigned graphics_at = clouds ? 0x2d3a : 0x541a;
    const unsigned graphics_size = clouds ? 0xee0 : 0x18e0;
    auto& terrain = out.terrain;
    terrain.columns = terrain.rows = 128; terrain.depth = 3;
    terrain.tiles = decode_tiles(slice(graphics, graphics_at, graphics_size), 4);
    const auto assignments = slice(later, (clouds ? 0xf969 : 0xf9e1) - 0xf450, terrain.tiles.size());
    for (unsigned i = 0; i < terrain.tiles.size(); ++i)
        for (auto& pixel : terrain.tiles[i]) if (pixel) pixel |= (assignments[i] & 15U) * 16U;
    // Two equal halves of a 128x128 map, each assembled from eight 32x32 blocks.
    const unsigned source = kind == Kind::jet ? 0x441a : clouds ? 0x3c1a : land ? 0x1d3a : 0x6cfa;
    const unsigned layout = kind == Kind::above_clouds ? 0xfb09 : kind == Kind::boat ? 0xfac9 :
                            (land || kind == Kind::jet) ? 0xfae9 : 0xfaa9;
    terrain.cells.resize(128 * 128);
    for (unsigned block = 0; block < 8; ++block) {
        const unsigned at = layout - 0xf450 + block * 4;
        const auto cells = slice(graphics, source + read16(later, at), 1024);
        const unsigned destination = read16(later, at + 2) / 2;
        require(destination % 128 <= 96 && destination / 128 <= 32, "Invalid flight map block");
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; ++x) {
                const auto tile = cells[y * 32 + x];
                require(tile < terrain.tiles.size(), "Flight map uses unprepared graphics");
                terrain.cells[destination + y * 128 + x] = tile;
                terrain.cells[destination + y * 128 + x + 8192] = tile;
            }
    }
    terrain.wrap_x = terrain.wrap_y = kind != Kind::jet;
    terrain.lines.resize(224);
    const bool subtract = kind <= Kind::distant_airship;
    // InitHWRegsCredits supplies the initial operation; the later scene helper
    // and the fog bands select the shared replacement. Keep both source choices.
    terrain.mix = {
        static_cast<ColorBlend>(subtract ? FF6_ROM_VALUE_ENDING_FLIGHT_SUBTRACT_ARITHMETIC_0
                                        : FF6_ROM_VALUE_ENDING_FLIGHT_ADD_ARITHMETIC_0),
        static_cast<ColorBlend>(subtract ? FF6_ROM_VALUE_ENDING_FLIGHT_SUBTRACT_ARITHMETIC_0
                                        : FF6_ROM_VALUE_ENDING_FLIGHT_ADD_ARITHMETIC_0)};
    // The authored destination list comes from LoadCreditsBGPal. The bank
    // origin and 32-byte stride describe the source format, not palette content.
    for (const auto [index, address] : {
            std::pair{(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_0 - 0x3049U) / 32U, 0x95bcU},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_1 - 0x3049U) / 32U, 0x95dc},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_2 - 0x3049U) / 32U, 0x95fc},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_3 - 0x3049U) / 32U, 0x961c},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_4 - 0x3049U) / 32U, 0x963c},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_5 - 0x3049U) / 32U, 0x965c}})
        out.colors[index] = palette(data, address);
    if (kind == Kind::above_clouds || kind == Kind::jet)
        out.colors[(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_6 - 0x3049U) / 32U] =
            out.colors[(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_7 - 0x3049U) / 32U] = palette(data, 0x959c);
    const bool sky = kind == Kind::sea_airship || kind == Kind::land_birds || kind == Kind::big_airship;
    if (sky) {
        out.colors[(FF6_ROM_VALUE_ENDING_FLIGHT_BACKGROUND_PALETTE_ROUTES_8 - 0x3049U) / 32U] = palette(data, 0x953c);
        auto& layer = out.sky.emplace();
        layer.columns = 32; layer.rows = 16; layer.lines.resize(224); layer.palettes.resize(8);
        const auto sky_graphics = decompress_lzss(pack.get("world_graphics_world_backdrop_gfx_4bpp_lz"));
        layer.tiles = decode_tiles(slice(sky_graphics, 0, 0x1a20), 4);
        const auto map = decompress_lzss(pack.get("world_graphics_world_backdrop_tiles_scr_lz"));
        for (unsigned i = 0; i < 512; ++i) {
            const unsigned cell = read16(map, i * 2);
            require((cell & 1023U) < layer.tiles.size(), "Sky map uses unprepared graphics");
            layer.cells.push_back({cell & 1023U, (cell >> 10U) & 7U, 10,
                                   bool(cell & 0x4000U), bool(cell & 0x8000U)});
        }
        // The three authored display bands share the same sky/terrain boundary.
        static_assert(FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_0 == FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_1);
        static_assert(FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_0 == FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_2);
        for (unsigned y = FF6_ROM_VALUE_ENDING_CREDIT_HORIZON_0; y < 224; ++y)
            layer.lines[y].enabled = false;
    }
    const auto sine = pack.get("data_c2fc6d_btlgfx_lookup_tables");
    for (unsigned i = 0; i < 256; ++i) {
        const unsigned value = read16(sine, i * 2);
        out.sine[i] = static_cast<std::int16_t>(value < 32768 ? int(value) : int(value) - 65536);
    }
    constexpr std::array<unsigned, 10> cameras{0x8b3c, 0x8b42, 0, 0x8a70, 0x8ad9, 0x8b36, 0x8b39, 0x8afa, 0, 0};
    if (const auto start = cameras[static_cast<unsigned>(kind)])
        out.camera = decode_ending_camera(data.subspan(start - 0x8a70));
    std::vector<Tile> tiles(512);
    Bytes object_data(slice(graphics, 0, 0x17ba).begin(), slice(graphics, 0, 0x17ba).end());
    object_data.resize(0x17c0); // Last transferred tile is partial; untouched planes remain clear.
    auto objects = decode_tiles(object_data, 4);
    std::copy(objects.begin(), objects.end(), tiles.begin());
    if (kind == Kind::big_airship) {
        const auto big = decode_tiles(slice(graphics, 0x17ba, 0x580), 4);
        std::copy(big.begin(), big.end(), tiles.begin() + 192);
    }
    // LoadCreditsSpritePal supplies the independent sprite-bank assignment.
    for (const auto [index, address] : {
            std::pair{(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_0 - 0x3149U) / 32U, 0x97f4U},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_1 - 0x3149U) / 32U, 0x9794},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_2 - 0x3149U) / 32U, 0x97b4},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_3 - 0x3149U) / 32U, 0x97d4},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_4 - 0x3149U) / 32U, 0x9834},
            {(FF6_ROM_VALUE_ENDING_FLIGHT_SPRITE_PALETTE_ROUTES_5 - 0x3149U) / 32U, 0x9814}})
        out.object_colors[index] = palette(data, address);
    out.object_colors[text_palette_first] = palette(data, subtract ? 0x9744 : 0x9754);
    out.object_colors[text_palette_second] = palette(data, subtract ? 0x973c : 0x974c);
    auto add = [&](Object id, unsigned address) {
        const bool low_bank = address >> 16 == 0xc2;
        out.animations[static_cast<unsigned>(id)] = animation(low_bank ? data : later,
            low_bank ? 0x8a70 : 0xf450, address & 65535U, tiles);
    };
    if (kind == Kind::distant_airship) { add(Object::tiny_airship, 0xc28cdc); add(Object::tiny_shadow, 0xc28cf0); }
    if (kind == Kind::boat) { add(Object::boat_left, 0xcff809); add(Object::boat_right, 0xcff7fd); }
    if (kind == Kind::sea_airship) {
        add(Object::left_airship, 0xc28bb1); add(Object::shadow, 0xc28bec); add(Object::splash, 0xc28d0b);
    }
    if (kind == Kind::land_birds) { add(Object::right_airship, 0xc28c25); add(Object::flying_bird, 0xcff74f); }
    if (kind == Kind::big_airship) {
        add(Object::big_airship, 0xc28c60); add(Object::propeller_left, 0xc28d3e); add(Object::propeller_right, 0xc28d91);
    }
    if (kind == Kind::boat || kind == Kind::sea_airship || kind == Kind::land_birds) {
        const unsigned table = kind == Kind::boat ? 0xf7b5 : kind == Kind::sea_airship ? 0xf7cd : 0xf7e5;
        for (unsigned i = 0; i < 6; ++i) {
            const auto at = table - 0xf450 + i * 4;
            add(static_cast<Object>(static_cast<unsigned>(Object::bird0) + i), 0xcf0000U + read16(later, at));
            const auto pos = slice(later, at + 2, 2);
            out.birds[i] = {pos[0], pos[1]};
        }
    }
    const unsigned scene = kind == Kind::above_clouds ? 1 : kind == Kind::distant_airship ? 2 :
        kind == Kind::boat ? 3 : kind == Kind::sea_airship ? 4 : kind == Kind::land_birds ? 5 :
        kind == Kind::landscape ? 6 : kind == Kind::big_airship ? 7 : 0;
    if (scene) {
        auto font = decompress_lzss(pack.get("cutscene_assets_ending_font_2bpp_lz"));
        require(font.size() >= 0x400, "Truncated credits font");
        font.resize(0xc00);
        for (unsigned i = 0; i < 64; ++i) {
            const unsigned source_tile = slice(data, 0x9854 - 0x8a70 + i, 1)[0];
            require(source_tile < 64, "Invalid credits font remap");
            std::copy_n(font.begin() + source_tile * 16, 16, font.begin() + 0x400 + i * 32);
            std::fill_n(font.begin() + 0x410 + i * 32, 16, 0);
        }
        const auto decoded = decode_tiles(font, 2);
        std::copy(decoded.begin(), decoded.end(), tiles.begin() + 256);
        const auto small = pack.get("menu_graphics_small_font_2bpp");
        const auto small_tiles = decode_tiles(slice(small, 0x800, 0x200), 2);
        std::copy(small_tiles.begin(), small_tiles.end(), tiles.begin() + 480);
        tiles[506] = decode_tiles(slice(small, 0xc50, 16), 2)[0];
        tiles[507] = decode_tiles(slice(small, 0xff0, 16), 2)[0];
        out.pages = lettering(data, scene - 1, tiles);
    }
    return out;
}
} // namespace ff6
