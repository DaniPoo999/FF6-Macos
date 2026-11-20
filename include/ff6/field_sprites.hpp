#pragma once
#include "ff6/field_map.hpp"

namespace ff6 {
enum class FieldFacing : std::uint8_t { up, right, down, left };
enum class FieldRide : std::uint8_t { none, chocobo, magitek, raft };
enum class FieldSpecialCycle : std::uint8_t { still, alternating_flip, two, four };
struct FieldSpecialPose {
    FieldSpecialCycle cycle = FieldSpecialCycle::still;
    std::uint8_t offset = 0, speed = 0; // Extracted offset identity; period = 4 << speed.
    bool operator==(const FieldSpecialPose&) const = default;
};
struct FieldPoseInput {
    FieldFacing facing = FieldFacing::down;
    FieldRide ride = FieldRide::none;
    bool walking = true, moving_x = false, moving_y = false;
    bool diagonal_tile = false;
    bool party_on_ladder = false; // Caller excludes the original FF tile-property sentinel.
    std::uint16_t x = 0, y = 0;
    std::uint8_t vblank_count = 0, logic_count = 0;
    std::optional<FieldSpecialPose> special;
};
struct FieldPoseTables {
    std::array<std::uint8_t, 16> vehicle{}, walking{};
    std::array<std::uint8_t, 4> standing{};
    std::array<std::uint8_t, 8> special{};
};
// An ordinary stopped actor retains its previous pending pose.
std::uint8_t select_field_pose(const FieldPoseTables& tables, const FieldPoseInput& input,
                               std::uint8_t previous);
using FieldSpriteTiles = std::array<Tile, 6>;
inline constexpr std::size_t field_sprite_graphics_count = 165;
struct FieldSpriteAssets {
    FieldPoseTables selection;
    std::array<bool, 128> upper_flip{}, lower_flip{};
    std::array<std::array<std::uint16_t, 6>, 58> pose_tiles{};
    std::array<std::uint32_t, field_sprite_graphics_count> graphics{}; // Normalized tile indices.
    std::vector<Tile> tiles;
    std::array<std::array<Rgb555, 16>, 32> palettes{};
    FieldSpriteTiles pose(std::uint8_t graphics_id, std::uint8_t pose_id) const;
};
FieldSpriteAssets load_field_sprites(const ContentPack& pack);
struct FieldSpriteObject {
    std::uint8_t graphics = 0, pending_pose = 0, pose = 0;
    bool operator==(const FieldSpriteObject&) const = default;
};
using FieldSpriteQueue = std::array<std::optional<std::uint8_t>, 24>;
void latch_field_poses(std::uint8_t event_visits, const FieldSpriteQueue& queue,
                       std::span<FieldSpriteObject> objects);
class FieldSpritePublication {
  public:
    // Caller owns objects, queue membership and event scheduling. Publication
    // samples live owners/graphics and their previously latched poses.
    void publish(std::uint8_t event_visits, const FieldSpriteAssets& assets,
                 const FieldSpriteQueue& queue, std::span<const FieldSpriteObject> objects);
    const std::array<std::optional<FieldSpriteTiles>, 24>& visible() const {
        return visible_;
    }

  private:
    std::array<std::optional<FieldSpriteTiles>, 24> visible_;
};
struct FieldSpriteAppearance {
    std::uint8_t palette = 0, priority = 2;
    bool flip_x = false, flip_y = false;
};
struct FieldSpritePosition {
    std::uint16_t x = 0, y = 0, scroll_x = 0, scroll_y = 0, shake_y = 0, jump_y = 0;
};
// Upper then lower piece. The caller must merge pieces into the recovered game
// ordering, not simply sort actors by their background depth.
std::vector<RasterSprite>
make_field_character(const FieldSpriteAssets& assets, const FieldSpriteTiles& tiles,
                     std::uint8_t latched_pose, const FieldSpritePosition& position,
                     FieldSpriteAppearance upper, FieldSpriteAppearance lower,
                     const std::array<std::array<Rgb555, 16>, 8>& colors, CompositeBlend mix);
SpriteLayer make_field_sprite_layer(std::vector<RasterSprite> sprites, FieldLayerStyle style);
} // namespace ff6
