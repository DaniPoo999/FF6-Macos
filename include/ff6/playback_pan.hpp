#pragma once
#include <cstdint>

namespace ff6 {
// Ordinary game-effect pan, shared by event callers and the native sequencer.
// Duration counts slow-control updates; zero assigns the unsigned target now.
struct EffectPanRamp {
    std::uint8_t duration = 0, target = 128;
    bool operator==(const EffectPanRamp&) const = default;
};
} // namespace ff6
