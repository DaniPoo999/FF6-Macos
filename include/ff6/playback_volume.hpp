#pragma once
#include <cstdint>

namespace ff6 {
// Ordinary game effects exclude separately owned system effects. Ramps count
// native slow-control updates; zero duration assigns the level immediately.
enum class PlaybackGroup : std::uint8_t { music, effects, music_and_effects };
struct PlaybackVolumeRamp {
    PlaybackGroup group = PlaybackGroup::music;
    std::uint8_t duration = 0, target = 255;
    bool operator==(const PlaybackVolumeRamp&) const = default;
};
} // namespace ff6
