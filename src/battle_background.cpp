#include "ff6/battle_background.hpp"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
Bytes compressed(ByteView data, std::size_t start, std::size_t maximum) {
    const auto size = read16(data, start);
    return decompress_lzss(slice(data, start, size), maximum);
}
unsigned wrapped(std::int64_t value, unsigned extent) {
    const auto remainder = value % extent;
    return static_cast<unsigned>(remainder < 0 ? remainder + extent : remainder);
}
} // namespace
BattleBackgroundDefinition decode_battle_background_definition(ByteView record) {
    require(record.size() == 6, "Invalid battle background definition extent");
    BattleBackgroundDefinition result;
    for (unsigned i = 0; i < 3; ++i) {
        if (record[i] == 255)
            continue;
        const auto identity = static_cast<std::uint8_t>(record[i] & 127U);
        require(identity < 75, "Invalid battle background graphics selector");
        result.graphics[i] = BattleBackgroundGraphics{
            identity, static_cast<std::uint16_t>((record[i] & 128U) ? 256 : 128)};
    }
    for (unsigned i = 0; i < 2; ++i) {
        if (record[i + 3] == 255)
            continue;
        require(record[i + 3] < 112, "Invalid battle background layout selector");
        result.layouts[i] = record[i + 3];
    }
    result.palette = record[5] & 127U;
    result.wave = (record[5] & 128U) != 0;
    require(result.palette < 53, "Invalid battle background palette selector");
    return result;
}
BattleBackgroundLayout decode_battle_background_layout(ByteView bytes) {
    require(bytes.size() == 2048, "Invalid battle background layout extent");
    BattleBackgroundLayout result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto word = read16(bytes, i * 2);
        result[i] = {static_cast<std::uint16_t>(word & 1023U),
                     static_cast<std::uint8_t>((word >> 10U) & 7U), (word & 0x2000U) != 0,
                     (word & 0x4000U) != 0, (word & 0x8000U) != 0};
    }
    return result;
}
BattleBackgroundGraphicsData load_battle_background_graphics(const ContentPack& pack,
                                                             std::uint8_t identity) {
    const auto pointers = pack.get("background_tables_gfx_pointers");
    require(identity < 75 && pointers.size() == 225, "Invalid battle graphics pointer table");
    const auto pointer = read24(pointers, std::size_t{identity} * 3);
    // Only the serialized data adapter knows original address conventions.
    // Raw field aliases select an 8 KiB window, even for a 4 KiB destination.
    if (pointer < 0xe70000U) {
        require(pointer >= 0xdfdb00U, "Battle graphics alias outside field resources");
        const auto raw = pack.get("map_graphics_map_gfx_4bpp");
        const auto start = std::size_t{pointer - 0xdfdb00U};
        require(start < raw.size() && raw.size() % 32 == 0 && start % 32 == 0,
                "Invalid field graphics alias extent");
        // The last field resource ends before the original blind 8 KiB read.
        // Preserve only extracted graphics; unowned trailing bytes stay unknown.
        return {decode_tiles(slice(raw, start, std::min<std::size_t>(8192, raw.size() - start)), 4),
                false};
    }
    require(pointer >= 0xe7a9e7U, "Battle graphics pointer outside compressed resources");
    return {decode_tiles(compressed(pack.get("battle_background_battle_bg_gfx_4bpp_lz"),
                                    pointer - 0xe7a9e7U, 8192),
                         4),
            true};
}
BattleBackgroundLayout load_battle_background_layout(const ContentPack& pack,
                                                     std::uint8_t identity) {
    const auto pointers = pack.get("background_tables_tiles_pointers");
    require(identity < 112 && pointers.size() == 224, "Invalid battle layout pointer table");
    const auto pointer = read16(pointers, std::size_t{identity} * 2);
    require(pointer >= 0x1928U, "Battle layout pointer outside resource");
    return decode_battle_background_layout(
        compressed(pack.get("battle_background_battle_bg_tiles_scr_lz"), pointer - 0x1928U, 2048));
}
BattleBackground load_battle_background(const ContentPack& pack, std::uint8_t identity,
                                        std::span<const std::optional<Tile>> retained_terrain) {
    const auto definitions = pack.get("background_tables_properties");
    require(identity < 56 && definitions.size() == 336, "Invalid battle background identity/table");
    BattleBackground result;
    require(retained_terrain.empty() || retained_terrain.size() == 256,
            "Invalid retained battle terrain extent");
    std::ranges::copy(retained_terrain, result.characters.begin());
    result.identity = identity;
    result.definition =
        decode_battle_background_definition(slice(definitions, std::size_t{identity} * 6, 6));
    // Initial scene construction clears these character ranges before loading.
    // The other holes belong to retained earlier content; never invent pixels.
    for (unsigned i = 448; i < 480; ++i)
        result.characters[i] = Tile{};
    for (unsigned i = 768; i < 1024; ++i)
        result.characters[i] = Tile{};
    std::array<std::optional<Tile>, 256> workspace;
    constexpr std::array<unsigned, 3> starts{0, 128, 896};
    for (unsigned group = 0; group < 3; ++group) {
        const auto& selection = result.definition.graphics[group];
        if (!selection)
            continue;
        const auto tiles = load_battle_background_graphics(pack, selection->identity);
        require(tiles.tiles.size() <= workspace.size(), "Oversized battle graphics selection");
        // Decompression replaces its produced prefix. A shorter next block can
        // reuse the preceding decoded tail; zero-padding would change that data.
        if (!tiles.retains_tail)
            workspace.fill(std::nullopt);
        std::ranges::copy(tiles.tiles, workspace.begin());
        const auto count = std::min<unsigned>(selection->tile_count, 1024U - starts[group]);
        for (unsigned i = 0; i < count; ++i)
            result.characters[starts[group] + i] = workspace[i];
    }
    for (unsigned i = 0; i < 2; ++i)
        if (const auto layout = result.definition.layouts[i])
            result.layouts[i] = load_battle_background_layout(pack, *layout);
    const auto palettes = pack.get("battle_background_battle_bg_pal_pal");
    require(palettes.size() == 53 * 96, "Invalid battle background palette extent");
    for (unsigned p = 0; p < 3; ++p)
        for (unsigned c = 0; c < 16; ++c)
            result.colors[p][c] =
                read16(palettes, std::size_t{result.definition.palette} * 96 + p * 32 + c * 2) &
                0x7fffU;
    return result;
}
TileLayer make_battle_background_layer(const BattleBackground& background,
                                       BattleBackgroundView view) {
    require(view.width > 0 && view.width <= 512 && view.height > 0 && view.height <= 256,
            "Invalid battle background viewport");
    const auto x = wrapped(view.x, 512), y = wrapped(view.y, 256);
    TileLayer result;
    result.columns = static_cast<std::uint16_t>((view.width + x % 8 + 7) / 8);
    result.rows = static_cast<std::uint16_t>((view.height + y % 8 + 7) / 8);
    result.wrap_x = result.wrap_y = false;
    result.palettes.assign(background.colors.begin(), background.colors.end());
    result.lines.resize(view.height);
    for (auto& line : result.lines) {
        line.right = view.width;
        line.offset_x = static_cast<std::int32_t>(x % 8);
        line.offset_y = static_cast<std::int32_t>(y % 8);
    }
    for (unsigned row = 0; row < result.rows; ++row)
        for (unsigned column = 0; column < result.columns; ++column) {
            const auto source_x = (x / 8 + column) % 64, source_y = (y / 8 + row) % 32;
            const auto& layout = background.layouts[source_x / 32];
            require(layout.has_value(), "Battle view requires an unrepresented retained layout");
            const auto cell = (*layout)[source_y * 32 + source_x % 32];
            require(cell.character < background.characters.size(),
                    "Invalid background character identity");
            const auto& tile = background.characters[cell.character];
            require(tile.has_value(), "Battle view requires an unrepresented retained character");
            const bool transparent =
                std::ranges::all_of(*tile, [](auto pixel) { return pixel == 0; });
            require(cell.palette >= 5 || transparent, "Battle view requires another scene palette");
            require(cell.palette < 8, "Invalid background cell palette");
            // Mode 1 background-two depths, preserving gaps for the scene's
            // other planes and objects. Index zero remains transparent.
            result.cells.push_back({static_cast<std::uint32_t>(result.tiles.size()),
                                    cell.palette >= 5 ? unsigned{cell.palette} - 5U : 0U,
                                    static_cast<std::uint16_t>(cell.priority ? 7 : 4), cell.flip_x,
                                    cell.flip_y});
            result.tiles.push_back(*tile);
        }
    return result;
}
TileLayer make_battle_background_plane(const BattleBackground& background,
                                       BattleBackgroundView view) {
    require(view.width > 0 && view.width <= 512 && view.height > 0 && view.height <= 256,
            "Invalid battle background viewport");
    TileLayer result;
    const bool repeated = background.layouts[0] && background.layouts[1] &&
                          background.layouts[0] == background.layouts[1];
    result.columns = repeated ? 32 : 64;
    result.rows = 32;
    result.palettes.assign(background.colors.begin(), background.colors.end());
    result.tiles.emplace_back();
    for (unsigned row = 0; row < 32; ++row)
        for (unsigned column = 0; column < result.columns; ++column) {
            RasterCell cell;
            cell.available = false;
            cell.depth = 7; // Conservative until a retained layout owns priority.
            const auto& layout = background.layouts[column / 32];
            if (layout) {
                const auto source = (*layout)[row * 32 + column % 32];
                require(source.character < background.characters.size() && source.palette < 8,
                        "Invalid battle background cell");
                cell.depth = source.priority ? 7 : 4;
                cell.flip_x = source.flip_x;
                cell.flip_y = source.flip_y;
                if (const auto& tile = background.characters[source.character]) {
                    const bool transparent =
                        std::ranges::all_of(*tile, [](auto pixel) { return pixel == 0; });
                    if (source.palette >= 5 || transparent) {
                        cell.available = true;
                        cell.palette = source.palette >= 5 ? source.palette - 5U : 0U;
                        cell.tile = static_cast<std::uint32_t>(result.tiles.size());
                        result.tiles.push_back(*tile);
                    }
                }
            }
            result.cells.push_back(cell);
        }
    result.lines.resize(view.height);
    for (auto& line : result.lines) {
        line.right = view.width;
        line.offset_x = static_cast<std::int32_t>(wrapped(view.x, result.columns * 8U));
        line.offset_y = static_cast<std::int32_t>(wrapped(view.y, 256));
    }
    return result;
}
RasterScene make_battle_background_study(const BattleBackground& background,
                                         BattleBackgroundView view) {
    RasterScene scene;
    scene.width = view.width;
    scene.height = view.height;
    scene.intensity.assign(scene.height, 15);
    scene.layers.push_back(make_battle_background_layer(background, view));
    return scene;
}
std::array<Tile, 256> assemble_battle_train_graphics(const BattleBackgroundLayout& layout,
                                                     std::span<const Tile> first,
                                                     std::span<const Tile> third) {
    std::array<Tile, 256> result;
    for (unsigned i = 0; i < result.size(); ++i) {
        const auto character = unsigned{layout[i].character} & 511U;
        const bool use_third = character >= 256;
        // The original subtracts 128 from the second range's low byte. Values
        // 256..383 would underflow into unowned data, so malformed templates fail.
        require(!use_third || character >= 384, "Invalid train graphics selector");
        const auto index = use_third ? character - 384 : character;
        const auto source = use_third ? third : first;
        require(index < source.size(), "Train graphics selector outside resource");
        result[i] = source[index];
    }
    return result;
}
BattleTrainGraphics load_battle_train_graphics(const ContentPack& pack) {
    const auto definitions = pack.get("background_tables_properties");
    require(definitions.size() == 336, "Invalid background definition table");
    const auto definition = decode_battle_background_definition(slice(definitions, 50 * 6, 6));
    require(definition.graphics[0] && definition.graphics[2] && definition.layouts[0],
            "Incomplete train assembly template");
    const auto first = load_battle_background_graphics(pack, definition.graphics[0]->identity);
    const auto third = load_battle_background_graphics(pack, definition.graphics[2]->identity);
    // LoadTrainGfx reads these sources directly. A compressed selector is not
    // interchangeable with the decoded background-loader workspace.
    require(!first.retains_tail && !third.retains_tail, "Train graphics must be raw resources");
    BattleTrainGraphics result;
    result.tiles = assemble_battle_train_graphics(
        load_battle_background_layout(pack, *definition.layouts[0]), first.tiles, third.tiles);
    const auto properties = pack.get("monster_tables_properties");
    require(properties.size() == 416 * 5, "Invalid monster graphics property table");
    constexpr std::size_t train = 262 * 5;
    const auto palette = ((unsigned{properties[train + 2]} << 8U) | properties[train + 3]) & 1023U;
    // Monster palette identities advance eight colors; sixteen colors are read,
    // preserving overlapping palette records rather than treating them as slots.
    const auto colors = slice(pack.get("monster_graphics_monster_pal_pal"), palette * 16, 32);
    for (unsigned i = 0; i < result.colors.size(); ++i)
        result.colors[i] = read16(colors, i * 2) & 0x7fffU;
    return result;
}
RasterScene make_battle_train_study(const BattleTrainGraphics& graphics) {
    RasterScene scene;
    scene.width = 256;
    scene.height = 152;
    scene.intensity.assign(scene.height, 15);
    TileLayer layer;
    layer.columns = 16;
    layer.rows = 16;
    layer.wrap_x = layer.wrap_y = false;
    layer.tiles.assign(graphics.tiles.begin(), graphics.tiles.end());
    layer.palettes.push_back(graphics.colors);
    layer.lines.resize(scene.height);
    for (auto& line : layer.lines) {
        line.right = scene.width;
        line.offset_x = -64; // Center the monster's 128-pixel graphics allocation.
    }
    for (std::uint32_t i = 0; i < graphics.tiles.size(); ++i)
        layer.cells.push_back({i, 0, 4, false, false});
    scene.layers.push_back(std::move(layer));
    return scene;
}
} // namespace ff6
