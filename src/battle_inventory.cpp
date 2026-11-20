#include "ff6/battle_inventory.hpp"
#include <algorithm>

namespace ff6 {
bool can_exchange_battle_hand(const BattleInventory& inventory, unsigned actor, unsigned hand,
                              unsigned bag_slot, bool genji_glove) {
    if (actor >= 4 || hand >= 2 || bag_slot >= inventory.items.size() || !inventory.hands[actor])
        return false;
    const auto& incoming = inventory.items[bag_slot];
    const auto& held = (*inventory.hands[actor])[hand];
    const auto& other = (*inventory.hands[actor])[hand ^ 1U];
    if (held.item && incoming.item && held.item->identity == incoming.item->identity) return true;
    if (incoming.item) {
        if (!incoming.quantity || !(incoming.item->usage_flags & 24U) ||
            !incoming.item->equippable_by || !(*incoming.item->equippable_by)[actor]) return false;
        if (other.item) {
            // C1/89D5: Genji permits two weapons but never two shields.
            // Without it, two occupied hands must be weapon plus shield.
            const auto a = incoming.item->usage_flags, b = other.item->usage_flags;
            if (genji_glove ? ((a & b & 8U) != 0) : (((a | b) & 24U) != 24U)) return false;
        }
    }
    if (!held.item || incoming.quantity <= 1) return true;
    return std::ranges::any_of(inventory.items, [&](const auto& slot) {
        return !slot.item || slot.item->identity == held.item->identity;
    });
}
bool exchange_battle_hand(BattleInventory& inventory, unsigned actor, unsigned hand,
                          unsigned bag_slot, bool genji_glove) {
    if (!can_exchange_battle_hand(inventory, actor, hand, bag_slot, genji_glove)) return false;
    auto& held = (*inventory.hands[actor])[hand];
    auto& incoming = inventory.items[bag_slot];
    if (!held.item && !incoming.item) return true;
    if (held.item && incoming.item && held.item->identity == incoming.item->identity) {
        // C1/8B53 and /8C08: selecting the held identity removes that hand.
        // Return its copy to the selected bag stack, with the original 99 cap.
        held = {};
        incoming.quantity = static_cast<std::uint8_t>(std::min(99U, unsigned{incoming.quantity} + 1U));
        return true;
    }
    const auto previous = held;
    held = {incoming.item, static_cast<std::uint8_t>(incoming.item ? 1 : 0)};
    if (incoming.quantity >= 2) --incoming.quantity;
    else incoming = {};
    if (previous.item) {
        auto found = std::ranges::find_if(inventory.items, [&](const auto& slot) {
            return slot.item && slot.item->identity == previous.item->identity;
        });
        if (found != inventory.items.end())
            found->quantity = static_cast<std::uint8_t>(std::min(99U, unsigned{found->quantity} + 1U));
        else {
            found = std::ranges::find_if(inventory.items, [](const auto& slot) { return !slot.item; });
            *found = {previous.item, 1}; // Capacity was established before mutation.
        }
    }
    return true;
}
void swap_battle_hands(BattleInventory& inventory, unsigned actor) {
    if (actor >= 4 || !inventory.hands[actor]) throw std::invalid_argument("Missing battle hand pair");
    std::swap((*inventory.hands[actor])[0], (*inventory.hands[actor])[1]);
}
BattleInventoryData decode_battle_inventory(ByteView items, ByteView type_flags) {
    if (items.size() != 256 * 30 || type_flags.size() != 8)
        throw std::invalid_argument("Invalid battle inventory table extent");
    BattleInventoryData data;
    for (std::size_t i = 0; i < data.items.size(); ++i) {
        const auto record = items.subspan(i * 30, 30);
        const auto flags = type_flags[record[0] & 7U];
        auto& out = data.items[i];
        // Game menu flags: unusable, throwable, tool, weapon and shield. These
        // are item properties, not device/register state.
        out.usage_flags = static_cast<std::uint8_t>(((record[0] & 32U) != 0 ? 0U : 128U) |
                                                    ((record[0] & 16U) != 0 ? 32U : 0U) |
                                                    wrap8(unsigned{flags} * 2U));
        out.targeting = record[14];
        if ((flags & 128U) == 0)
            out.equipment_mask = read16(record, 1);
    }
    return data;
}
BattleInventoryData load_battle_inventory(const ContentPack& pack) {
    return decode_battle_inventory(pack.get("data_d85000_item_prop"),
                                   pack.get("battle_item_type_flags"));
}
BattleInventory initialize_battle_inventory(const BattleInventoryData& data,
                                            const Inventory& persistent,
                                            const BattleCharacters& characters) {
    auto prepare = [&](std::uint8_t identity, std::uint8_t quantity) {
        BattleItemStack out;
        out.quantity = quantity;
        if (identity == 255)
            return out;
        const auto& source = data.items[identity];
        BattleItem item{identity, source.usage_flags, source.targeting, {}};
        if (source.equipment_mask) {
            item.equippable_by.emplace();
            for (std::size_t i = 0; i < characters.slots.size(); ++i) {
                const auto& character = characters.slots[i];
                (*item.equippable_by)[i] =
                    character && ((*source.equipment_mask & character->combat.actor_mask) != 0);
            }
        }
        out.item = item;
        return out;
    };
    BattleInventory result;
    result.gil = persistent.gil;
    for (unsigned i = 0; i < 4; ++i)
        if (characters.slots[i])
            result.actor_masks[i] = characters.slots[i]->combat.actor_mask;
    for (std::size_t i = 0; i < result.items.size(); ++i) {
        const auto& slot = persistent.slots[i];
        result.items[i] = prepare(slot.item, slot.quantity);
        // Tools are recognized by identity even at quantity zero. The command
        // reads the bag only; an equipped tool does not supply ownership.
        if (slot.item >= 163 && slot.item < 171)
            result.tools_owned.set(slot.item - 163U);
    }
    for (std::size_t i = 0; i < characters.slots.size(); ++i) {
        const auto& character = characters.slots[i];
        if (!character)
            continue;
        const auto& equipment = character->combat.persistent.equipment;
        result.hands[i] = {prepare(equipment[0], 1), prepare(equipment[1], 1)};
    }
    return result;
}
void queue_battle_item(BattleInventory& inventory, const BattleInventoryData& data,
                       std::uint8_t identity) {
    if (identity == 255 || inventory.obtained.size() >= 16)
        throw std::invalid_argument("Invalid obtained battle item or full pending queue");
    const auto& source = data.items[identity];
    BattleItem item{identity, source.usage_flags, source.targeting, {}};
    if (source.equipment_mask) {
        item.equippable_by.emplace();
        for (unsigned i = 0; i < 4; ++i)
            (*item.equippable_by)[i] = (*source.equipment_mask & inventory.actor_masks[i]) != 0;
    }
    inventory.obtained.push_back({item, 1});
}
void publish_battle_items(BattleInventory& inventory) {
    while (!inventory.obtained.empty()) {
        const auto& gained = inventory.obtained.front();
        auto target = std::ranges::find_if(inventory.items, [&](const auto& slot) {
            return slot.item && slot.item->identity == gained.item->identity;
        });
        if (target != inventory.items.end())
            target->quantity =
                std::min<std::uint8_t>(99, wrap8(unsigned{target->quantity} + gained.quantity));
        else {
            target =
                std::ranges::find_if(inventory.items, [](const auto& slot) { return !slot.item; });
            if (target == inventory.items.end())
                return;
            *target = gained;
        }
        if (gained.item->identity >= 163 && gained.item->identity <= 170)
            inventory.tools_owned.set(gained.item->identity - 163);
        inventory.obtained.erase(inventory.obtained.begin());
    }
}
} // namespace ff6
