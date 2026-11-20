#pragma once
#include "ff6/field_state.hpp"

namespace ff6 {
struct FieldVehiclePiece {
    std::array<Tile, 4> tiles;
    FieldSpriteAppearance appearance;
    // Optional normalized tile in the shared special/vehicle atlas. Synthetic
    // stand-alone pieces may provide their four tiles directly.
    std::optional<std::uint16_t> atlas_tile;
};
struct FieldObjectViewAssets {
    FieldSpriteAssets sprites;
    std::array<std::uint8_t, 128> jump_offsets{};
    // Four directions, four phases, upper-left/right then lower-left/right.
    std::array<std::array<FieldVehiclePiece, 4>, 16> magitek;
    std::array<Bytes, 4> chocobo; // Up/down/right/left frame tables, retained as data.
    std::array<std::array<std::uint16_t, 4>, 4> raft{}; // Piece, then facing.
    // Game-owned graphics atlas. Vehicle content initializes slots 32..255;
    // special NPC definitions overwrite rectangular regions in declaration
    // order, even when disabled. Unwritten retained slots stay explicitly unknown.
    std::array<std::optional<Tile>, 256> special_tiles;
    // InitPortrait uses the first sixteen portraits directly, without the
    // menu's character aliases. Tiles are arranged as a 5x5 image here.
    std::array<std::array<Tile, 25>, 16> portraits{};
    std::array<std::array<Rgb555, 16>, 16> portrait_colors{};
};
FieldObjectViewAssets load_field_object_view(const ContentPack& pack);
// Prepare the map's defined NPCs, independently of enabled/visible membership.
// Preserves other objects and commits graphics/actor changes together.
void prepare_field_special_graphics(FieldObjects& field, FieldObjectViewAssets& assets,
                                    std::size_t npc_count);
void prepare_field_portrait(FieldObjectViewAssets& assets, std::uint8_t identity,
                           std::array<std::array<Rgb555, 16>, 8>& pending_colors);
struct FieldObjectClocks {
    std::uint8_t logic = 0, vblank = 0;
    bool diagonal_tile = false, party_on_ladder = false;
};
// Integrate chosen velocities, update jump/pose, then stop at joint tile alignment.
// This does not choose a route, test collision or advance a host clock.
void step_field_objects(FieldObjects& field, const FieldObjectViewAssets& assets,
                        FieldObjectClocks clocks);
void latch_field_object_poses(FieldObjects& field);
void publish_field_objects(const FieldObjects& field, const FieldObjectViewAssets& assets,
                           FieldSpritePublication& publication);
struct FieldObjectView {
    std::array<FieldOrigin, 2> origins;
    std::uint16_t shake_y = 0;
    bool party_low_priority = false;
    std::array<std::array<Rgb555, 16>, 8> colors;
    FieldLayerStyle style;
    // Clock at DrawObjSprites, retained with the prepared object list. Later
    // display/audio-only publications must not animate that list again.
    std::uint8_t vblank_count = 0;
};
struct FieldOverlaySprites;
// Produces character, vehicle and special pieces in the game's draw order. Unsupported visible
// object kinds and unresolved published graphics fail explicitly.
SpriteLayer make_field_object_layer(const FieldObjects& field, const FieldObjectViewAssets& assets,
                                    const FieldSpritePublication& publication,
                                    const FieldObjectView& view,
                                    const FieldOverlaySprites* overlays = nullptr);
} // namespace ff6
