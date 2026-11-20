#include "ff6/ending_characters.hpp"
#include "rom-materials/ending_large_text_waits.macros.inc"
#include "rom-materials/ending_large_text_vertical_origin.macros.inc"
#include "rom-materials/ending_large_text_vertical_speed.macros.inc"
#include "rom-materials/ending_character_composite_modes.macros.inc"
#include "rom-materials/ending_missing_name_glyph.macros.inc"
#include "rom-materials/ending_character_phase_waits.macros.inc"
#include "rom-materials/ending_character_scroll_limits.macros.inc"
#include "rom-materials/ending_character_scroll_origins.macros.inc"
#include "rom-materials/ending_character_caption_scroll_duration.macros.inc"
#include "rom-materials/ending_character_caption_scroll_mask.macros.inc"
#include "rom-materials/ending_character_as_lifetimes.macros.inc"
#include "rom-materials/ending_character_as_positions.macros.inc"
#include "rom-materials/ending_book_scroll_duration.macros.inc"
#include "rom-materials/ending_book_scroll_mask.macros.inc"
#include "rom-materials/ending_decoration_movement_byte_waits.macros.inc"
#include "rom-materials/ending_decoration_walking_wait.macros.inc"
#include "rom-materials/ending_decoration_late_positions.macros.inc"
#include "rom-materials/ending_decoration_glimmer_hold.macros.inc"
#include "rom-materials/ending_decoration_sparkle_start.macros.inc"
#include "rom-materials/ending_decoration_sparkle_counts.macros.inc"
#include "rom-materials/ending_decoration_sparkle_period.macros.inc"
#include "rom-materials/ending_decoration_sparkle_positions.macros.inc"
#include "rom-materials/ending_flight_white_fade_period.macros.inc"
#include "rom-materials/ending_character_decoration_fade_period.macros.inc"
#include "rom-materials/ending_book_background_fade_periods.macros.inc"
#include "rom-materials/ending_character_secondary_priority.macros.inc"
#include "rom-materials/ending_character_card_priority.macros.inc"
#include "rom-materials/ending_book_palette_routes.macros.inc"
#include "rom-materials/ending_character_palette_routes.macros.inc"
#include "rom-materials/ending_character_player_name_row.macros.inc"
#include "rom-materials/ending_brothers_second_character.macros.inc"
#include "rom-materials/ending_locke_celes_second_character.macros.inc"
#include "rom-materials/ending_character_caption_palette.macros.inc"
#include "rom-materials/ending_character_caption_center.macros.inc"
#include "rom-materials/ending_character_caption_priority.macros.inc"
#include "rom-materials/ending_character_decoration_extent.macros.inc"
#include <algorithm>

