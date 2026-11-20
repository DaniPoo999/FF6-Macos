#include "ff6/field_map_startup.hpp"

namespace ff6 {
std::uint32_t field_map_startup_event(const ContentPack& pack, std::uint16_t map) {
    const auto entries = pack.get("ptr_table_d1fa00_map_init_events");
    if (entries.size() != 512 * 3 || map >= 512)
        throw std::invalid_argument("Invalid map startup-event table or identity");
    const auto entry = read24(entries, unsigned{map} * 3U);
    if (entry >= field_event_script(pack).size())
        throw std::invalid_argument("Map startup event is outside the extracted script");
    return entry;
}
bool begin_field_map_startup(FieldState& state, FieldEventRunner& runner, std::uint32_t entry) {
    // Stage both owners: a missing player must not leave an installed wrapper
    // or disabled user control. Startup preserves unrelated flags and movement.
    auto next_state = state;
    auto next_runner = runner;
    FieldFlagAccess flags(next_state);
    if (!next_runner.begin_map_startup(entry, flags))
        return false;
    const auto player = next_state.field.update_order[0];
    if (!player || *player > FieldObjects::camera)
        throw std::invalid_argument("Map startup needs a valid party object");
    auto& object = next_state.field.objects[*player];
    if (object.movement.mode >= 16 || object.movement.options >= 16)
        throw std::invalid_argument("Map startup has invalid player movement");
    object.saved_movement = object.movement;
    state = std::move(next_state);
    runner = std::move(next_runner);
    return true;
}
} // namespace ff6
