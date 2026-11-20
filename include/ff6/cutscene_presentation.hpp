#pragma once
#include "ff6/music_player.hpp"

namespace ff6 {
struct CutscenePresentationTick {
    bool interval = false, input_sampled = false, image_changed = false;
    std::vector<MusicPlayerCommand> audio;
};
} // namespace ff6
