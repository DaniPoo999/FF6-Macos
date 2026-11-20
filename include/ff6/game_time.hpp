#pragma once
#include <cstdint>

namespace ff6 {
struct GameTime {
    std::uint8_t hours = 0, minutes = 0, seconds = 0;
    bool operator==(const GameTime&) const = default;
};
// One owned game-clock interval, independent of host refresh and audio buffers.
// Equality-only rollovers and byte wrapping preserve the original routine even
// for unusual retained values. File validation belongs to the save reader.
void advance_game_time(GameTime& time, std::uint8_t& frames);
} // namespace ff6
