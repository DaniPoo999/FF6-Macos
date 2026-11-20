#include "ff6/audio.hpp"
#include "audio_math.hpp"
#include <algorithm>

namespace ff6 {
using audio_math::floor_shift;

std::array<std::int16_t, 16> decode_brr_block(const BrrBlock& block, BrrHistory& history) {
    if (block.shift > 15 || block.filter > 3) {
        throw std::runtime_error("Invalid BRR shift or filter");
    }
    std::array<std::int16_t, 16> output{};
    for (std::size_t i = 0; i < output.size(); ++i) {
        const std::int32_t residual = block.residuals[i];
        if (residual < -8 || residual > 7) {
            throw std::runtime_error("Invalid BRR residual");
        }
        // Large shifts have a special sign-only result; they are not clamped to 12.
        std::int32_t sample =
            block.shift <= 12
                ? floor_shift(residual * static_cast<std::int32_t>(1U << block.shift), 1)
                : (residual < 0 ? -2048 : 0);
        const std::int32_t previous = history.previous;
        const auto older_half = floor_shift(history.older, 1);
        switch (block.filter) {
        case 0:
            break;
        case 1:
            sample += floor_shift(previous, 1) + floor_shift(-previous, 5);
            break;
        case 2:
            sample +=
                previous - older_half + floor_shift(older_half, 4) + floor_shift(-3 * previous, 6);
            break;
        case 3:
            sample += previous - older_half + floor_shift(-13 * previous, 7) +
                      floor_shift(3 * older_half, 4);
            break;
        }
        sample = std::clamp(sample, -32768, 32767);
        // The clipped value doubles with signed 16-bit wrap. Widen and mask so
        // the original saturation/wrap behavior never requires host signed UB.
        const auto doubled = static_cast<std::uint32_t>(sample + 32768) * 2U;
        const auto result = static_cast<std::int16_t>(signed16(wrap16(doubled)));
        history.older = history.previous;
        history.previous = result;
        output[i] = result;
    }
    return output;
}

std::vector<std::int16_t> decode_brr_pass(ByteView sample) {
    const auto blocks = decode_brr(sample);
    std::vector<std::int16_t> result;
    result.reserve(blocks.size() * 16);
    BrrHistory history;
    for (const auto& block : blocks) {
        const auto pcm = decode_brr_block(block, history);
        result.insert(result.end(), pcm.begin(), pcm.end());
    }
    return result;
}
} // namespace ff6
