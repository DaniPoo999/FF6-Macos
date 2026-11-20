#pragma once
#include "ff6/field_map.hpp"

namespace ff6 {
struct FieldScrollAxis {
    std::uint32_t position = 0; // Unsigned 24.8; display uses low sixteen integer bits.
    std::uint8_t center = 0;    // Retained metatile center, not recomputed from the origin.
    std::int16_t follow_rate = 0, background_rate = 0;
    bool operator==(const FieldScrollAxis&) const = default;
};
struct FieldScroll {
    std::array<std::array<FieldScrollAxis, 2>, 3> layers; // x,y for each background.
    bool operator==(const FieldScroll&) const = default;
};
struct FieldScrollLimit {
    std::uint8_t minimum = 0, maximum = 255;
    bool operator==(const FieldScrollLimit&) const = default;
};
using FieldScrollLimits = std::array<FieldScrollLimit, 2>;
FieldScrollLimits field_scroll_limits(const FieldMapDefinition& definition);
FieldScroll initialize_field_scroll(const FieldMapDefinition& definition,
                                    const FieldParallax& parallax, std::uint8_t x, std::uint8_t y);
struct FieldScrollFocus {
    bool locked = false, caller_is_camera = false;
    std::array<std::uint8_t, 2> party_tile{};
    std::array<std::int16_t, 2> party_speed{}, camera_speed{};
};
// Select follow rates on an action boundary; independent background rates persist.
void update_field_scroll_rate(FieldScroll& scroll, const FieldScrollLimits& limits,
                              const std::array<std::uint8_t, 4>& multipliers,
                              const FieldScrollFocus& focus);
// One logical advancement, independent of presentation refresh. Notifications are
// x,y crossings per background; original transfer queues are not production state.
std::array<std::array<bool, 2>, 3> advance_field_scroll(FieldScroll& scroll);
std::array<FieldOrigin, 3> field_scroll_origins(const FieldScroll& scroll);
} // namespace ff6
