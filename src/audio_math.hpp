#pragma once
#include <cstdint>

namespace ff6::audio_math {
// Callers use fixed shifts <= 11. Widen before negation so even INT32_MIN is safe.
constexpr std::int32_t floor_shift(std::int32_t value, unsigned shift) {
    const std::int64_t divisor = std::int64_t{1} << shift;
    const std::int64_t wide = value;
    return static_cast<std::int32_t>(wide >= 0 ? wide / divisor
                                               : -((-wide + divisor - 1) / divisor));
}
} // namespace ff6::audio_math
