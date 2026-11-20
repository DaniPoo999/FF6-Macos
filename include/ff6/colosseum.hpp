#pragma once
#include "ff6/content.hpp"
#include "ff6/inventory.hpp"
#include "ff6/party.hpp"

namespace ff6 {
struct ColosseumWager {
    std::uint8_t monster = 0, prize = 255;
    bool hidden_prize = false;
};
using ColosseumData = std::array<ColosseumWager, 256>;
ColosseumData decode_colosseum_data(ByteView bytes);
struct ColosseumSelection {
    std::uint8_t wager = 255, prize = 255, monster = 0, actor = 255;
    bool shadow = false;
    bool operator==(const ColosseumSelection&) const = default;
};
// Item reservation is semantic: the original removes the wager only on battle
// return. Cancellation leaves inventory untouched; equipped-only items cannot
// be wagered. Character identity is the roster's persistent property identity.
ColosseumSelection select_colosseum_wager(const ColosseumData&, const Inventory&,
                                          unsigned slot, bool shadow_waiting);
void select_colosseum_challenger(ColosseumSelection&, const Roster&, unsigned position);
void consume_colosseum_wager(Inventory&, std::uint8_t wager);
} // namespace ff6