#include "rom-materials/ending_character_patch_geometry.macros.inc"
#include "rom-materials/ending_card_positions.macros.inc"
#include "rom-materials/ending_card_velocities.macros.inc"
#include "rom-materials/ending_decoration_initial_bytes.macros.inc"
#include "rom-materials/ending_decoration_initial_words.macros.inc"

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
// Movement phases come from the shared large-letter task's ROM operands.
// Each counter phase includes the visit which observes zero before advancing.
static_assert(FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_4 == FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_5);
static_assert(FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_0 == FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_1);
constexpr unsigned lettering_initial_wait = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_0;
constexpr unsigned lettering_move_in = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_1 + 1U;
constexpr unsigned lettering_second_start = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_1 + FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_2 + 3U;
constexpr unsigned lettering_move_out = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_3 + 1U;
constexpr unsigned lettering_pixel_period = 256U / (65536U - FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_SPEED_0);
constexpr unsigned lettering_second_wait = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_4;
constexpr unsigned lettering_lifetime = FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_1 + FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_2 + FF6_ROM_VALUE_ENDING_LARGE_TEXT_WAITS_3 + 4U;
static_assert(FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_ORIGIN_0 == FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_ORIGIN_1);
// Character-ending states share phase lengths, but each authored source is
// authenticated. Derive elapsed deadlines by summing those phases; do not
// retain a second hard-coded schedule. Scene control flow remains native.
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_5 == FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_0);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_6 == FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_1);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_8 == FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_3);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_11 == FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_3);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_12 == FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_4);
constexpr unsigned caption_first_in = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_0;
constexpr unsigned caption_first_out = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_0 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_1;
constexpr unsigned caption_second_in = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_7;
constexpr unsigned paired_name_gap = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_7 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_8;
constexpr unsigned caption_second_out = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_7 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_8 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_9;
constexpr unsigned single_fade_start = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_0 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_1 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_2;
constexpr unsigned paired_fade_start = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_5 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_6 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_7 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_8 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_9 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_10;
constexpr unsigned final_caption_wait = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_3;
constexpr unsigned final_fade_duration = FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_3 + FF6_ROM_VALUE_ENDING_CHARACTER_PHASE_WAITS_4;
// Background and caption tasks have separate authored cadence/lifetimes.
// Convert their selected masks to elapsed periods; retain signed word origins.
constexpr unsigned background_scroll_period = FF6_ROM_VALUE_ENDING_BOOK_SCROLL_MASK_0 + 1U;
constexpr unsigned caption_scroll_period = FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_SCROLL_MASK_0 + 1U;
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_AS_LIFETIMES_0 == caption_second_in);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_ORIGINS_0 >= 32768U);
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_ORIGINS_1 >= 32768U);
// Sparkle cadence is independent of caption timing, even when its first
// publication coincides. Counter-state visits remain native scheduling rules.
constexpr unsigned sparkle_start = FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_START_0;
constexpr unsigned sparkle_period = FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_PERIOD_0;
static_assert(sparkle_start == caption_first_out);
// Shared native constant pools serve multiple original fade creators.
// The separately authenticated sources must agree before the compiler folds them.
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_WHITE_FADE_PERIOD_0 == FF6_ROM_VALUE_ENDING_BOOK_BACKGROUND_FADE_PERIODS_0);
static_assert(FF6_ROM_VALUE_ENDING_FLIGHT_WHITE_FADE_PERIOD_0 == FF6_ROM_VALUE_ENDING_CHARACTER_DECORATION_FADE_PERIOD_0 + 1U);
// Palette destinations are selected ROM data. Convert their original addresses
// to native bank indices; buffer origins and record size are format metadata.
constexpr unsigned caption_background_palette = (FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_0 - 0x3049U) / 32U;
constexpr unsigned caption_object_palette = (FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_1 - 0x3149U) / 32U;
constexpr unsigned departing_caption_palette = (FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_2 - 0x3149U) / 32U;
constexpr unsigned decoration_palette = (FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_3 - 0x3149U) / 32U;
constexpr unsigned card_palette = (FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_4 - 0x3149U) / 32U;
// The two name rows carry tile attributes; palette bits choose a four-color
// group within the selected background bank. Keep the selection ROM-backed,
// while tile-word bit positions and palette sizes remain format metadata.
static_assert(FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_PALETTE_0 == FF6_ROM_VALUE_ENDING_CHARACTER_PALETTE_ROUTES_0);
static_assert((FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_PALETTE_1 & 0x1c00U) ==
              (FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_PALETTE_2 & 0x1c00U));
