#pragma once
#include "ff6/esper.hpp"
#include "ff6/battle_commands.hpp"
#include "ff6/menu_items.hpp"
namespace ff6 {
struct MenuSpell {
    std::uint8_t targeting = 0, elements = 0, attack_flags = 0, menu_flags = 0, effects = 0,
                 mp_cost = 0, power = 0, status = 0, persistent = 0;
};
using MenuEsper = Esper;
struct MenuSkillData {
    BattleCommandData commands;
    std::array<MenuSpell, 256> spells;
    std::array<MenuEsper, 27> espers;
    std::array<std::uint8_t, 27> esper_order;
};
MenuSkillData load_menu_skills(const ContentPack& pack);
std::uint8_t menu_magic_progress(const Roster& roster, std::uint8_t character, std::uint8_t spell);
std::uint8_t menu_spell_cost(const MenuSkillData& data, std::uint8_t spell,
                             const EquipmentStats& stats, GameTime time);
bool menu_spell_available(const MenuSkillData& data, const Roster& roster, std::uint8_t caster,
                          const EquipmentStats& stats, std::uint8_t spell,
                          FieldItemPermissions permissions);
bool menu_spell_accepts(std::uint8_t spell, const Character& target);
// Atomic MP payment and effects. Multiple targets share one payment; invalid
// targets are skipped, and no valid target means no charge. Warp is a field handoff.
FieldItemResult use_menu_spell(const MenuSkillData& data, Roster& roster, std::uint8_t caster,
                               const EquipmentStats& stats, std::uint8_t spell,
                               std::span<const std::uint8_t> targets, bool multiple,
                               FieldItemPermissions permissions);
bool equip_menu_esper(Roster& roster, std::uint8_t character, std::optional<std::uint8_t> esper);
std::vector<std::uint8_t> menu_gogo_commands(const Roster& roster, std::uint16_t initialized,
                                           const MenuSkillData& data);
unsigned menu_attack_power(const Character& character, const MenuItemData& items,
                           const EquipmentStats& stats);
} // namespace ff6
