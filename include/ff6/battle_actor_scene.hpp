#pragma once
#include "ff6/battle_animation.hpp"
#include "ff6/battle_character_presentation.hpp"
#include "ff6/battle_display_transfers.hpp"
#include <variant>
#include <memory>

namespace ff6 {
using BattleCharacterPaletteBanks = std::array<std::array<Rgb555, 16>, 4>;
using BattleMonsterPaletteBanks = std::array<std::array<Rgb555, 16>, 3>;
struct BattleStatusSpriteLayout {
    std::int16_t x = 0, y = 0;
    std::uint8_t tile = 0;
    BattleSpriteStyle style;
};
struct BattleStatusSpriteAssets {
    std::array<std::uint16_t, 8> mirrored_x{};
    std::array<BattleStatusSpriteLayout, 14> layouts{};
    std::array<std::uint16_t, 32> frame_tiles{};
    std::vector<Tile> tiles;
};
BattleStatusSpriteAssets load_battle_status_sprite_assets(const ContentPack& pack);

struct BattleActorSceneAssets {
    BattleCharacterViewData view;
    BattleCharacterGraphicsData graphics;
    BattleCharacterPaletteData palettes;
    BattleMonsterGraphics monsters;
    MonsterSpriteVariants monster_pieces;
    std::array<std::array<Rgb555, 4>, 4> status_colors{};
    std::optional<BattleStatusSpriteAssets> status_sprites;
    // Original selections plus decoded source data for status-driven reloads.
    // The working/published image belongs to BattleActorScene.
    std::shared_ptr<const BattleMonsterGraphicsData> monster_graphics_data;
    std::optional<BattleMonsterGraphicsRequest> monster_graphics_selection;
};
enum class BattleActorAppearanceOperation { toggle_imp, vanish, replace_graphics, toggle_status };
struct BattleActorAppearanceChange {
    BattleActorAppearanceOperation operation;
    std::uint8_t participant = 0;
    std::uint8_t graphics = 255; // Used only by replace_graphics.
    std::uint32_t status = 0; // Used only by the authored status toggle.
    bool operator==(const BattleActorAppearanceChange&) const = default;
};
using BattleMonsterTiles = std::array<Tile, 256>;
struct BattleMonsterImageUpdate {
    BattleMonsterGraphicsRequest selection;
    std::shared_ptr<const BattleMonsterTiles> prepared, visible;
    unsigned published_tiles = 256;
};
struct BattleCharacterScenePiece {
    std::uint8_t slot = 0;
    BattleCharacterDraw draw;
};
struct BattleArmorScenePiece {
    std::uint8_t slot = 0;
    BattleArmorDraw draw;
};
struct BattleMonsterScenePiece {
    std::uint8_t slot = 0;
    BattleMonsterDrawPiece draw;
};
struct BattleStatusScenePiece {
    std::uint8_t slot = 0, tile = 0;
    BattleScreenPoint position;
    BattleSpriteStyle style;
};
using BattleActorScenePiece =
    std::variant<BattleCharacterScenePiece, BattleArmorScenePiece, BattleMonsterScenePiece,
                 BattleStatusScenePiece>;
struct BattleArmorImageRequest {
    std::uint8_t slot = 0, frame = 0;
    bool operator==(const BattleArmorImageRequest&) const = default;
};
// Preserve participant boundaries until effects have been interleaved. Armor
// and rider pieces belong to the same participant and retain their local order.
struct BattleActorSprite {
    RasterSprite image;
    bool color_math = true;
};
struct BattleActorSpriteGroup {
    std::uint8_t participant = 0;
    std::vector<BattleActorSprite> sprites;
};
struct BattleActorPublication {
    // Descriptions contain no pixels. A publication joins the previous pieces
    // with separately published character images, armor selections and palettes.
    std::vector<BattleActorScenePiece> prepared, visible;
    std::array<BattleCharacterFrame, 4> character_frames{};
    std::array<std::optional<std::uint8_t>, 4> armor_frames{};
    BattleCharacterPaletteBanks colors{};
    std::optional<BattleMonsterPaletteBanks> monster_colors;
    std::optional<BattleArmorImageRequest> pending_armor;
    std::shared_ptr<const BattleMonsterTiles> monster_tiles;
    std::array<Tile, 32> status_tiles{};
    BattleDisplayTransfers transfers;
};
struct BattleActorScene {
    BattleCharacterPresentation characters;
    BattleMonsterPositions monsters;
    std::array<BattleMonsterStyle, 6> monster_styles{};
    BattleSceneAnchors anchors;
    BattleDrawOrder order;
    BattleAnimationControl animation;
    BattleActorPublication publication;
    // Retained from the actual preparation decisions for the future display
    // scheduler; not yet converted to elapsed clocks or raster deadlines.
    BattleCharacterImageWork image_preparation_work;
    // Ordinary status updates replace only colors 0..11. Character ADD fades
    // also write 12..15, which remain retained across later SUB/status updates.
    std::optional<std::array<std::array<Rgb555, 4>, 4>> retained_status_colors;
    std::optional<BattleMonsterPaletteBanks> monster_colors;
    std::optional<BattleMonsterImageUpdate> monster_images;
    std::array<std::uint8_t, 4> rider_offsets{};
    // Eight prior character descriptions, sampled on the authored clock mask.
    // Echoes use current published images/palettes, not snapshots of pixels.
    std::array<std::optional<BattleCharacterScenePiece>, 8> echo_history{};
    std::uint32_t echo_reset_serial = 0;
    // This clock drives armor selection and palette effects. Palette/image
    // round-robin phase is characters.layout.palette_phase, a separate byte.
    std::uint16_t clock = 0;
    // Image draws one character's offset copy per preparation pass.
    std::uint8_t image_phase = 0;
    std::uint8_t condemned_phase = 0;
    std::array<std::array<Tile, 2>, 4> condemned_digits{}, condemned_flipped{};
};
struct BattleActorSceneRules {
    bool magitek = false, battle_time_stopped = false, freeze_palettes = false;
    bool hide_characters = false;
    std::array<bool, 4> shown_characters{}, enemy_characters{};
    std::array<bool, 6> shown_monsters{};
    // The status/image visibility gate is independent of script hide/show and
    // entrance publication (C1/27C2). Clear can change it during an animation.
    std::bitset<6> invisible_monsters;
    std::array<std::uint8_t, 6> retained_monster_facing{};
    std::array<std::uint8_t, 4> condemned{};
    bool forced_running_background = false, run_buttons = false;
    BattleCharacterAppearanceRules appearance;
};
// These are presentation changes requested by the animation data language.
// Combat status resolution remains a separate owner. Staged monster changes
// publish 32 decoded tiles per boundary while the action suspends its tasks.
void apply_battle_actor_appearance_changes(const BattleActorSceneAssets& assets,
                                          const BattleActorSceneRules& rules,
                                          std::span<const BattleActorAppearanceChange> changes,
                                          BattleActorScene& state, bool staged = true);
BattleCharacterPaletteBanks battle_character_colors(const BattleActorScene& state,
                                                    const BattleActorSceneAssets& assets);
BattleMonsterPaletteBanks battle_monster_colors(const BattleActorScene& state,
                                                const BattleActorSceneAssets& assets);
void set_battle_character_colors(BattleActorScene& state,
                                 const BattleCharacterPaletteBanks& colors);
// Publication precedes preparation in the original scene frame. A pending armor
// image postpones the extra forced character image, preserving that request.
void publish_battle_actor_scene(BattleActorScene& state, const BattleActorSceneAssets& assets,
                                bool hide_characters);
// Scoped ordinary/Magitek actor pass with explicit visibility. Rejects overlays
// whose separate owners are not implemented. Does not advance game/battle time,
// run animation commands, stage a new draw order, or service menus/fades.
void prepare_battle_actor_scene(const BattleActorSceneAssets& assets,
                                const BattleActorSceneRules& rules, BattleActorScene& state);
std::vector<RasterSprite> make_battle_actor_sprites(const BattleActorSceneAssets& assets,
                                                    const BattleActorPublication& publication,
                                                    const BattleActorPaletteOverrides& colors = {});
std::vector<BattleActorSpriteGroup>
make_battle_actor_sprite_groups(const BattleActorSceneAssets& assets,
                                const BattleActorPublication& publication,
                                const BattleActorPaletteOverrides& colors = {});
} // namespace ff6