constexpr unsigned caption_first_color = ((FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_PALETTE_1 >> 10U) & 7U) * 4U;
static_assert(caption_first_color + 4U <= 16U);
using Kind = EndingCharacterScene;
using Decoration = EndingDecoration;
void require(bool value, const char* why) { if (!value) throw std::invalid_argument(why); }
constexpr std::array<std::uint8_t, 12> selectors{40, 50, 60, 64, 70, 80, 90, 100, 110, 120, 130, 135};
constexpr std::array<unsigned, 12> characters{
#include "rom-materials/ending_character_identities.inc"
};
unsigned index(Decoration value) { return static_cast<unsigned>(value); }
CutscenePalette palette(ByteView data, unsigned offset) {
    CutscenePalette result{};
    for (unsigned c = 0; c < 16; ++c) result[c] = read16(data, offset + c * 2) & 0x7fffU;
    return result;
}
// Resolve the animation's pointer-bearing data once. Playback sees only native
// resource identities, frame durations and decoded images, never ROM addresses.
EndingSpriteAnimation animation(ByteView data, unsigned base, unsigned address,
                                std::span<const Tile> tiles) {
    require(address >= base, "Invalid ending animation pointer");
    EndingSpriteAnimation out;
    for (unsigned n = 0; n < 85; ++n) {
        const auto at = address - base + n * 3;
        const unsigned pointer = read16(data, at);
        const auto duration = slice(data, at + 2, 1)[0];
        require(pointer >= base, "Invalid ending image pointer");
        std::size_t cursor = pointer - base;
        out.groups.push_back(decode_cutscene_sprite_group(data, cursor, tiles));
        out.frames.push_back({n, duration});
        if (duration >= 254) return out;
    }
    throw std::invalid_argument("Unterminated ending object animation");
}
TileLayer player_name(const ContentPack& pack, const std::array<std::uint8_t, 6>& name) {
    const auto glyphs = decode_glyphs(pack.get("large_font_large_font_rows_1bpp"));
    const auto widths = pack.get("font_tables_widths");
    TileLayer layer;
    // A padded native canvas for six variable-width glyphs and their shadows.
    // Blank capacity is transparent; placement and palette choices are ROM inputs.
    layer.columns = 12; layer.rows = 2;
    layer.tiles.resize(24); layer.cells.resize(24); layer.palettes.resize(1);
    layer.wrap_x = layer.wrap_y = false;
    // Original row attributes select priority; rank values belong to the
    // native BG3 compositor rather than the authored caption data.
    for (unsigned i = 0; i < 24; ++i)
        layer.cells[i] = {i, 0, static_cast<std::uint16_t>(
            1 + 13 * FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_PRIORITY_0)};
    unsigned column = 0;
    auto set = [&](unsigned x, unsigned y, unsigned color) {
        require(x < 96 && y < 16, "Ending player name exceeds its area");
        layer.tiles[y / 8 * 12 + x / 8][y % 8 * 8 + x % 8] |= static_cast<std::uint8_t>(color);
    };
    for (const auto letter : name) {
        if (letter == 255) break;
        require(letter >= 128, "Invalid ending player name glyph");
        const auto& glyph = glyphs.at(letter - 128);
        for (unsigned y = 0; y < 11; ++y)
            for (unsigned x = 0; x < 12; ++x)
                if (glyph[y] & (1U << (11 - x))) {
                    // CopyBigLetterGfx starts four rows into the two-tile line;
                    // the second bitplane is its one-pixel down/right shadow.
                    set(column + x, y + 4, 1);
                    set(column + x + 1, y + 5, 2);
                }
        column += slice(widths, letter - 96, 1)[0];
    }
    layer.lines.resize(224);
    for (auto& line : layer.lines) {
        line.offset_x = static_cast<int>(column / 2) - FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_CENTER_0;
        // Tilemap cells are two bytes, rows have 32 cells, and tiles are 8 pixels.
        line.offset_y = 1 - static_cast<int>(FF6_ROM_VALUE_ENDING_CHARACTER_PLAYER_NAME_ROW_0 / 8U);
    }
    return layer;
}
}
bool is_ending_character_segment(std::uint8_t selector) {
    return std::find(selectors.begin(), selectors.end(), selector) != selectors.end();
}
EndingCharacterScene ending_character_segment(std::uint8_t selector) {
    const auto found = std::find(selectors.begin(), selectors.end(), selector);
    require(found != selectors.end(), "Ending segment is not a character cinematic");
    return static_cast<Kind>(found - selectors.begin());
}
std::array<std::uint8_t, 6> ending_player_name(const Roster& roster, unsigned character) {
    for (const auto& entry : roster.characters)
        if (entry.properties == character) return entry.name;
    std::array<std::uint8_t, 6> unknown{};
    // The absent-character caption is localized ROM text, including its fallback glyph.
    unknown.fill(FF6_ROM_VALUE_ENDING_MISSING_NAME_GLYPH_0);
    return unknown;
}
EndingCharacterAssets load_ending_character_assets(const ContentPack& pack, Kind kind, const Roster& roster) {
    EndingCharacterAssets out;
    out.kind = kind;
    out.background = load_ending_book_assets(pack);
    const auto data = pack.get("data_c28a70_ending_animation_and_credits");
    const auto later = pack.get("data_cff450_ending_animation");
    const auto graphics = decompress_lzss(pack.get("cutscene_assets_ending1_bin_lz"));
    std::vector<Tile> tiles(512);
    const auto font = decode_tiles(decompress_lzss(pack.get("cutscene_assets_ending_font_2bpp_lz")), 2);
    std::copy(font.begin(), font.end(), tiles.begin() + 256);
    auto load = [&](const char* id, unsigned bytes, unsigned depth = 4, unsigned first = 0) {
        auto decoded = decompress_lzss(pack.get(id));
        require(decoded.size() >= bytes, "Truncated ending object graphics");
        decoded.resize(bytes);
        // The last ending3 tile has only two planes transferred. Its remaining
        // planes stay clear; do not read beyond the authored graphics extent.
        const unsigned tile_bytes = depth * 8;
        decoded.resize((bytes + tile_bytes - 1) / tile_bytes * tile_bytes);
        const auto images = decode_tiles(decoded, depth);
        require(first + images.size() <= 256, "Ending decoration overlaps lettering");
        std::copy(images.begin(), images.end(), tiles.begin() + first);
    };
    if (kind == Kind::shadow) load("cutscene_assets_ending2_4bpp_lz", 0x380);
    else if (kind == Kind::brothers) load("cutscene_assets_ending4_4bpp_lz", 0x800);
    else if (kind == Kind::gau) load("cutscene_assets_ending5_2bpp_lz", 0xc0, 2);
    else if (kind != Kind::locke_celes && kind != Kind::strago)
        load("cutscene_assets_ending3_4bpp_lz", FF6_ROM_VALUE_ENDING_CHARACTER_DECORATION_EXTENT_0);
    if (kind == Kind::umaro) load("cutscene_assets_ending4_4bpp_lz", 0x800, 4, 128);
    auto patch = [&](unsigned bytes, unsigned rows, unsigned source, unsigned destination) {
        apply_ending_tile_patch(out.background.illustration,
                               decode_ending_tile_patch(graphics, bytes, rows, source, destination));
    };
    switch (kind) {
    case Kind::cyan:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_0, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_1,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_2, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_3);
        break;
    case Kind::mog:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_4, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_5,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_6, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_7);
        break;
    case Kind::gogo:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_8, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_9,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_10, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_11);
        break;
    case Kind::gau:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_12, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_13,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_14, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_15);
        break;
    case Kind::terra:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_16, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_17,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_18, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_19);
        break;
    case Kind::locke_celes:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_20, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_21,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_22, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_23);
        break;
    case Kind::relm:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_24, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_25,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_26, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_27);
        break;
    case Kind::strago:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_28, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_29,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_30, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_31);
        break;
    case Kind::umaro:
        patch(FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_32, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_33,
              FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_34, FF6_ROM_VALUE_ENDING_CHARACTER_PATCH_GEOMETRY_35);
        break;
    default: break;
    }
    const unsigned palette_address = kind == Kind::gogo ? 0x957c :
        (kind == Kind::cyan || kind == Kind::terra || kind == Kind::relm) ? 0x955c : 0x9774;
    out.decoration_colors = palette(data, palette_address - 0x8a70);
    auto add = [&](Decoration id, unsigned address) {
        const auto bank = address >> 16;
        const auto bytes = bank == 0xc2 ? data : bank == 0xcf ? later :
            pack.get("data_d8e8c8_menu_animation_and_letters");
        const unsigned base = bank == 0xc2 ? 0x8a70 : bank == 0xcf ? 0xf450 : 0xe8c8;
        out.animations[index(id)] = animation(bytes, base, address & 0xffffU, tiles);
    };
    switch (kind) {
    case Kind::shadow: add(Decoration::apple, 0xc29497); break;
    case Kind::cyan: add(Decoration::sword, 0xcff5fa); break;
    case Kind::brothers: add(Decoration::coin, 0xd8ec93); break;
    case Kind::mog: add(Decoration::dancer, 0xcff515); break;
    case Kind::gogo: add(Decoration::glimmer, 0xcff450); break;
    case Kind::gau: add(Decoration::eyes, 0xcff647); break;
    case Kind::terra: case Kind::relm: add(Decoration::pendant, 0xcff5b4); break;
    case Kind::setzer: add(Decoration::cards, 0xc292fa); break;
    case Kind::umaro: add(Decoration::skull, 0xcff56e); break;
    default: break;
    }
    if (kind == Kind::mog || kind == Kind::umaro) {
        constexpr std::array<unsigned, 7> addresses{0xcff458, 0xcff473, 0xcff47c,
                                                   0xcff47f, 0xcff485, 0xcff48b, 0xcff491};
        for (unsigned i = 0; i < addresses.size(); ++i)
            add(static_cast<Decoration>(index(Decoration::moogle_idle) + i), addresses[i]);
        for (unsigned i = 0; i < 16; ++i) {
            const int value = slice(later, 0xf55e - 0xf450 + i, 1)[0];
            out.jump[i] = value < 128 ? value : value - 256;
        }
    }
    auto name = [&](unsigned character) {
        EndingCharacterName resolved;
        resolved.player = player_name(pack, ending_player_name(roster, character));
        for (unsigned part = 0; part < 2; ++part) {
            const unsigned at = 0x8de4 - 0x8a70 + character * 6 + part * 3;
            const auto stream = animation(data, 0x8a70, read16(data, at), tiles);
            require(stream.frames.size() == 1 && stream.frames.front().duration == 254,
                    "Unexpected animated ending character lettering");
            resolved.full[part] = stream.groups.front();
            resolved.x[part] = slice(data, at + 2, 1)[0];
        }
        out.names.push_back(std::move(resolved));
    };
    name(characters.at(static_cast<unsigned>(kind)));
    if (kind == Kind::brothers) name(FF6_ROM_VALUE_ENDING_BROTHERS_SECOND_CHARACTER_0);
    if (kind == Kind::locke_celes) name(FF6_ROM_VALUE_ENDING_LOCKE_CELES_SECOND_CHARACTER_0);
    out.as = animation(data, 0x8a70, 0x8e38, tiles).groups.front();
    return out;
}
void EndingCharacters::Palette::to(const CutscenePalette& next, std::uint8_t period) {
    target = next;
    fade = CutscenePaletteFade{period};
}
bool EndingCharacters::paired() const { return assets_.kind == Kind::brothers || assets_.kind == Kind::locke_celes; }
unsigned EndingCharacters::fade_start() const { return paired() ? paired_fade_start : single_fade_start; }
bool EndingCharacters::complete() const { return updates_ >= fade_start() + final_fade_duration; }
void EndingCharacters::set_animation(Object& object, Decoration id) {
    const auto& frames = assets_.animations[index(id)].frames;
    require(!frames.empty(), "Missing ending object animation");
    object.animation = id;
    object.cursor = {0, frames.front().duration};
}
void EndingCharacters::spawn(Decoration id, int x, int y, bool scroll, unsigned priority) {
    Object object{};
    set_animation(object, id);
    object.x = static_cast<std::uint16_t>(x * 256);
    object.y = static_cast<std::uint16_t>(y * 256);
    object.scroll = scroll; object.priority = priority;
    decorations_.push_back(object);
}
EndingCharacters::EndingCharacters(EndingCharacterAssets assets) : assets_(std::move(assets)) {
    require(assets_.names.size() == (paired() ? 2U : 1U), "Invalid ending character name count");
    for (const auto& stream : assets_.animations) {
        if (stream.frames.empty()) continue;
        require(stream.frames.back().duration >= 254, "Unterminated ending decoration");
        for (const auto& frame : stream.frames)
            require(frame.resource < stream.groups.size(), "Invalid ending decoration frame");
    }
    for (unsigned i = 0; i < 8; ++i) background_[i].to(assets_.background.colors[i], FF6_ROM_VALUE_ENDING_BOOK_BACKGROUND_FADE_PERIODS_0);
    const auto kind = assets_.kind;
    if (kind != Kind::gogo && kind != Kind::locke_celes && kind != Kind::strago)
        objects_[kind == Kind::setzer ? card_palette : decoration_palette].to(assets_.decoration_colors,
            kind == Kind::cyan || kind == Kind::terra || kind == Kind::relm ?
                FF6_ROM_VALUE_ENDING_CHARACTER_DECORATION_FADE_PERIOD_0 : FF6_ROM_VALUE_ENDING_FLIGHT_WHITE_FADE_PERIOD_0);
    switch (kind) {
    case Kind::shadow:
        spawn(Decoration::apple, FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_0,
              FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_1, true);
        break;
    case Kind::brothers:
        spawn(Decoration::coin, FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_2,
              FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_3, false, FF6_ROM_VALUE_ENDING_CHARACTER_SECONDARY_PRIORITY_0);
        decorations_.back().vx = FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_5;
        decorations_.back().counter = FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_12;
        decorations_.back().movement = Movement::coin;
        break;
    case Kind::gogo:
        spawn(Decoration::glimmer, FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_4,
              FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_5, true);
        break;
    case Kind::mog:
        spawn(Decoration::dancer, FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_6,
              FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_7, true);
        break;
    case Kind::umaro:
        spawn(Decoration::skull, FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_8,
              FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_9, true);
        break;
    case Kind::setzer:
        // C3/E8E3 writes X twice for the first card, leaving its initialized Y.
        // ROM-generated data literals preserve that authored result and the
        // exact native compiler output. The shared card motion stays native.
        for (const auto& card : {std::array<int, 4>{
                 FF6_ROM_VALUE_ENDING_CARD_POSITIONS_0, 0,
                 FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_0, FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_1}, {
                 FF6_ROM_VALUE_ENDING_CARD_POSITIONS_1, FF6_ROM_VALUE_ENDING_CARD_POSITIONS_2,
                 FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_2, FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_3}, {
                 FF6_ROM_VALUE_ENDING_CARD_POSITIONS_3, FF6_ROM_VALUE_ENDING_CARD_POSITIONS_4,
                 FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_4, FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_5}, {
                 FF6_ROM_VALUE_ENDING_CARD_POSITIONS_5, FF6_ROM_VALUE_ENDING_CARD_POSITIONS_6,
                 FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_6, FF6_ROM_VALUE_ENDING_CARD_VELOCITIES_7}}) {
            spawn(Decoration::cards, card[0], card[1], false, FF6_ROM_VALUE_ENDING_CHARACTER_CARD_PRIORITY_0);
            decorations_.back().vx = card[2]; decorations_.back().vy = card[3];
        }
        break;
    case Kind::strago:
        require(!assets_.background.animations[0].empty(), "Missing Strago book animation");
        book_ = {0, assets_.background.animations[0].front().duration};
        break;
    default: break;
    }
    if (kind == Kind::mog || kind == Kind::umaro) {
        // C3/EA24 and C3/EA7C author each mini-moogle's horizontal position.
        // The values are private ROM inputs; task behavior remains native code.
        constexpr std::array<int, 4> mog_x{
#include "rom-materials/ending_mog_positions.inc"
        }, umaro_x{
#include "rom-materials/ending_umaro_positions.inc"
        };
        constexpr std::array<unsigned, 4> arrival_waits{
            FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_0,
            FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_1,
            FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_2,
            FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_3
        };
        for (unsigned i = 0; i < 4; ++i) {
            const bool walk = kind == Kind::umaro;
            spawn(walk ? Decoration::moogle_walk : Decoration::moogle_idle,
                  walk ? umaro_x[i] : mog_x[i],
                  walk ? FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_10 : FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_BYTES_11, true, FF6_ROM_VALUE_ENDING_CHARACTER_SECONDARY_PRIORITY_0);
            auto& object = decorations_.back();
            object.movement = walk ? Movement::arrival : Movement::waiting;
            object.vx = walk ? FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_6 : 0;
            object.counter = walk ? arrival_waits[i] : FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_4;
            object.final_pose = i;
        }
    }
}
void EndingCharacters::advance_object(Object& object) {
    if (!object.alive) return;
    switch (object.movement) {
    case Movement::coin:
        if (object.counter) --object.counter;
        else object.vx = 0;
        break;
    case Movement::waiting:
        if (!object.counter) {
            object.movement = Movement::jumping; object.counter = FF6_ROM_VALUE_ENDING_DECORATION_MOVEMENT_BYTE_WAITS_0;
            set_animation(object, Decoration::moogle_jump);
        }
        --object.counter;
        break;
    case Movement::jumping:
        if (!object.counter) { object.movement = Movement::resting; object.counter = FF6_ROM_VALUE_ENDING_DECORATION_MOVEMENT_BYTE_WAITS_1; }
        --object.counter;
        object.y = static_cast<std::uint16_t>(static_cast<int>(object.y) + assets_.jump[object.jump++ % 16] * 256);
        break;
    case Movement::resting:
        if (!object.counter) {
            object.movement = Movement::walking; object.counter = FF6_ROM_VALUE_ENDING_DECORATION_WALKING_WAIT_0;
            object.vx = FF6_ROM_VALUE_ENDING_DECORATION_INITIAL_WORDS_6;
            set_animation(object, Decoration::moogle_walk);
        }
        --object.counter;
        break;
    case Movement::arrival:
        if (!object.counter) {
            object.movement = Movement::stopped; object.vx = 0;
            set_animation(object, static_cast<Decoration>(index(Decoration::moogle_stop0) + object.final_pose));
        } else --object.counter;
        break;
    default: break;
    }
    if (object.remove_on_hold && object.cursor.wait == 254) { object.alive = false; return; }
    const auto& animation = assets_.animations[index(object.animation)];
    advance_cutscene_animation(object.cursor, animation.frames);
    object.x = static_cast<std::uint16_t>(static_cast<int>(object.x) + object.vx);
    object.y = static_cast<std::uint16_t>(static_cast<int>(object.y) + object.vy);
}
void EndingCharacters::caption_in() {
    background_[caption_background_palette].to(assets_.background.lettering, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    objects_[caption_object_palette].to(assets_.background.lettering, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
}
void EndingCharacters::caption_out() {
    background_[caption_background_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0); objects_[caption_object_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    objects_[departing_caption_palette].to(assets_.background.lettering, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
}
bool EndingCharacters::advance() {
    if (complete()) return false;
    const auto kind = assets_.kind;
    if (updates_ == caption_first_in) caption_in();
    if (updates_ == caption_first_out) {
        caption_out();
        if (kind == Kind::gau) spawn(Decoration::eyes, FF6_ROM_VALUE_ENDING_DECORATION_LATE_POSITIONS_0, FF6_ROM_VALUE_ENDING_DECORATION_LATE_POSITIONS_1, true);
        if (kind == Kind::terra) spawn(Decoration::pendant, FF6_ROM_VALUE_ENDING_DECORATION_LATE_POSITIONS_2, FF6_ROM_VALUE_ENDING_DECORATION_LATE_POSITIONS_3, false);
    }
    if (paired() && updates_ == caption_second_in) { objects_[departing_caption_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0); caption_in(); }
    if (paired() && updates_ == caption_second_out) caption_out();
    if (updates_ == fade_start()) {
        for (unsigned i : {
                (FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_0 - 0x3049U) / 32U,
                (FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_1 - 0x3049U) / 32U,
                (FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_2 - 0x3049U) / 32U,
                (FF6_ROM_VALUE_ENDING_BOOK_PALETTE_ROUTES_3 - 0x3049U) / 32U}) background_[i].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
        if (kind != Kind::setzer) objects_[decoration_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    }
    if (updates_ == fade_start() + final_caption_wait) {
        objects_[departing_caption_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
        if (kind == Kind::setzer) objects_[card_palette].to({}, FF6_ROM_VALUE_ENDING_FLIGHT_SHARED_FADE_PERIODS_0);
    }
    if (kind == Kind::gogo) {
        // C3/E44A loads, rather than stores, the proposed initial delay. Its
        // scratch counter is not initialized by ResetTasks. Native presentation
        // starts deterministically at zero; residue-dependent phase is deferred.
        if (updates_ == 1) objects_[decoration_palette].to(assets_.decoration_colors, FF6_ROM_VALUE_ENDING_CHARACTER_DECORATION_FADE_PERIOD_0);
        if (updates_ == FF6_ROM_VALUE_ENDING_DECORATION_GLIMMER_HOLD_0 + 3U) objects_[decoration_palette].to({}, FF6_ROM_VALUE_ENDING_CHARACTER_DECORATION_FADE_PERIOD_0);
    }
    if ((kind == Kind::cyan || kind == Kind::relm) && updates_ >= sparkle_start) {
        const unsigned step = updates_ - sparkle_start;
        const unsigned count = kind == Kind::cyan ? FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_COUNTS_1 : FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_COUNTS_0;
        if (step % sparkle_period == 0 && step / sparkle_period < count) {
            spawn(kind == Kind::cyan ? Decoration::sword : Decoration::pendant,
                  (kind == Kind::cyan ? FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_POSITIONS_0 : FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_POSITIONS_1) -
                      static_cast<int>(step / sparkle_period) * 4,
                  kind == Kind::cyan ? FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_POSITIONS_2 : FF6_ROM_VALUE_ENDING_DECORATION_SPARKLE_POSITIONS_3, true, FF6_ROM_VALUE_ENDING_CHARACTER_SECONDARY_PRIORITY_0);
            decorations_.back().remove_on_hold = true;
        }
    }
    if (kind == Kind::strago) {
        const auto& stream = assets_.background.animations[0];
        advance_cutscene_animation(book_, stream);
        apply_ending_tile_patch(assets_.background.illustration,
                               assets_.background.patches.at(stream[book_.frame].resource));
    }
    for (auto& channel : background_) channel.advance();
    for (auto& channel : objects_) channel.advance();
    for (auto& object : decorations_) advance_object(object);
    ++updates_;
    return true;
}
int EndingCharacters::scroll() const {
    const auto kind = assets_.kind;
    if (kind == Kind::brothers || kind == Kind::strago) return 0;
    const bool continuous = kind == Kind::cyan || kind == Kind::gogo || kind == Kind::setzer;
    const unsigned initial = std::min<unsigned>(updates_, continuous ? FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_LIMITS_0 : FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_LIMITS_1) / background_scroll_period;
    const unsigned final = !continuous && updates_ > fade_start() ?
        std::min<unsigned>(updates_ - fade_start(), FF6_ROM_VALUE_ENDING_BOOK_SCROLL_DURATION_0) / background_scroll_period : 0;
    return static_cast<int>(initial + final) + (kind == Kind::cyan ? static_cast<int>(FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_ORIGINS_0) - 65536 : kind == Kind::locke_celes ? static_cast<int>(FF6_ROM_VALUE_ENDING_CHARACTER_SCROLL_ORIGINS_1) - 65536 : 0);
}
RasterScene EndingCharacters::scene() const {
    RasterScene result;
    result.layers = {assets_.background.illustration, assets_.background.shading};
    for (unsigned layer = 0; layer < 2; ++layer) {
        for (unsigned p = 0; p < 8; ++p) result.layers[layer].palettes[p] = background_[p].colors;
        if (!layer) for (auto& line : result.layers[layer].lines) line.offset_x = scroll();
    }
    const unsigned caption = paired() && updates_ > caption_second_in ? 1 : 0;
    auto player = assets_.names[caption].player;
    for (unsigned c = 0; c < 4; ++c) player.palettes[0][c] = background_[caption_background_palette].colors[caption_first_color + c];
    const unsigned caption_start = caption ? caption_second_out : caption_first_out;
    const unsigned down = updates_ > caption_start ? std::min<unsigned>(updates_ - caption_start, FF6_ROM_VALUE_ENDING_CHARACTER_CAPTION_SCROLL_DURATION_0) / caption_scroll_period : 0;
    for (auto& line : player.lines) line.offset_y -= static_cast<int>(down);
    result.layers.push_back(std::move(player));
    result.objects.emplace();
    result.objects->lines.resize(224);
    auto draw = [&](const std::vector<CutsceneSprite>& group, int x, int y) {
        for (const auto& piece : group) {
            auto image = piece.image;
            image.x = wrap8(static_cast<unsigned>(image.x + x));
            image.y = wrap8(static_cast<unsigned>(image.y + y)) - 1;
            if (image.y >= 224) image.y -= 256;
            image.colors = objects_[piece.palette].colors;
            if (piece.palette >= 4) image.mix = {ending_composition.covered, ending_composition.uncovered};
            result.objects->sprites.push_back(std::move(image));
        }
    };
    auto decorations = [&](unsigned priority) {
        for (const auto& object : decorations_) {
            if (!object.alive || object.priority != priority) continue;
            const auto& stream = assets_.animations[index(object.animation)];
            draw(stream.groups.at(stream.frames.at(object.cursor.frame).resource),
                 static_cast<int>(object.x / 256) - (object.scroll ? scroll() : 0), object.y / 256);
        }
    };
    // Original task priorities select these groups; the compositor stays native.
    decorations(FF6_ROM_VALUE_ENDING_CHARACTER_CARD_PRIORITY_0);
    if (!paired() || updates_ < paired_name_gap || updates_ > caption_second_out) {
        const unsigned name = paired() && updates_ > caption_second_out ? 1 : 0;
        const unsigned elapsed = name ? updates_ - caption_second_out : updates_;
        const unsigned delay = name ? lettering_second_wait : lettering_initial_wait;
        const unsigned motion = std::min(elapsed > delay + 1 ? elapsed - delay - 1 : 0U, lettering_move_in) +
            std::min(elapsed > delay + lettering_second_start ?
                     elapsed - delay - lettering_second_start : 0U, lettering_move_out);
        if (elapsed < delay + lettering_lifetime) {
            for (unsigned part = 0; part < 2; ++part)
                draw(assets_.names[name].full[part], assets_.names[name].x[part],
                     FF6_ROM_VALUE_ENDING_LARGE_TEXT_VERTICAL_ORIGIN_0 -
                     static_cast<int>((motion + lettering_pixel_period - 1U) / lettering_pixel_period));
        }
    }
    decorations(FF6_ROM_VALUE_ENDING_CHARACTER_SECONDARY_PRIORITY_0);
    decorations(FF6_ROM_VALUE_ENDING_CHARACTER_DEFAULT_PRIORITY_0);
    if (updates_ < FF6_ROM_VALUE_ENDING_CHARACTER_AS_LIFETIMES_0 || (paired() && updates_ >= FF6_ROM_VALUE_ENDING_CHARACTER_AS_LIFETIMES_0 && updates_ < FF6_ROM_VALUE_ENDING_CHARACTER_AS_LIFETIMES_1))
        draw(assets_.as, FF6_ROM_VALUE_ENDING_CHARACTER_AS_POSITIONS_0, FF6_ROM_VALUE_ENDING_CHARACTER_AS_POSITIONS_1);
    result.intensity.assign(224, complete() ? 0 : 15);
    return result;
}
} // namespace ff6
