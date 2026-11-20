#include "ff6/colosseum.hpp"
#include "rom-materials/colosseum_recruitment_wager.macros.inc"
#include <stdexcept>

namespace ff6 {
ColosseumData decode_colosseum_data(ByteView bytes) {
    if (bytes.size() != 1024) throw std::invalid_argument("Invalid Coliseum table extent");
    ColosseumData result;
    for (unsigned i = 0; i < result.size(); ++i)
        // C3/B22C loads only the low opponent byte; byte 1 is not an index extension.
        result[i] = {bytes[i * 4], bytes[i * 4 + 2], bytes[i * 4 + 3] != 0};
    return result;
}
ColosseumSelection select_colosseum_wager(const ColosseumData& data, const Inventory& inventory,
                                          unsigned slot, bool shadow_waiting) {
    if (slot >= inventory.slots.size() || inventory.slots[slot].item == 255 ||
        !inventory.slots[slot].quantity)
        throw std::invalid_argument("Coliseum wager is not in inventory");
    const auto item = inventory.slots[slot].item;
    return {item, data[item].prize, data[item].monster, 255,
            shadow_waiting && item == FF6_ROM_VALUE_COLOSSEUM_RECRUITMENT_WAGER_0};
}
void select_colosseum_challenger(ColosseumSelection& selection, const Roster& roster,
                                 unsigned position) {
    if (selection.wager == 255 || position >= 4)
        throw std::invalid_argument("Invalid Coliseum challenger selection");
    for (unsigned i = 0; i < roster.membership.size(); ++i) {
        const auto& member = roster.membership[i];
        const auto& character = roster.characters[i];
        if (member.party == roster.active_party && member.position == position && character.properties) {
            selection.actor = *character.properties;
            return;
        }
    }
    throw std::invalid_argument("Empty Coliseum challenger slot");
}
void consume_colosseum_wager(Inventory& inventory, std::uint8_t wager) {
    if (wager == 255) return;
    // C2/49AD scans backwards. Preserve its accumulator change when a stack is
    // emptied; duplicated malformed stacks must not create an unbounded search.
    for (unsigned n = 256; n; --n) {
        auto& stack = inventory.slots[n - 1];
        if (stack.item != wager) continue;
        const auto remaining = static_cast<std::uint8_t>(unsigned{stack.quantity} + 255U);
        if (!remaining || remaining >= 128) {
            stack = {};
            wager = 255;
        } else stack.quantity = remaining;
    }
}
} // namespace ff6
