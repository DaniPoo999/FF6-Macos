#pragma once
#include "ff6/equipment.hpp"
#include "ff6/inventory.hpp"

namespace ff6 {
struct MenuItem {
    std::uint8_t type = 0, icon = 255, power = 0, effects = 0;
    std::uint8_t status = 0, persistent_effects = 0;
    std::uint16_t characters = 0;
    bool field_use = false, two_handed = false, imp = false;
    std::uint8_t learn_rate = 0, learned_spell = 255;
};
struct MenuItemData {
    std::array<MenuItem, 256> items{};
    std::array<std::uint8_t, 10> optimum_excluded{};
};
MenuItemData load_menu_items(const ContentPack& pack);
std::vector<std::uint8_t> menu_item_users(const Roster& roster, std::uint16_t initialized,
                                         const MenuItem& item);
enum class FieldItemResult { invalid, applied, naming, tent, warp };
struct FieldItemPermissions {
    bool rest = false, warp = false;
};
bool field_item_available(const MenuItemData& data, std::uint8_t item,
                          FieldItemPermissions permissions);
// Failed requests never consume stock or mutate the target. Tent/warp/naming
// are explicit handoffs; the session must complete their presentation/lifecycle.
FieldItemResult use_field_item(const MenuItemData& data, Inventory& inventory, Character& target,
                               std::uint8_t slot, FieldItemPermissions permissions);
void arrange_menu_inventory(const MenuItemData& data, Inventory& inventory);
void swap_menu_items(Inventory& inventory, std::uint8_t first, std::uint8_t second);
std::vector<std::uint8_t> menu_equipment_candidates(const MenuItemData& data,
                                                    const Inventory& inventory,
                                                    const Character& character,
                                                    const EquipmentStats& stats, unsigned slot);
bool menu_can_equip(const MenuItemData& data, const Character& character,
                    const EquipmentStats& stats, unsigned slot, std::uint8_t item);
// Transactions retain total stock except the original 99-item cap. The copy
// commits only when both returning the old item and taking the new item succeed.
bool change_menu_equipment(const MenuItemData& data, const PartyData& party,
                           const EquipmentData& equipment, Inventory& inventory,
                           Character& character, unsigned slot,
                           std::optional<std::uint8_t> inventory_slot);
void empty_menu_equipment(Inventory& inventory, Character& character, bool relics = false);
void optimize_menu_equipment(const MenuItemData& data, const PartyData& party,
                             const EquipmentData& equipment, Inventory& inventory,
                             Character& character);
} // namespace ff6
