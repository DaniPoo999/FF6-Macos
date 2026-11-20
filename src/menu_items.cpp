#include "ff6/menu_items.hpp"
#include <algorithm>

namespace ff6 {
std::vector<std::uint8_t> menu_item_users(const Roster& roster, std::uint16_t initialized,
                                         const MenuItem& item) {
    std::vector<std::uint8_t> users;
    // C3/85D5 scans properties in equip-mask order and stops at the first slot
    // with matching properties, even when that slot is not initialized.
    for (unsigned property = 0; property < 14; ++property)
        if (item.characters & (1U << property))
            for (unsigned who = 0; who < roster.characters.size(); ++who)
                if (roster.characters[who].properties == property) {
                    if (initialized & (1U << who))
                        users.push_back(static_cast<std::uint8_t>(who));
                    break;
                }
    return users;
}
namespace {
constexpr std::uint8_t rename_card = 231, tonic = 232, potion = 233, x_potion = 234, tincture = 235,
                       ether = 236, x_ether = 237, elixir = 238, megalixir = 239, revive = 240,
                       revivify = 241, antidote = 242, eyedrop = 243, soft = 244, remedy = 245,
                       sleeping_bag = 246, tent = 247, green_cherry = 248, warp_stone = 253,
                       dried_meat = 254;
bool target_accepts(std::uint8_t item, const Character& target) {
    const auto status = target.status;
    if (status & 128U)
        return item == revive;
    const bool blocked = (status & (128U | 64U | 2U)) != 0;
    const bool hp = target.hp.current < maximum_vital(target.hp, 9999);
    const bool mp = target.mp.current < maximum_vital(target.mp, 999);
    switch (item) {
    case tonic:
    case potion:
    case x_potion:
    case dried_meat:
        return !blocked && hp;
    case tincture:
    case ether:
    case x_ether:
        return !blocked && mp;
    case elixir:
        return !blocked && (hp || mp);
    case revivify:
        return (status & 2U) != 0;
    case antidote:
        return (status & 4U) != 0;
    case eyedrop:
        return (status & 1U) != 0;
    case soft:
        return (status & 64U) != 0;
    case green_cherry:
        return (status & 32U) != 0;
    case remedy:
        return (status & (64U | 32U | 4U | 1U)) != 0;
    case sleeping_bag:
        return (status & 0xf7U) != 0 || (target.persistent_effects & 128U) || hp || mp;
    default:
        return false;
    }
}
void return_item(Inventory& inventory, std::uint8_t item) {
    // Menu IncItemQty ignores the empty identity, unlike the field-event primitive.
    if (item != 255)
        give_inventory_item(inventory, item);
}
void update_vitals(Character& character, const PartyData& party, const EquipmentData& data) {
    const auto stats = calculate_equipment(character, party, data);
    character.hp.boost = stats.hp_boost;
    character.mp.boost = stats.mp_boost;
}
} // namespace
MenuItemData load_menu_items(const ContentPack& pack) {
    const auto records = pack.get("data_d85000_item_prop");
    const auto names = pack.get("text_item_name_bin");
    if (records.size() != 256 * 30 || names.size() != 256 * 13)
        throw std::invalid_argument("Invalid menu item tables");
    MenuItemData result;
    const auto excluded = pack.get("data_ed82e4_imp_item");
    if (excluded.size() != 16)
        throw std::invalid_argument("Invalid optimum exclusion table");
    std::copy_n(excluded.begin(), 10, result.optimum_excluded.begin());
    for (unsigned i = 0; i < 256; ++i) {
        const auto r = records.subspan(i * 30, 30);
        result.items[i] = {static_cast<std::uint8_t>(r[0] & 7U),
                           names[i * 13],
                           r[20],
                           r[19],
                           r[21],
                           r[24],
                           read16(r, 1),
                           (r[0] & 64U) != 0,
                           (r[19] & 64U) != 0,
                           (r[2] & 64U) != 0,
                           r[3],
                           r[4]};
    }
    return result;
}
bool field_item_available(const MenuItemData& data, std::uint8_t item, FieldItemPermissions p) {
    if (item == 255 || item == megalixir)
        return false;
    if (item == tent || item == sleeping_bag)
        return p.rest;
    if (item == warp_stone)
        return p.warp;
    return data.items[item].type == 6 && data.items[item].field_use;
}
FieldItemResult use_field_item(const MenuItemData& data, Inventory& inventory, Character& target,
                               std::uint8_t slot, FieldItemPermissions permissions) {
    const auto id = inventory.slots[slot].item;
    if (!inventory.slots[slot].quantity || !field_item_available(data, id, permissions))
        return FieldItemResult::invalid;
    FieldItemResult result = FieldItemResult::applied;
    if (id == tent)
        result = FieldItemResult::tent;
    else if (id == warp_stone)
        result = FieldItemResult::warp;
    else if (id == rename_card) {
        if (!target.properties || *target.properties >= 14)
            return FieldItemResult::invalid;
        result = FieldItemResult::naming;
    } else if (!target_accepts(id, target))
        return FieldItemResult::invalid;
    auto next = target;
    if (result == FieldItemResult::applied) {
        const auto& item = data.items[id];
        // CalcMagicEffect's item path sets/removes persistent status bits; it
        // deliberately does not apply combat damage or status-immunity rules.
        if (item.effects & 32U) {
            next.status &= static_cast<std::uint8_t>(~item.status);
            next.persistent_effects &= static_cast<std::uint8_t>(~item.persistent_effects);
        } else {
            next.status |= item.status;
            next.persistent_effects |= item.persistent_effects;
        }
        const auto restore = [&](VitalStat& vital, std::uint16_t cap) {
            const auto maximum = maximum_vital(vital, cap);
            const unsigned amount =
                (item.effects & 128U) ? unsigned{maximum} * item.power / 16U : item.power;
            vital.current = std::min(maximum, wrap16(unsigned{vital.current} + amount));
        };
        if (item.effects & 8U)
            restore(next.hp, 9999);
        if (item.effects & 16U)
            restore(next.mp, 999);
    }
    auto stock = inventory;
    take_inventory_item(stock, id);
    inventory = std::move(stock);
    target = std::move(next);
    return result;
}
void arrange_menu_inventory(const MenuItemData& data, Inventory& inventory) {
    auto next = inventory;
    next.slots = {};
    unsigned output = 0;
    // Stable icon groups: unmarked consumables first, then the sixteen symbols.
    // No alphabetical sort, duplicate merge or invented quantity normalization.
    for (unsigned group = 0; group < 17; ++group)
        for (const auto slot : inventory.slots)
            if (slot.item != 255 && data.items[slot.item].icon == (group ? 0xd7U + group : 255U))
                next.slots[output++] = slot;
    const auto count = std::ranges::count_if(inventory.slots, [](auto v) { return v.item != 255; });
    if (output != count)
        throw std::invalid_argument("Unknown item icon in arrangement");
    inventory = std::move(next);
}
void swap_menu_items(Inventory& inventory, std::uint8_t first, std::uint8_t second) {
    std::swap(inventory.slots[first], inventory.slots[second]);
}
bool menu_can_equip(const MenuItemData& data, const Character& character,
                    const EquipmentStats& stats, unsigned slot, std::uint8_t item) {
    if (slot >= 6)
        throw std::invalid_argument("Invalid equipment slot");
    if (item == 255)
        return true;
    if (!character.properties || *character.properties >= 16)
        return false;
    const auto mask =
        (1U << *character.properties) | ((stats.relic_effects[3] & 32U) ? 0x8000U : 0U);
    const auto& record = data.items[item];
    if (!(record.characters & mask))
        return false;
    if (slot >= 4)
        return record.type == 5;
    if (slot == 2)
        return record.type == 4;
    if (slot == 3)
        return record.type == 2;
    if (record.type != 1 && record.type != 3)
        return false;
    const auto other = character.equipment[slot ^ 1U];
    if (other == 255)
        return true;
    const auto other_type = data.items[other].type;
    return record.type != other_type || (other_type != 3 && (stats.relic_effects[3] & 16U));
}
std::vector<std::uint8_t> menu_equipment_candidates(const MenuItemData& data,
                                                    const Inventory& inventory,
                                                    const Character& character,
                                                    const EquipmentStats& stats, unsigned slot) {
    if (slot >= 6)
        throw std::invalid_argument("Invalid equipment slot");
    std::vector<std::uint8_t> result;
    if (!character.properties || *character.properties >= 16)
        return result;
    const auto mask =
        (1U << *character.properties) | ((stats.relic_effects[3] & 32U) ? 0x8000U : 0U);
    // GetValidEquip scans slots 0..254. SortValidEquip is a stable descending
    // power sort: ties retain stock order. Hand conflicts remain visible.
    for (unsigned i = 0; i < (slot >= 4 ? 256U : 255U); ++i) {
        const auto id = inventory.slots[i].item;
        const auto& r = data.items[id];
        const bool type = slot < 2    ? (r.type == 1 || r.type == 3)
                          : slot == 2 ? r.type == 4
                          : slot == 3 ? r.type == 2
                                      : r.type == 5;
        if (id != 255 && type && (r.characters & mask))
            result.push_back(static_cast<std::uint8_t>(i));
    }
    std::stable_sort(result.begin(), result.end(), [&](auto a, auto b) {
        return data.items[inventory.slots[a].item].power >
               data.items[inventory.slots[b].item].power;
    });
    return result;
}
bool change_menu_equipment(const MenuItemData& data, const PartyData& party,
                           const EquipmentData& equipment, Inventory& inventory,
                           Character& character, unsigned slot,
                           std::optional<std::uint8_t> inventory_slot) {
    if (slot >= 6)
        throw std::invalid_argument("Invalid equipment slot");
    const auto id = inventory_slot ? inventory.slots[*inventory_slot].item : std::uint8_t{255};
    if (inventory_slot && (id == 255 || !inventory.slots[*inventory_slot].quantity))
        return false;
    if (!menu_can_equip(data, character, calculate_equipment(character, party, equipment), slot,
                        id))
        return false;
    auto next = character;
    auto stock = inventory;
    return_item(stock, next.equipment[slot]);
    if (id != 255)
        take_inventory_item(stock, id);
    next.equipment[slot] = id;
    update_vitals(next, party, equipment);
    character = std::move(next);
    inventory = std::move(stock);
    return true;
}
void empty_menu_equipment(Inventory& inventory, Character& character, bool relics) {
    auto stock = inventory;
    auto next = character;
    for (unsigned i = relics ? 4 : 0; i < (relics ? 6U : 4U); ++i) {
        return_item(stock, next.equipment[i]);
        next.equipment[i] = 255;
    }
    inventory = std::move(stock);
    character = std::move(next);
}
void optimize_menu_equipment(const MenuItemData& data, const PartyData& party,
                             const EquipmentData& equipment, Inventory& inventory,
                             Character& character) {
    auto stock = inventory;
    auto next = character;
    const auto stats = calculate_equipment(next, party, equipment);
    empty_menu_equipment(stock, next);
    const auto effects = stats.relic_effects[3];
    for (unsigned slot = 0; slot < 4; ++slot) {
        if (slot == 1 && (effects & 8U))
            continue;
        auto candidates = menu_equipment_candidates(data, stock, next, stats, slot);
        std::erase_if(candidates, [&](auto i) {
            const auto& r = data.items[stock.slots[i].item];
            return std::ranges::find(data.optimum_excluded, stock.slots[i].item) !=
                       data.optimum_excluded.end() ||
                   !stock.slots[i].quantity ||
                   (slot < 2 && r.type != (slot == 0 || (effects & 16U) ? 1 : 3));
        });
        std::stable_sort(candidates.begin(), candidates.end(), [&](auto a, auto b) {
            return data.items[stock.slots[a].item].power > data.items[stock.slots[b].item].power;
        });
        if (candidates.empty())
            continue;
        auto choice = candidates.begin();
        if (slot == 0 && (effects & 8U)) {
            const auto two_hand = std::ranges::find_if(
                candidates, [&](auto i) { return data.items[stock.slots[i].item].two_handed; });
            if (two_hand != candidates.end())
                choice = two_hand;
        }
        const auto id = stock.slots[*choice].item;
        take_inventory_item(stock, id);
        next.equipment[slot] = id;
    }
    update_vitals(next, party, equipment);
    inventory = std::move(stock);
    character = std::move(next);
}
} // namespace ff6
