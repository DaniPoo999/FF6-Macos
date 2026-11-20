#pragma once
#include "ff6/codecs.hpp"

namespace ff6 {
// Native sample-decoder history. Loop callers must retain it between blocks.
struct BrrHistory {
    std::int16_t previous = 0;
    std::int16_t older = 0;
};
std::array<std::int16_t, 16> decode_brr_block(const BrrBlock& block, BrrHistory& history);
// One traversal only: no envelope, interpolation, looping, mixer or sequencing.
std::vector<std::int16_t> decode_brr_pass(ByteView sample);
} // namespace ff6
