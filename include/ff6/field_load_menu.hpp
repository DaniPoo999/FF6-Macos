#pragma once
#include "ff6/field_loop.hpp"
#include "ff6/original_sram.hpp"

namespace ff6 {
// Complete only the source's immediate no-save branch. An accepted header needs
// the selection owner, even when every slot is empty, and returns false unchanged.
// All validation, defaults, retained state and AB completion commit together.
bool complete_field_load_without_save(FieldLoopSeed& retained, OriginalSram& storage,
                                      const FieldLoopAssets& assets);
} // namespace ff6
