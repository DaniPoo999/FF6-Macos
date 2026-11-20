#pragma once
#include "ff6/field_session.hpp"

namespace ff6 {
// InitNewGame's represented effects, separate from Reset and the later load
// menu's InitSaveSlot. Preserve fields this routine does not initialize, notably
// active party, gil, steps, RNG, saved positions and the last 128 treasure flags.
// Returns a replacement so malformed resources cannot partly reset the caller.
FieldLoopSeed initialize_field_new_game(FieldLoopSeed retained, const ContentPack& pack,
                                        const PartyData& party);
FieldMapArrival decode_field_boot_arrival(ByteView data);
// Native cold-start policy supplies zero for persistent values that Reset does
// not write. This is a defined starting profile, not a claim about power-on RAM.
// The actual extracted root script and initial destination are used; no title
// request is acknowledged and no opening character is pre-created.
FieldLoopSeed make_initial_field_seed(const ContentPack& pack);
FieldSession make_initial_field_session(const ContentPack& pack);
} // namespace ff6
