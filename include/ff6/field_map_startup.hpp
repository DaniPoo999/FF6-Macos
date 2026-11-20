#pragma once
#include "ff6/field_state.hpp"

namespace ff6 {
// Table entries are offsets in the extracted event data language, not CPU code.
std::uint32_t field_map_startup_event(const ContentPack& pack, std::uint16_t map);
// Saves the selected object's movement only for a nonempty startup script.
// This installs the native continuation, but does not run scene/object work.
bool begin_field_map_startup(FieldState& state, FieldEventRunner& runner, std::uint32_t entry);
} // namespace ff6
