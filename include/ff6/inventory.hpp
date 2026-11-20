#pragma once
#include <array>
#include <cstdint>

namespace ff6 {
struct InventorySlot {
    std::uint8_t item = 255, quantity = 0;
    bool operator==(const InventorySlot&) const = default;
};
struct Inventory {
    std::array<InventorySlot, 256> slots;
    std::uint32_t gil = 0; // Unsigned 24-bit persistent value.
    bool operator==(const Inventory&) const = default;
};
// Search in stored slot order. Preserve duplicate/sentinel and byte-wrap rules;
// an absent item with no empty slot would escape original storage and is rejected.
void give_inventory_item(Inventory& inventory, std::uint8_t item);
void take_inventory_item(Inventory& inventory, std::uint8_t item);
void give_inventory_gil(Inventory& inventory, std::uint16_t amount);
// Returns the original affordability result. A failed request retains gil;
// the event owner must also update its "not enough gil" story flag.
bool take_inventory_gil(Inventory& inventory, std::uint16_t amount);
} // namespace ff6
