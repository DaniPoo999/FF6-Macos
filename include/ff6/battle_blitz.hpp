#pragma once
#include "rom-materials/battle_blitz_initial_deadline.macros.inc"
#include "ff6/content.hpp"
#include "ff6/input.hpp"

namespace ff6 {
struct BattleBlitzCode {
    std::array<Buttons, 10> inputs{};
    std::uint8_t length = 0; // Includes the terminal confirmation button.
};
using BattleBlitzCodes = std::array<BattleBlitzCode, 8>;
BattleBlitzCodes load_battle_blitz_codes(const ContentPack& pack);
// C1/6FF5: input samples are changes of the held-button chord, rather than
// directional edges. An idle deadline invalidates the first captured chord.
struct BattleBlitzInput {
    std::array<Buttons, 16> chords{};
    Buttons previous = 0;
    std::uint8_t cursor = 0, deadline = FF6_ROM_VALUE_BATTLE_BLITZ_INITIAL_DEADLINE_0;
    void sample(Buttons held);
    std::uint8_t match(const BattleBlitzCodes& codes) const;
};
} // namespace ff6
