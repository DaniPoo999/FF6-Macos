#pragma once
#include "ff6/battle_menu_navigation.hpp"
#include "ff6/battle_menu_transition.hpp"
#include "ff6/battle_readout.hpp"

namespace ff6 {
struct BattleMenuSurfaceAssets {
    BattleReadoutAssets readout;
    BattleTextNames names;
    BattleMagitekChoices magitek;
    std::array<BattleReadoutRectangle, 13> windows;
    // Indexed by BattleCommandLayout; rectangle describes bounded glyph space.
    std::array<BattleReadoutRectangle, 3> command_regions;
    std::array<Bytes, 3> command_programs;
    Bytes magitek_program, magic_program, item_program, summon_program;
    Bytes equipment_program{0};
    std::array<std::uint8_t, 2> magic_columns{};
    std::uint8_t item_column = 0;
    std::array<std::uint8_t, 21> mp_glyphs{};
    std::array<std::uint8_t, 42> mp_layout{};
    std::array<Tile, 64> slot_tiles{};
};
BattleMenuSurfaceAssets load_battle_menu_surface_assets(const ContentPack& pack);
// Output of initial window preparation, before later text/slot/status owners.
// Panels follow BattleWindowCanvas, caches follow BattleCommandLayout. The
// original prepares normal, Control and short caches in that order and uploads
// the short cache initially, regardless of the selected command layout.
struct BattleWindowInitialization {
    std::array<TileLayer, 9> panels;
    std::array<TileLayer, 3> command_cache;
};
BattleWindowInitialization
make_battle_window_initialization(const BattleMenuSurfaceAssets& assets,
                                  const BattleReadoutView& view,
                                  const std::array<Rgb555, 8>& retained_slot_colors);
struct BattleMenuSurfaces {
    std::array<std::optional<TileLayer>, 9> windows;
    std::array<std::optional<TileLayer>, 6> text;
    // Command preparation retains the scratch canvas across layout/actor changes.
    // It is distinct from the published closed-menu readout surface.
    TileLayer command_work;
    std::optional<TileLayer> mp;
};
BattleMenuSurfaces make_battle_menu_surfaces(const BattleMenuSurfaceAssets& assets,
                                             const BattleReadoutView& view);
// Prepares complete surfaces atomically; publication and input waits belong to
// the caller. selected_row is compacted text order, never a combat slot.
void prepare_battle_command_surface(BattleMenuSurfaces& surfaces,
                                    const BattleMenuSurfaceAssets& assets,
                                    const BattleCommandList& commands, BattleCommandLayout layout,
                                    std::uint8_t selected_row);
// The caller prepares/publishes one row per accepted staging visit, then opens.
void prepare_battle_magitek_row(BattleMenuSurfaces& surfaces, const BattleMenuSurfaceAssets& assets,
                                std::uint8_t row, std::uint8_t displayed_identity);
void prepare_battle_mp(BattleMenuSurfaces& surfaces, const BattleMenuSurfaceAssets& assets,
                       CombatVitals mp, std::uint8_t cost);
void prepare_battle_magic_row(BattleMenuSurfaces& surfaces, const BattleMenuSurfaceAssets& assets,
                              std::uint8_t row, const BattleSpellList& spells, unsigned first);
void prepare_battle_lore_row(BattleMenuSurfaces& surfaces, const BattleMenuSurfaceAssets& assets,
                             std::uint8_t row, const std::optional<BattleSpell>& spell);
void prepare_battle_summon_surface(BattleMenuSurfaces&, const BattleMenuSurfaceAssets&, const BattleSpell&);
void prepare_battle_equipment_surface(BattleMenuSurfaces&, const BattleMenuSurfaceAssets&,
                                       const std::array<BattleItemStack, 2>&);
void prepare_battle_item_row(BattleMenuSurfaces& surfaces, const BattleMenuSurfaceAssets& assets,
                             std::uint8_t row, const BattleItemStack& stack);
void prepare_battle_stance_surface(BattleMenuSurfaces& surfaces,
                                   const BattleMenuSurfaceAssets& assets, std::uint8_t row,
                                   bool short_form, bool defend);
void prepare_battle_bushido_surface(BattleMenuSurfaces&, const BattleMenuSurfaceAssets&,
                                    unsigned known, std::uint8_t counter);
// Scoped initialization for closed, command and list menus. Other menu families
// need their own bank initialization before their transitions can be presented.
BattleMenuTransition make_battle_command_transition();
// Project prepared native surfaces through retained per-line bindings. Missing
// surfaces fail explicitly; the function never invents placeholder menus.
std::vector<TileLayer> make_battle_menu_layers(const BattleMenuSurfaces& surfaces,
                                               const BattleMenuTransition& transition);
} // namespace ff6
