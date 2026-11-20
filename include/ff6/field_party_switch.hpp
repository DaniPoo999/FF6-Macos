#pragma once
#include "ff6/field_loop.hpp"

namespace ff6 {
// Shared by Y-button cycling and collision-event restoration. Candidate order
// comes from enabled field members, while positions and vitals stay with each
// party. An absent target cycles from the currently controlled party.
bool switch_field_party(FieldLoopSeed& seed, std::optional<std::uint8_t> target = std::nullopt);
bool consume_field_party_control(FieldLoopSeed& seed);
} // namespace ff6
