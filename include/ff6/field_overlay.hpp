#pragma once
#include "ff6/field_movement.hpp"

namespace ff6 {
// Terrain masks are game resources and ordered scene pieces, not a video-device
// interface. A missing graphic is distinct from a loaded, transparent graphic.
struct FieldOverlaySet {
    std::array<std::uint8_t, 256> controls{};
    std::array<std::optional<Tile>, 64> graphics;
};
FieldOverlaySet load_field_overlay(const ContentPack& pack, std::uint8_t id);
struct FieldOverlayOffset {
    std::int16_t x = 0, y = 0;
    std::uint8_t local_cell = 0;
};
struct FieldOverlayLayouts {
    std::array<std::array<FieldOverlayOffset, 3>, 5> stairs;
    std::array<std::array<FieldOverlayOffset, 2>, 4> cardinal;
};
FieldOverlayLayouts load_field_overlay_layouts(const ContentPack& pack);
struct FieldOverlayPiece {
    std::uint8_t x = 0, y = 0;
    std::optional<std::uint8_t> graphic;
    bool flip_x = false, flip_y = false, lower = false;
    std::uint8_t palette = 0;
    bool operator==(const FieldOverlayPiece&) const = default;
};
using FieldOverlayState = std::array<FieldOverlayPiece, 4>;
struct FieldOverlayView {
    std::array<std::uint16_t, 2> origin{};
    std::array<std::int16_t, 2> follow_rate{};
    std::uint8_t shake_y = 0;
};
// Called on player action boundaries; other movement modes retain the old masks.
// Cached current/top flags and freshly looked-up neighboring properties have
// different source ownership, so both the local cache and map are explicit.
void update_field_overlay(FieldOverlayState& state, const FieldOverlaySet& assets,
                          const FieldOverlayLayouts& layouts, const FieldMap& map,
                          const FieldObject& player, const FieldPlayerTerrain& terrain,
                          const FieldLocalTiles& local, const FieldOverlayView& view);
void clear_field_overlay(FieldOverlayState& state);
// A new object frame starts empty. This projection applies the current scroll
// phase and shake to retained action-boundary positions, preserving band order.
using FieldOverlayFrame = std::array<std::array<std::optional<FieldOverlayPiece>, 4>, 2>;
FieldOverlayFrame project_field_overlay(const FieldOverlayState& state,
                                        const FieldOverlayView& view);
struct FieldOverlaySprites {
    std::array<std::vector<RasterSprite>, 2> bands;
};
FieldOverlaySprites make_field_overlay_sprites(const FieldOverlaySet& assets,
                                               const FieldOverlayFrame& frame,
                                               const std::array<std::array<Rgb555, 16>, 8>& colors,
                                               CompositeBlend mix = {});
} // namespace ff6
