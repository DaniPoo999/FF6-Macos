#pragma once
#include "ff6/content.hpp"
#include "ff6/raster.hpp"
#include <map>
#include <optional>

namespace ff6 {
struct MapLayout {
    std::uint16_t resource = 0, width = 16, height = 16;
    bool operator==(const MapLayout&) const = default;
};
struct FieldMapDefinition {
    std::array<std::uint8_t, 4> graphics{};
    std::uint8_t background3_graphics = 0;
    std::array<std::uint8_t, 2> tilesets{};
    std::array<MapLayout, 3> layouts;
    bool background3_foreground = false;
    std::uint8_t tile_properties = 0, palette = 0, palette_animation = 0;
    std::uint8_t background_animation = 0, background3_animation = 0;
    std::array<std::uint8_t, 4> offsets{}; // BG2 x/y, BG3 x/y, in metatiles.
    std::uint8_t parallax = 0, overlay = 0, color_math = 0, song = 0;
    std::array<std::uint8_t, 2> scroll_bounds{};
    std::uint8_t window_mode = 0;
    bool operator==(const FieldMapDefinition&) const = default;
};
struct MapQuadrant {
    std::uint16_t character = 0;
    std::uint8_t palette = 0;
    bool priority = false, flip_x = false, flip_y = false;
    bool operator==(const MapQuadrant&) const = default;
};
using MapMetatile = std::array<MapQuadrant, 4>;
using MapMetatiles = std::array<MapMetatile, 256>;
FieldMapDefinition decode_field_map_definition(ByteView record);
MapMetatiles decode_map_metatiles(ByteView data);
struct FieldLayerStyle {
    bool visible = false, operand = false;
    CompositeBlend mix;
    bool operator==(const FieldLayerStyle&) const = default;
};
struct FieldComposition {
    std::array<FieldLayerStyle, 3> backgrounds;
    FieldLayerStyle objects; // Retained for the future object scene producer.
    CompositeBlend backdrop;
    bool operator==(const FieldComposition&) const = default;
};
struct FieldParallax {
    std::array<std::int16_t, 4> autonomous_speed{}; // 1/256 pixel, BG2 x/y then BG3 x/y.
    std::array<std::uint8_t, 4> multiplier{};       // Unsigned sixteenths.
    bool operator==(const FieldParallax&) const = default;
};
struct FieldOrigin {
    std::int32_t x = 0, y = 0;
    bool operator==(const FieldOrigin&) const = default;
};
struct FieldCamera {
    std::uint8_t center_x = 0, center_y = 0;
    std::array<FieldOrigin, 3> scroll; // Layer-pixel origins before display offsets.
    bool operator==(const FieldCamera&) const = default;
};
FieldComposition decode_field_composition(ByteView record);
FieldParallax decode_field_parallax(ByteView record);
FieldCamera initialize_field_camera(const FieldMapDefinition& definition,
                                    const FieldParallax& parallax, std::uint8_t x, std::uint8_t y);
struct FieldMap {
    FieldComposition composition;
    FieldParallax parallax;
    FieldMapDefinition definition;
    // Original ordered storage, not guessed rectangles. An empty vector means
    // the explicit layout-zero clear; short nonempty layouts have unknown tails.
    std::array<Bytes, 3> layouts;
    // Native edits keyed by (y * 256 + x). Base storage remains immutable;
    // defining one previously unknown cell does not invent its surrounding tail.
    std::array<std::map<std::uint16_t, std::uint8_t>, 3> tile_edits;
    std::array<MapMetatiles, 2> metatiles;
    std::array<std::array<std::uint8_t, 2>, 256> tile_properties;
    std::array<Rgb555, 128> colors;
    // Game character identities. Missing values require retained-state research;
    // there is no implicit blank or executable fallback.
    std::array<std::optional<Tile>, 1024> characters;
    std::array<Tile, 256> background3;
    std::array<std::uint8_t, 64> background3_controls;
};
// Prepared native scene cells retain formations independently of logical terrain
// and later table changes. Current character graphics and palettes remain live.
using FieldMapView = std::array<MapMetatiles, 3>;
MapMetatile field_map_metatile(const FieldMap& map, std::uint8_t layer, std::uint8_t identity);
FieldMap load_field_map(const ContentPack& pack, std::uint16_t identity);
// Ordinary destination loading: resolve short layout tails from the preceding
// BG3/layer decodes in this load, and preserve only graphics not overwritten.
// Unknown earlier decompression tails remain unknown; never invent blank cells.
FieldMap load_field_map_destination(const ContentPack& pack, std::uint16_t identity,
                                    const FieldMap& previous_display);
// Unwrapped coordinates. Known retained rows below row 64 may be read even
// beyond the logical height (the player's current-row lookup requires this).
std::uint8_t field_map_tile(const FieldMap& map, std::uint8_t layer, unsigned x, unsigned y);
struct FieldMapPatch {
    std::uint8_t width = 0, height = 0;
    Bytes tiles; // Row-major metatile identities, not pixels or executable code.
    bool operator==(const FieldMapPatch&) const = default;
};
FieldMapPatch decode_field_map_patch(ByteView data);
// Unwrapped rectangular edit, committed atomically. Cross-layer/retained cells
// outside the logical map are not yet represented and fail explicitly.
void apply_field_map_patch(FieldMap& map, std::uint8_t layer, std::uint8_t x, std::uint8_t y,
                           const FieldMapPatch& patch);
// Field dialogue refresh also owns these background palette entries when no
// text is visible. Keep both the unmodified base and active colors consistent.
void set_field_text_colors(std::array<Rgb555, 128>& base, std::array<Rgb555, 128>& active,
                           Rgb555 font_color);
void set_field_window_colors(std::array<Rgb555, 128>& base, std::array<Rgb555, 128>& active,
                             const std::array<Rgb555, 7>& colors);
// Isolated layer using the map's currently owned graphics/colors. Coordinates
// are layer pixels; this function does not advance camera or animation state.
RasterScene make_field_layer_scene(const FieldMap& map, std::uint8_t layer, std::int32_t origin_x,
                                   std::int32_t origin_y, std::uint16_t width = 256,
                                   std::uint16_t height = 224, const FieldMapView* view = nullptr);
// Backgrounds only: caller supplies independently owned camera/animation state.
// No objects, overlays, doors, waves, dialogue windows or fades are implied.
// Optional inactive layers retain their ordinary disabled coverage so a later
// scene effect can enable specific rows. Included layers preserve source order.
RasterScene make_field_background_scene(const FieldMap& map,
                                        const std::array<FieldOrigin, 3>& origins,
                                        std::uint16_t width = 256, std::uint16_t height = 224,
                                        const FieldMapView* view = nullptr,
                                        std::array<bool, 3> include_inactive = {});
} // namespace ff6
