#pragma once
#include "ff6/battle_commands.hpp"
#include "ff6/inventory.hpp"

namespace ff6 {
struct BattleItemDefinition {
    std::uint8_t usage_flags = 128, targeting = 0;
    // Absent means this item category never runs the equipment-eligibility check.
    std::optional<std::uint16_t> equipment_mask;
};
struct BattleInventoryData {
    std::array<BattleItemDefinition, 256> items;
};
BattleInventoryData decode_battle_inventory(ByteView items, ByteView type_flags);
BattleInventoryData load_battle_inventory(const ContentPack& pack);
struct BattleItem {
    std::uint8_t identity = 0, usage_flags = 128, targeting = 0;
    std::optional<std::bitset<4>> equippable_by;
    bool operator==(const BattleItem&) const = default;
};
struct BattleItemStack {
    std::optional<BattleItem> item;
    std::uint8_t quantity = 0;
    bool operator==(const BattleItemStack&) const = default;
};
struct BattleInventory {
    std::uint32_t gil = 0;
    std::array<BattleItemStack, 256> items;
    std::array<std::uint16_t, 4> actor_masks{};
    std::vector<BattleItemStack> obtained;
    // One optional pair per occupied actor: right hand, then left hand. A real
    // character's empty hand is distinct from an absent character's hand list.
    std::array<std::optional<std::array<BattleItemStack, 2>>, 4> hands;
    std::bitset<8> tools_owned;
    bool operator==(const BattleInventory&) const = default;
};
// Obtained items retain their metadata and wait if the bag is full. Existing
// identities take precedence over empty slots, matching the battle menu owner.
void queue_battle_item(BattleInventory& inventory, const BattleInventoryData& data,
                       std::uint8_t identity);
void publish_battle_items(BattleInventory& inventory);
// Equipment swaps are immediate menu operations, not queued Item actions.
// Empty bag slots unequip a hand. Invalid/full native inventories are rejected
// atomically instead of reproducing out-of-bounds original buffer writes.
bool can_exchange_battle_hand(const BattleInventory&, unsigned actor, unsigned hand,
                              unsigned bag_slot, bool genji_glove);
bool exchange_battle_hand(BattleInventory&, unsigned actor, unsigned hand,
                          unsigned bag_slot, bool genji_glove);
void swap_battle_hands(BattleInventory&, unsigned actor);
BattleInventory initialize_battle_inventory(const BattleInventoryData& data,
                                            const Inventory& persistent,
                                            const BattleCharacters& characters);
} // namespace ff6
