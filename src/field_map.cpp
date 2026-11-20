#include "ff6/field_map.hpp"
#include "rom-materials/field_camera_minimum_x.macros.inc"
#include "rom-materials/field_camera_shared_edge.macros.inc"
#include "rom-materials/field_camera_center_tiles.macros.inc"
#include "rom-materials/field_camera_parallax_origin.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::invalid_argument(message);
}
Bytes compressed(const ContentPack& pack, std::string_view payload_id, std::string_view pointers_id,
                 std::size_t count, std::size_t pointer_width, std::size_t identity,
                 std::size_t limit) {
    const auto payload = pack.get(payload_id), pointers = pack.get(pointers_id);
    require(identity < count && pointers.size() == (count + 1) * pointer_width,
            "Invalid map resource pointer table");
    const auto pointer = [&](std::size_t index) -> std::size_t {
        return pointer_width == 2 ? read16(pointers, index * 2) : read24(pointers, index * 3);
    };
    require(pointer(0) == 0 && pointer(count) == payload.size(), "Invalid map resource extent");
    const auto start = pointer(identity), end = pointer(identity + 1);
    require(start < end, "Unordered map resource pointers");
    return decompress_lzss(slice(payload, start, end - start), limit);
}
std::uint16_t dimension(unsigned code) {
    return static_cast<std::uint16_t>(16U << code);
}
std::int64_t wrapped(std::int64_t position, std::uint16_t extent) {
    const auto remainder = position % extent;
    return remainder < 0 ? remainder + extent : remainder;
}
std::uint8_t metatile_at(const FieldMap& map, std::uint8_t layer, std::int64_t x, std::int64_t y) {
    const auto& layout = map.definition.layouts[layer];
    require(layout.width == 16 || layout.width == 32 || layout.width == 64 || layout.width == 128,
            "Invalid field layout width");
    require(layout.height == 16 || layout.height == 32 || layout.height == 64 ||
                layout.height == 128,
            "Invalid field layout height");
    const auto column = static_cast<std::size_t>(wrapped(x, layout.width));
    const auto row = static_cast<std::size_t>(wrapped(y, layout.height));
    return field_map_tile(map, layer, static_cast<unsigned>(column), static_cast<unsigned>(row));
}
} // namespace

std::uint8_t field_map_tile(const FieldMap& map, std::uint8_t layer, unsigned x, unsigned y) {
    require(layer < 3, "Invalid field map layer");
    const auto layout = map.definition.layouts[layer];
    const auto valid = [](unsigned n) { return n == 16 || n == 32 || n == 64 || n == 128; };
    require(valid(layout.width) && valid(layout.height), "Invalid field layout extent");
    require(x < layout.width && y < 64, "Field cell needs unresolved retained/cross-layer state");
    const auto edited = map.tile_edits[layer].find(static_cast<std::uint16_t>(y * 256U + x));
    if (edited != map.tile_edits[layer].end())
        return edited->second;
    if (layout.resource == 0)
        return 0;
    const auto index = std::size_t{y} * layout.width + x;
    require(index < map.layouts[layer].size(), "Field cell needs unresolved retained layout state");
    return map.layouts[layer][index];
}
FieldMapPatch decode_field_map_patch(ByteView data) {
    require(data.size() >= 2, "Truncated field map patch");
    const auto count = std::size_t{data[0]} * data[1];
    require(count != 0 && data.size() == count + 2, "Invalid field map patch extent");
    return {data[0], data[1], Bytes(data.begin() + 2, data.end())};
}
void apply_field_map_patch(FieldMap& map, std::uint8_t layer, std::uint8_t x, std::uint8_t y,
                           const FieldMapPatch& patch) {
    require(layer < 3, "Invalid field edit layer");
    const auto layout = map.definition.layouts[layer];
    const auto valid = [](unsigned n) { return n == 16 || n == 32 || n == 64 || n == 128; };
    require(valid(layout.width) && valid(layout.height), "Invalid field edit extent");
    require(patch.width && patch.height &&
                patch.tiles.size() == std::size_t{patch.width} * patch.height,
            "Invalid field map patch payload");
    require(unsigned{x} + patch.width <= layout.width &&
                unsigned{y} + patch.height <= layout.height && unsigned{y} + patch.height <= 64,
            "Field edit needs unrepresented retained/cross-layer cells");
    auto edits = map.tile_edits[layer];
    for (unsigned row = 0; row < patch.height; ++row)
        for (unsigned column = 0; column < patch.width; ++column)
            edits[static_cast<std::uint16_t>((y + row) * 256U + x + column)] =
                patch.tiles[row * patch.width + column];
    map.tile_edits[layer] = std::move(edits);
}

