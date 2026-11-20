#pragma once
#include "rom-materials/battle_character_default_status.macros.inc"
#include "rom-materials/battle_character_default_armor_palette.macros.inc"
#include "ff6/battle_character_graphics.hpp"
#include "ff6/battle_drawing.hpp"

namespace ff6 {
struct BattleCharacterPieceLayout {
    std::int16_t x = 0, y = 0;
    std::uint8_t part = 0;
    bool flip_x = false, flip_y = false;
};
struct BattleCharacterViewData {
    std::array<std::array<std::uint8_t, 4>, 39> actions{};
    std::array<std::array<std::array<BattleCharacterPieceLayout, 2>, 2>, 4> pieces{};
    std::array<std::array<std::uint16_t, 4>, 4> row_offsets{}, angles{};
    std::array<std::array<std::uint8_t, 4>, 5> y{}, facing{};
    std::array<std::array<bool, 4>, 4> swap_hands{};
    std::array<std::uint8_t, 4> armor_palettes{};
    std::array<std::int16_t, 8> floating_offsets{};
    std::array<BattleScreenPoint, 4> armor_offsets{};
};
BattleCharacterViewData load_battle_character_view_data(const ContentPack& pack);
struct BattleCharacterPose {
    std::uint16_t x = 0, y = 0, offset_x = 0, offset_y = 0, animation_x = 0, jump_y = 0;
    std::uint16_t desired_angle = 0, current_angle = 0;
    BattleSpriteStyle appearance{};
    std::uint8_t facing = 0, status_action = FF6_ROM_VALUE_BATTLE_CHARACTER_DEFAULT_STATUS_0, action = 0, frame_override = 0;
    std::uint8_t counter = 0, current_frame = 0;
    bool double_speed = false, disable_float = false;
    // Seize's original placement survives other actions until Discard.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> seize_origin;
    bool operator==(const BattleCharacterPose&) const = default;
};
struct BattleCharacterPlacementOverride {
    std::uint16_t angle = 0, y = 0;
};
struct BattleCharacterLayoutRequest {
    BattleApproach approach = BattleApproach::normal;
    bool magitek_train = false, colosseum = false, magitek = false, green_soldier_disguise = false;
    std::array<std::uint8_t, 4> graphics{255, 255, 255, 255}, characters{255, 255, 255, 255};
    std::array<bool, 4> back_row{}, enemy{};
    std::array<std::optional<BattleCharacterPlacementOverride>, 4> overrides{};
};
struct BattleCharacterLayout {
    std::array<BattleCharacterPose, 4> poses{};
    std::array<BattleCharacterGraphics, 4> graphics{};
    std::array<BattleCharacterColors, 4> visible_colors{};
    std::array<bool, 4> back_row{}, swap_hands{};
    std::array<std::uint8_t, 4> graphics_ids{255, 255, 255, 255};
    std::uint8_t armor_palette = FF6_ROM_VALUE_BATTLE_CHARACTER_DEFAULT_ARMOR_PALETTE_0 / 2U, palette_phase = 0;
};
// Complete semantic InitCharGfxMain state; character x publication remains a
// separate step, including its cached-angle equality check.
void initialize_battle_character_layout(const BattleCharacterViewData& view,
                                        const BattleCharacterGraphicsData& graphics,
                                        const BattleCharacterLayoutRequest& request,
                                        BattleCharacterLayout& state);
// Original asymmetric integer projection; y is the retained low coordinate byte.
std::uint16_t calculate_battle_character_x(std::uint8_t y, std::uint16_t angle);
void update_battle_character_x(std::array<BattleCharacterPose, 4>& poses);
struct BattleCharacterDrawContext {
    std::uint32_t shown_status = 0;
    bool image_copy = false, run_animation_blocked = false, forced_running_background = false;
    bool run_buttons = false, blocking_animation = false, acting = false, enemy = false;
    bool magitek = false, battle_time_stopped = false;
};
struct BattleCharacterDrawPiece {
    BattleScreenPoint position;
    std::uint8_t part = 0;
    BattleSpriteStyle style;
};
// Shared float/Magitek displacement for character drawing and its active arrow.
std::int16_t battle_character_y_offset(const BattleCharacterViewData& data,
                                       std::uint32_t shown_status, const BattleCharacterPose& pose,
                                       bool magitek);
using BattleCharacterDraw = std::array<BattleCharacterDrawPiece, 2>;
// Selects a raw frame (including mirrored/blank identities), publishes the
// counter/current frame, and projects two pieces. Does not publish image tiles.
BattleCharacterDraw draw_battle_character(const BattleCharacterViewData& data,
                                          const BattleCharacterDrawContext& context,
                                          BattleCharacterPose& pose);
struct BattleArmorDraw {
    BattleScreenPoint position;
    BattleSpriteStyle style;
    std::optional<BattleMagitekFrameSelection> frame_update;
};
// Returns an optional frame publication request; other slots retain their image.
// Applies the retained rider displacement to the already projected pieces.
BattleArmorDraw draw_battle_armor(const BattleCharacterViewData& view,
                                  const BattleCharacterGraphicsData& graphics,
                                  const BattleCharacterPose& pose, std::uint8_t slot,
                                  std::uint8_t action, std::uint8_t counter, bool time_stopped,
                                  std::uint8_t palette, std::uint8_t& retained_rider_offset,
                                  BattleCharacterDraw& rider);
std::array<RasterSprite, 2> make_battle_character_sprites(const BattleCharacterDraw& draw,
                                                          const BattleCharacterFrame& frame,
                                                          const std::array<Rgb555, 16>& colors);
RasterSprite make_battle_armor_sprite(const BattleArmorDraw& draw,
                                      const std::array<Tile, 16>& frame,
                                      const std::array<Rgb555, 16>& colors);
} // namespace ff6
