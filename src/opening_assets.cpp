#include "ff6/opening.hpp"
#include <algorithm>
#include <map>
#include "rom-materials/opening_town_blank_word.macros.inc"
#include "rom-materials/opening_town_blank_extent.macros.inc"
#include "rom-materials/opening_town_map_priority.macros.inc"

namespace ff6 {
namespace {
void require(bool condition, const char* why) {
    if (!condition)
        throw std::invalid_argument(why);
}
OpeningPalette palette(ByteView bytes, std::uint8_t residue) {
    require(bytes.size() == 32, "Invalid opening palette extent");
    OpeningPalette result;
    result.particle_fraction = residue;
    for (unsigned i = 0; i < 16; ++i)
        result.colors[i] = read16(bytes, i * 2) & 0x7fffU;
    return result;
}
} // namespace

OpeningAssets load_opening_assets(const ContentPack& pack) {
    OpeningAssets assets;
    assets.landscape = load_opening_landscape(pack);
    const auto data = decompress_lzss(pack.get("cutscene_assets_title_opening_bin_lz"));
    const auto install = [](Bytes& target, std::size_t offset, ByteView bytes) {
        require(offset <= target.size() && bytes.size() <= target.size() - offset,
                "Opening graphics exceed their typed pool");
        std::ranges::copy(bytes, target.begin() + static_cast<std::ptrdiff_t>(offset));
    };
    Bytes town_tiles(1024 * 32), font_tiles(1024 * 16), object_tiles(256 * 32);
    // The town's lower character range aliases the landscape's interleaved
    // map/texel data. Reconstruct the prepared portion as a typed resource;
    // do not assume those aliased characters were cleared. Tile zero is used
    // in the otherwise empty bottom rows of the town map.
    Bytes aliases;
    for (unsigned i = 0; i < 96 * 64; ++i) {
        aliases.push_back(static_cast<std::uint8_t>(assets.landscape.cells[8192 + i]));
        aliases.push_back(assets.landscape.tiles[128 + i / 64][i % 64]);
    }
    install(town_tiles, 0, aliases);
    install(town_tiles, 512 * 32, slice(data, 0x53c0, 0x2200));
    install(font_tiles, 0, slice(data, 0x75c0, 0x370));
    install(object_tiles, 64 * 32, slice(data, 0, 0x940));
    assets.object_tiles = decode_tiles(object_tiles, 4);
    assets.town.columns = assets.town.rows = assets.text.columns = assets.text.rows = 32;
    assets.town.tiles = decode_tiles(town_tiles, 4);
    assets.text.tiles = decode_tiles(font_tiles, 2);
    for (auto* layer : {&assets.town, &assets.text}) {
        layer->lines.resize(224);
        layer->palettes.resize(8);
        layer->cells.resize(1024);
    }
    assets.town.operand = true;
    for (unsigned i = 0; i < 1024; ++i) {
        const unsigned word = i < FF6_ROM_VALUE_OPENING_TOWN_BLANK_EXTENT_0
            ? FF6_ROM_VALUE_OPENING_TOWN_BLANK_WORD_0
            : wrap16(read16(data, 0x23c0 + i * 2) + FF6_ROM_VALUE_OPENING_TOWN_MAP_PRIORITY_0 * 8192U);
        require((word & 1023U) < 384 || (word & 1023U) >= 512,
                "Opening town references an unprepared tile alias");
        assets.town.cells[i] = {word & 1023U, (word >> 10U) & 7U,
                                static_cast<std::uint16_t>(9U + FF6_ROM_VALUE_OPENING_TOWN_MAP_PRIORITY_0),
                                (word & 16384U) != 0,
                                (word & 32768U) != 0};
        assets.text.cells[i] = {0, 0, 1};
    }
    const auto landscapes = pack.get("opening_landscape_palettes");
    const auto title_colors = pack.get("title_palettes");
    const auto neutral = pack.get("cutscene_neutral_palettes");
    require(landscapes.size() == 96 && title_colors.size() == 288 && neutral.size() == 64,
            "Invalid opening shared palettes");
    // The residue arguments are low address bytes of these source resources
    // (BlackPal 7465, TownPal1 7C43, TownPal2 6498, ArmorPal 6470,
    // BG palettes 6410/6430/6450, FontPal 6400, SnowPal 6490). These are
    // address/alias metadata, not an embedded palette or particle preset.
    assets.black = palette(neutral.last(32), 0x65);
    assets.town_light = palette(title_colors.last(32), 0x43);
    assets.town_dark = palette(pack.get("title_town_dark_palette"), 0x98);
    assets.armor_palette = palette(pack.get("opening_armor_palette"), 0x70);
    for (unsigned i = 0; i < 3; ++i)
        assets.landscape_palettes[i] =
            palette(landscapes.subspan(i * 32, 32), static_cast<std::uint8_t>(0x10 + i * 32));
    // LoadPal consumes sixteen colors even when a declaration is shorter. The
    // neighboring data supplies the remaining colors; nothing is zero-filled.
    Bytes font(pack.get("opening_font_palette").begin(), pack.get("opening_font_palette").end());
    require(font.size() == 16, "Invalid opening font palette");
    append(font, landscapes.first(16));
    assets.font = palette(font, 0);
    Bytes snow_colors(pack.get("opening_snow_palette").begin(),
                      pack.get("opening_snow_palette").end());
    require(snow_colors.size() == 8, "Invalid opening snow palette");
    append(snow_colors, pack.get("title_town_dark_palette").first(24));
    assets.snow_palette = palette(snow_colors, 0x90);
    const auto random = pack.get("encounter_rng_tbl_bin");
    require(random.size() == 256, "Invalid cutscene random table");
    std::ranges::copy(random, assets.random.begin());

    // Resolve bounded legacy data pointers to resource indices. The resulting
    // animations and captions contain no original executable/data addresses.
    const auto armor = pack.get("opening_armor_animation");
    const auto parts = pack.get("opening_armor_parts");
    const auto snow = pack.get("opening_snow_animation");
    const auto selection = pack.get("opening_snow_selection");
    require(armor.size() == 472 && parts.size() == 24 && snow.size() == 76 &&
                selection.size() == 16,
            "Invalid opening animation resource extent");
    for (unsigned i = 0; i < 5; ++i)
        assets.armor_offsets.push_back({armor[i * 2], armor[i * 2 + 1]});
    require(assets.armor_offsets.back().duration == 255, "Unterminated armor offset cycle");
    for (unsigned i = 0; i < 4; ++i)
        require(assets.armor_offsets[i].duration != 0 && assets.armor_offsets[i].duration < 254,
                "Invalid armor offset duration");
    std::map<unsigned, std::size_t> groups;
    const auto resolve_group = [&](ByteView bytes, unsigned base, unsigned pointer) {
        auto it = groups.find(pointer);
        if (it != groups.end())
            return it->second;
        require(pointer >= base && pointer - base < bytes.size(),
                "Unresolved opening sprite pointer");
        std::size_t at = pointer - base;
        const auto index = assets.sprite_frames.size();
        assets.sprite_frames.push_back(decode_cutscene_sprite_group(bytes, at, assets.object_tiles));
        groups.emplace(pointer, index);
        return index;
    };
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned pointer = read16(parts, i * 4);
        require(pointer >= 0x6031 && pointer < 0x61ff, "Invalid armor animation pointer");
        auto& part = assets.armor[i];
        part.offset_x = parts[i * 4 + 2];
        part.offset_y = parts[i * 4 + 3];
        for (std::size_t at = pointer - 0x6027;; at += 3) {
            const auto frame = slice(armor, at, 3);
            part.animation.push_back({resolve_group(armor, 0x6027, read16(frame, 0)), frame[2]});
            if (frame[2] == 255)
                break;
            require(frame[2] > 0 && frame[2] < 254 && part.animation.size() < 85,
                    "Invalid looping armor animation");
        }
    }
    for (unsigned i = 0; i < 7; ++i) {
        const auto record = snow.subspan(i * 3, 3);
        require(record[2] == 254, "Snow animation is not held");
        assets.snow[i].push_back({resolve_group(snow, 0x63b4, read16(record, 0)), 254});
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned pointer = read16(selection, i * 2);
        require(pointer >= 0x63b4 && pointer <= 0x63c6 && (pointer - 0x63b4) % 3 == 0,
                "Invalid snow selection");
        assets.snow_selection[i] = (pointer - 0x63b4) / 3;
    }
    const auto captions = pack.get("opening_credit_text");
    const auto timeline = pack.get("opening_credit_animation");
    const auto scrolls = pack.get("opening_credit_scrolls");
    require(captions.size() == 1068 && timeline.size() == 324 && scrolls.size() == 32,
            "Invalid credit data extent");
    for (unsigned i = 0; i < 32; ++i)
        assets.caption_scrolls[i / 4][i % 4] = scrolls[i];
    std::map<unsigned, std::size_t> caption_ids;
    const auto decode_caption = [&](ByteView bytes, std::size_t at, unsigned pointer) {
        caption_ids.emplace(pointer, assets.captions.size());
        const auto header = slice(bytes, at, 4);
        const unsigned destination = read16(header, 0);
        require(destination >= 0x4400 && destination < 0x4680 && destination % 2 == 0 &&
                    header[3] < 8,
                "Invalid credit caption header");
        OpeningCaption caption{(destination - 0x4400) / 2, header[2], header[3], {}};
        for (at += 4;; ++at) {
            const auto glyph = slice(bytes, at, 1)[0];
            caption.glyphs.push_back(glyph);
            if (glyph == 0)
                break;
        }
        assets.captions.push_back(std::move(caption));
        return at + 1;
    };
    for (std::size_t at = 0; at < captions.size();)
        at = decode_caption(captions, at, 0x6598 + static_cast<unsigned>(at));
    require(decode_caption(timeline, 210, 0x5e19) == timeline.size(),
            "Invalid blank credit extent");
    for (unsigned i = 0; i < 70; ++i) {
        const auto frame = timeline.subspan(i * 3, 3);
        const auto it = caption_ids.find(read16(frame, 0));
        require(it != caption_ids.end() && frame[2] > 0 &&
                    (i == 69 ? frame[2] == 254 : frame[2] < 254),
                "Invalid credit timeline reference or duration");
        assets.caption_animation.push_back({it->second, frame[2]});
    }
    return assets;
}
} // namespace ff6