FieldMapDefinition decode_field_map_definition(ByteView record) {
    require(record.size() == 33, "Invalid field map record length");
    FieldMapDefinition result;
    result.graphics = {static_cast<std::uint8_t>(record[7] & 127U),
                       static_cast<std::uint8_t>((read16(record, 7) >> 7U) & 127U),
                       static_cast<std::uint8_t>((read16(record, 8) >> 6U) & 127U),
                       static_cast<std::uint8_t>((read16(record, 9) >> 5U) & 127U)};
    result.background3_graphics = (read16(record, 10) >> 4U) & 63U;
    result.tilesets = {static_cast<std::uint8_t>((read16(record, 11) >> 2U) & 127U),
                       static_cast<std::uint8_t>(record[12] >> 1U)};
    result.layouts = {MapLayout{static_cast<std::uint16_t>(read16(record, 13) & 1023U),
                                dimension(record[23] >> 6U), dimension((record[23] >> 4U) & 3U)},
                      MapLayout{static_cast<std::uint16_t>((read16(record, 14) >> 2U) & 1023U),
                                dimension((record[23] >> 2U) & 3U), dimension(record[23] & 3U)},
                      MapLayout{static_cast<std::uint16_t>((read16(record, 15) >> 4U) & 1023U),
                                dimension(record[24] >> 6U), dimension((record[24] >> 4U) & 3U)}};
    result.background3_foreground = (record[2] & 128U) != 0;
    result.tile_properties = record[4];
    result.palette = record[25];
    result.palette_animation = record[26];
    result.background_animation = record[27] & 31U;
    result.background3_animation = record[27] >> 5U;
    std::ranges::copy(record.subspan(18, 4), result.offsets.begin());
    result.parallax = record[22];
    result.overlay = record[17];
    result.color_math = record[32];
    result.song = record[28];
    result.scroll_bounds = {record[30], record[31]};
    result.window_mode = record[6] & 3U;
    for (auto id : result.graphics)
        require(id < 82, "Unknown field graphics identity");
    for (auto id : result.tilesets)
        require(id < 75, "Unknown field metatile identity");
    for (auto layout : result.layouts)
        require(layout.resource < 350, "Unknown field layout identity");
    require(result.background3_graphics < 18 && result.tile_properties < 42 &&
                result.palette < 48 && result.background_animation < 20 &&
                result.background3_animation <= 6 && result.palette_animation <= 10 &&
                result.parallax < 21 && result.color_math < 19,
            "Unsupported field map selector");
    return result;
}
MapMetatiles decode_map_metatiles(ByteView data) {
    require(data.size() == 2048, "Invalid metatile descriptor length");
    MapMetatiles result;
    for (std::size_t id = 0; id < 256; ++id)
        for (std::size_t quadrant = 0; quadrant < 4; ++quadrant) {
            const auto offset = quadrant * 256 + id;
            const auto word = unsigned{data[offset]} | (unsigned{data[offset + 1024]} << 8U);
            result[id][quadrant] = {static_cast<std::uint16_t>(word & 1023U),
                                    static_cast<std::uint8_t>((word >> 10U) & 7U),
                                    (word & 0x2000U) != 0, (word & 0x4000U) != 0,
                                    (word & 0x8000U) != 0};
        }
    return result;
}
FieldComposition decode_field_composition(ByteView record) {
    require(record.size() == 3 && (record[0] & 8U) == 0 && (record[1] & ~23U) == 0 &&
                (record[2] & ~23U) == 0,
            "Invalid field composition record");
    const bool subtract = (record[0] & 128U) != 0, half = (record[0] & 64U) != 0;
    const auto full = subtract ? ColorBlend::subtract : ColorBlend::add;
    const auto covered =
        half ? (subtract ? ColorBlend::half_difference : ColorBlend::average) : full;
    const auto mix = [&](unsigned bit) -> CompositeBlend {
        return (record[0] & bit) != 0 ? CompositeBlend{covered, full} : CompositeBlend{};
    };
    FieldComposition result;
    for (unsigned layer = 0; layer < 3; ++layer) {
        const auto bit = 1U << layer;
        result.backgrounds[layer] = {((record[1] | 1U) & bit) != 0, (record[2] & bit) != 0,
                                     mix(bit)};
    }
    result.objects = {(record[1] & 16U) != 0, (record[2] & 16U) != 0, mix(16)};
    result.backdrop = mix(32);
    return result;
}
FieldParallax decode_field_parallax(ByteView record) {
    require(record.size() == 8, "Invalid field parallax record");
    FieldParallax result;
    for (std::size_t i = 0; i < 4; ++i) {
        const auto signed_speed = record[i] < 128 ? int{record[i]} : int{record[i]} - 256;
        result.autonomous_speed[i] = static_cast<std::int16_t>(signed_speed * 16);
        result.multiplier[i] = record[i + 4];
    }
    return result;
}
FieldCamera initialize_field_camera(const FieldMapDefinition& definition,
                                    const FieldParallax& parallax, std::uint8_t x, std::uint8_t y) {
    // CalcScrollRange and InitScrollPos supply these viewport selections.
    // Signed offsets preserve positions before the first complete metatile.
    constexpr auto minimum_x = FF6_ROM_VALUE_FIELD_CAMERA_MINIMUM_X_0;
    constexpr auto edge = FF6_ROM_VALUE_FIELD_CAMERA_SHARED_EDGE_0;
    constexpr auto center = static_cast<int>(FF6_ROM_VALUE_FIELD_CAMERA_CENTER_TILES_0);
    constexpr auto parallax_origin = static_cast<int>(FF6_ROM_VALUE_FIELD_CAMERA_PARALLAX_ORIGIN_0);
    const bool bounded = definition.scroll_bounds[0] != 0 || definition.scroll_bounds[1] != 0;
    // The original compares the minimum first, then the maximum. Preserve that
    // order even for malformed ranges whose wrapped maximum is below minimum.
    const auto clamp = [](std::uint8_t value, std::uint8_t low, std::uint8_t high) {
        return value <= low ? low : value > high ? high : value;
    };
    FieldCamera camera;
    camera.center_x =
        bounded ? clamp(x, minimum_x, wrap8(unsigned{definition.scroll_bounds[0]} + (256U - edge))) : x;
    camera.center_y =
        bounded ? clamp(y, edge, wrap8(unsigned{definition.scroll_bounds[1]} + (256U - edge))) : y;
    camera.scroll[0] = {(int{camera.center_x} - center) * 16, (int{camera.center_y} - center) * 16};
    for (std::size_t layer = 1; layer < 3; ++layer) {
        const auto& layout = definition.layouts[layer];
        require((layout.width == 16 || layout.width == 32 || layout.width == 64 ||
                 layout.width == 128) &&
                    (layout.height == 16 || layout.height == 32 || layout.height == 64 ||
                     layout.height == 128),
                "Invalid camera clipping dimensions");
        const auto i = (layer - 1) * 2;
        const auto horizontal =
            int{definition.offsets[i]} * 16 + int{camera.center_x} * parallax.multiplier[i] - parallax_origin;
        const auto vertical = int{definition.offsets[i + 1]} * 16 +
                              int{camera.center_y} * parallax.multiplier[i + 1] - parallax_origin;
        camera.scroll[layer] = {static_cast<std::int32_t>(wrapped(
                                    horizontal, static_cast<std::uint16_t>(layout.width * 16))),
                                static_cast<std::int32_t>(wrapped(
                                    vertical, static_cast<std::uint16_t>(layout.height * 16)))};
    }
    return camera;
}
FieldMap load_field_map(const ContentPack& pack, std::uint16_t identity) {
    const auto records = pack.get("field_structure_map_prop_bin");
    require(records.size() == 415 * 33 && identity >= 3 && identity < 415,
            "Field map identity outside ordinary maps (3..414)");
    FieldMap map{};
    map.definition = decode_field_map_definition(slice(records, std::size_t{identity} * 33, 33));
    const auto& definition = map.definition;
    const auto styles = pack.get("data_c0fe00_map_color_math");
    const auto parallax = pack.get("data_c0fe40_map_parallax");
    require(styles.size() == 19 * 3 && parallax.size() == 21 * 8, "Invalid field scene tables");
    map.composition =
        decode_field_composition(slice(styles, std::size_t{definition.color_math} * 3, 3));
    map.parallax = decode_field_parallax(slice(parallax, std::size_t{definition.parallax} * 8, 8));
    for (std::size_t layer = 0; layer < 3; ++layer)
        if (definition.layouts[layer].resource != 0)
            map.layouts[layer] =
                compressed(pack, "submap_sub_tilemap_bin_lz", "submap_sub_tilemap_ptr_bin", 350, 3,
                           definition.layouts[layer].resource, 8192);
    for (std::size_t layer = 0; layer < 2; ++layer)
        map.metatiles[layer] = decode_map_metatiles(
            compressed(pack, "map_resource_map_tileset_bin_lz", "map_resource_map_tileset_ptr_bin",
                       75, 3, definition.tilesets[layer], 2048));
    const auto properties =
        compressed(pack, "map_resource_map_tile_prop_bin_lz", "map_resource_map_tile_prop_ptr_bin",
                   42, 2, definition.tile_properties, 512);
    require(properties.size() == 512, "Invalid map tile-property length");
    for (std::size_t i = 0; i < 256; ++i)
        map.tile_properties[i] = {properties[i], properties[i + 256]};
    const auto palettes = pack.get("map_graphics_map_pal_pal");
    require(palettes.size() == 48 * 256, "Invalid map palette resource");
    for (std::size_t i = 0; i < 128; ++i)
        map.colors[i] = read16(palettes, std::size_t{definition.palette} * 256 + i * 2) & 0x7fffU;
    map.colors[0] = 0;
    const auto raw = pack.get("map_graphics_map_gfx_4bpp"),
               pointers = pack.get("map_graphics_map_gfx_ptr_bin");
    require(pointers.size() == 82 * 3 && raw.size() % 32 == 0, "Invalid raw map graphics extent");
    // Resolve the game's overlapping character selections directly. Sources may
    // extend into adjacent stored graphics; no invented padding or device bus.
    constexpr std::array<std::size_t, 4> starts{0, 256, 384, 512};
    for (std::size_t group = 0; group < 4; ++group) {
        if (group == 3 && definition.graphics[3] == definition.graphics[2])
            continue;
        const auto start = read24(pointers, std::size_t{definition.graphics[group]} * 3);
        require(start < raw.size() && start % 32 == 0, "Invalid map graphics pointer");
        const auto available = std::min<std::size_t>(256, (raw.size() - start) / 32);
        const auto tiles = decode_tiles(slice(raw, start, available * 32), 4);
        for (std::size_t i = 0; i < 256; ++i)
            map.characters[starts[group] + i] =
                i < tiles.size() ? std::optional{tiles[i]} : std::nullopt;
    }
    const auto bg3 =
        compressed(pack, "map_graphics_map_gfx_bg3_2bpp_lz", "map_graphics_map_gfx_bg3_ptr_bin", 18,
                   3, definition.background3_graphics, 4160);
    require(bg3.size() == 4160, "Invalid static BG3 resource length");
    std::ranges::copy(ByteView(bg3).first(64), map.background3_controls.begin());
    const auto pixels = ByteView(bg3).subspan(64);
    std::ranges::copy(decode_tiles(pixels, 2), map.background3.begin());
    const auto aliases = decode_tiles(pixels, 4);
    for (std::size_t i = 0; i < aliases.size(); ++i)
        map.characters[768 + i] = aliases[i];
    const auto animation = pack.get("field_background_animation_tables");
    require(animation.size() == 1616, "Invalid field background animation tables");
    const auto frames = animation.subspan(42);
    const auto group_start = read16(animation, std::size_t{definition.background_animation} * 2);
    const auto graphics = pack.get("map_animation_map_anim_4bpp");
    require(graphics.size() == 32768, "Invalid initial background animation graphics");
    for (std::size_t group = 0; group < 16; ++group) {
        const auto source = read16(frames, group_start + group * 10 + 2);
        const auto tiles = decode_tiles(slice(graphics, source, 128), 4);
        for (std::size_t i = 0; i < 4; ++i)
            map.characters[640 + group * 4 + i] = tiles[i];
    }
    return map;
}
FieldMap load_field_map_destination(const ContentPack& pack, std::uint16_t identity,
                                    const FieldMap& previous_display) {
    auto map = load_field_map(pack, identity);
    // TfrBG3Gfx precedes all three layout decodes into their shared work buffer.
    // CopyMapTiles then consumes width * 64 bytes, irrespective of stream size.
    // Express that resource dependency as a known prefix, not a memory map.
    auto known =
        compressed(pack, "map_graphics_map_gfx_bg3_2bpp_lz", "map_graphics_map_gfx_bg3_ptr_bin", 18,
                   3, map.definition.background3_graphics, 4160);
    for (std::size_t layer = 0; layer < 3; ++layer) {
        if (map.definition.layouts[layer].resource == 0)
            continue; // Explicit clear does not overwrite the decoder's work.
        auto& layout = map.layouts[layer];
        if (layout.size() > known.size())
            known.resize(layout.size());
        std::ranges::copy(layout, known.begin());
        const auto count =
            std::min<std::size_t>(known.size(), map.definition.layouts[layer].width * 64U);
        layout.assign(known.begin(), known.begin() + static_cast<std::ptrdiff_t>(count));
    }
    // Groups 1..3 always overwrite 0..639; initial animation overwrites 640..703.
    // Only this final range survives when the fourth graphics copy is skipped.
    if (map.definition.graphics[3] == map.definition.graphics[2])
        std::copy_n(previous_display.characters.begin() + 704, 64, map.characters.begin() + 704);
    return map;
}
void set_field_text_colors(std::array<Rgb555, 128>& base, std::array<Rgb555, 128>& active,
                           Rgb555 font_color) {
    require(font_color <= 0x7fff, "Invalid field font color");
    for (auto* colors : {&base, &active}) {
        (*colors)[1] = font_color;
        (*colors)[2] = 0;
        (*colors)[3] = font_color;
    }
}
void set_field_window_colors(std::array<Rgb555, 128>& base, std::array<Rgb555, 128>& active,
                             const std::array<Rgb555, 7>& colors) {
    for (auto color : colors)
        require(color <= 0x7fff, "Invalid field window color");
    std::ranges::copy(colors, base.begin() + 121);
    std::ranges::copy(colors, active.begin() + 121);
}
MapMetatile field_map_metatile(const FieldMap& map, std::uint8_t layer, std::uint8_t identity) {
    require(layer < 3, "Invalid field formation layer");
    if (layer < 2)
        return map.metatiles[layer][identity];
    MapMetatile result;
    const auto id = identity & 63U;
    for (unsigned quadrant = 0; quadrant < 4; ++quadrant)
        result[quadrant] = {static_cast<std::uint16_t>(id * 4 + (quadrant ^ (identity >> 6U))),
                            static_cast<std::uint8_t>((map.background3_controls[id] >> 2U) & 7U),
                            map.definition.background3_foreground, (identity & 64U) != 0,
                            (identity & 128U) != 0};
    return result;
}
RasterScene make_field_layer_scene(const FieldMap& map, std::uint8_t layer, std::int32_t origin_x,
                                   std::int32_t origin_y, std::uint16_t width, std::uint16_t height,
                                   const FieldMapView* view) {
    require(layer < 3 && width > 0 && width <= 4096 && height > 0 && height <= 4096,
            "Invalid field layer viewport");
    // Floor division matters when inspecting a viewport across the negative edge.
    const auto local_x = wrapped(origin_x, 8), local_y = wrapped(origin_y, 8);
    const auto start_x = (std::int64_t{origin_x} - local_x) / 8;
    const auto start_y = (std::int64_t{origin_y} - local_y) / 8;
    TileLayer output;
    output.columns = static_cast<std::uint16_t>((width + local_x + 7) / 8);
    output.rows = static_cast<std::uint16_t>((height + local_y + 7) / 8);
    require(output.columns <= 512 && output.rows <= 512, "Field viewport exceeds raster grid");
    output.wrap_x = output.wrap_y = false;
    output.lines.resize(height);
    for (auto& line : output.lines) {
        line.offset_x = static_cast<std::int32_t>(local_x);
        line.offset_y = static_cast<std::int32_t>(local_y);
        line.right = width;
    }
    output.palettes.resize(8);
    for (std::size_t p = 0; p < 8; ++p)
        for (std::size_t c = 0; c < (layer == 2 ? 4U : 16U); ++c)
            output.palettes[p][c] = map.colors[p * (layer == 2 ? 4U : 16U) + c];
    std::array<std::optional<std::uint32_t>, 1024> selected{};
    for (std::uint16_t y = 0; y < output.rows; ++y)
        for (std::uint16_t x = 0; x < output.columns; ++x) {
            const auto tx = start_x + x, ty = start_y + y;
            const auto qx = wrapped(tx, 2), qy = wrapped(ty, 2);
            const auto mx = (tx - qx) / 2, my = (ty - qy) / 2;
            const auto formation =
                view ? (*view)[layer]
                              [static_cast<std::size_t>(wrapped(my, 16) * 16 + wrapped(mx, 16))]
                     : field_map_metatile(map, layer, metatile_at(map, layer, mx, my));
            const auto quadrant = static_cast<std::size_t>(qy * 2 + qx);
            const auto descriptor = formation[quadrant];
            require(descriptor.character < selected.size() && descriptor.palette < 8,
                    "Invalid field quadrant");
            auto& tile = selected[descriptor.character];
            if (!tile) {
                tile = static_cast<std::uint32_t>(output.tiles.size());
                if (layer == 2) {
                    require(descriptor.character < map.background3.size(), "Invalid BG3 character");
                    output.tiles.push_back(map.background3[descriptor.character]);
                } else {
                    require(map.characters[descriptor.character].has_value(),
                            "Field character needs unresolved retained content");
                    output.tiles.push_back(*map.characters[descriptor.character]);
                }
            }
            output.cells.push_back({*tile, descriptor.palette,
                                    static_cast<std::uint16_t>(descriptor.priority ? 2 : 1),
                                    descriptor.flip_x, descriptor.flip_y});
        }
    RasterScene scene;
    scene.width = width;
    scene.height = height;
    scene.intensity.assign(height, 15);
    scene.layers.push_back(std::move(output));
    return scene;
}
RasterScene make_field_background_scene(const FieldMap& map,
                                        const std::array<FieldOrigin, 3>& origins,
                                        std::uint16_t width, std::uint16_t height,
                                        const FieldMapView* view,
                                        std::array<bool, 3> include_inactive) {
    require(width > 0 && width <= 4096 && height > 0 && height <= 4096,
            "Invalid field background viewport");
    RasterScene scene;
    scene.width = width;
    scene.height = height;
    scene.backdrop = map.colors[0];
    scene.backdrop_mix = map.composition.backdrop;
    scene.intensity.assign(height, 15);
    // Ordinary field ordering includes gaps for object depths {2, 3, 6, 9}.
    // Background relative order is unchanged. See FIELD_SPRITES.md.
    constexpr std::array<std::array<std::uint16_t, 2>, 3> depths{{{5, 8}, {4, 7}, {1, 10}}};
    for (std::uint8_t i = 0; i < 3; ++i) {
        const auto& style = map.composition.backgrounds[i];
        if (!style.visible && !style.operand && !include_inactive[i])
            continue;
        auto layer_scene =
            make_field_layer_scene(map, i, origins[i].x, origins[i].y, width, height, view);
        auto layer = std::move(layer_scene.layers.front());
        for (auto& cell : layer.cells)
            cell.depth = depths[i][cell.depth == 2 ? 1 : 0];
        layer.visible = style.visible;
        layer.operand = style.operand;
        layer.mix = style.mix;
        scene.layers.push_back(std::move(layer));
    }
    return scene;
}
} // namespace ff6
