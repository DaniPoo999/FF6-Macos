#include "ff6/inventory.hpp"
#include <algorithm>
#include <stdexcept>

namespace ff6 {
void give_inventory_item(Inventory& inventory, std::uint8_t item) {
    const auto found = std::ranges::find(inventory.slots, item, &InventorySlot::item);
    if (found != inventory.slots.end()) {
        // Equality-only cap: malformed retained counts above 99 still increment.
        if (found->quantity != 99)
            found->quantity = static_cast<std::uint8_t>(unsigned{found->quantity} + 1U);
        return;
    }
    const auto empty = std::ranges::find(inventory.slots, 255, &InventorySlot::item);
    if (empty == inventory.slots.end())
        throw std::invalid_argument("Inventory has no represented empty slot for an absent item");
    *empty = {item, 1};
}
void take_inventory_item(Inventory& inventory, std::uint8_t item) {
    const auto found = std::ranges::find(inventory.slots, item, &InventorySlot::item);
    if (found == inventory.slots.end())
        return;
    found->quantity = static_cast<std::uint8_t>(unsigned{found->quantity} + 255U);
    if (found->quantity == 0)
        found->item = 255;
}
void give_inventory_gil(Inventory& inventory, std::uint16_t amount) {
    if (inventory.gil > 0xffffff)
        throw std::invalid_argument("Inventory gil exceeds its 24-bit range");
    auto total = (inventory.gil + amount) & 0xffffffU;
    // Preserve the original two independent comparisons, not a numeric min.
    constexpr std::uint32_t limit = 9999999;
    if ((total >> 16U) >= (limit >> 16U) && (total & 65535U) >= (limit & 65535U))
        total = limit;
    inventory.gil = total;
}
bool take_inventory_gil(Inventory& inventory, std::uint16_t amount) {
    if (inventory.gil > 0xffffff)
        throw std::invalid_argument("Inventory gil exceeds its 24-bit range");
    const auto remaining = (inventory.gil + 0x1000000U - amount) & 0xffffffU;
    // The source tests the high byte against A0, rather than signedness/borrow.
    if ((remaining >> 16U) >= 0xa0)
        return false;
    inventory.gil = remaining;
    return true;
}
} // namespace ff6
