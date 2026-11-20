#include "ff6/field_contact.hpp"
#include "rom-materials/field_contact_major_speed.macros.inc"

namespace ff6 {
namespace {
void require(bool value, const char* why) {
    if (!value)
        throw std::invalid_argument(why);
}
void valid_actor(const FieldObject& actor) {
    require(actor.x <= 0xffffff && actor.y <= 0xffffff && actor.facing < 4 &&
                actor.membership.party < 8 && actor.movement.mode < 16 &&
                actor.movement.options < 16,
            "Invalid contact actor state");
}
std::array<std::int16_t, 2> rates(std::array<int, 2> difference) {
    const unsigned dx = static_cast<unsigned>(difference[0] < 0 ? -difference[0] : difference[0]);
    const unsigned dy = static_cast<unsigned>(difference[1] < 0 ? -difference[1] : difference[1]);
    const unsigned major = dy < dx ? 0 : 1;
    const unsigned minor = major ^ 1U;
    const unsigned large = major == 0 ? dx : dy, small = major == 0 ? dy : dx;
    require(large >= 2, "Contact camera rate requires a nontrivial distance");
    std::array<unsigned, 2> magnitude{};
    magnitude[major] = FF6_ROM_VALUE_FIELD_CONTACT_MAJOR_SPEED_0;
    // The dividend is narrowed before division. At a tied distance of -128 on
    // both axes, 128 * 512 wraps to zero, leaving a zero horizontal rate.
    magnitude[minor] = ((small * FF6_ROM_VALUE_FIELD_CONTACT_MAJOR_SPEED_0) & 65535U) / large;
    return {static_cast<std::int16_t>(difference[0] < 0 ? -static_cast<int>(magnitude[0])
                                                        : static_cast<int>(magnitude[0])),
            static_cast<std::int16_t>(difference[1] < 0 ? -static_cast<int>(magnitude[1])
                                                        : static_cast<int>(magnitude[1]))};
}
} // namespace
bool activate_field_contact(FieldState& state, FieldActionContext& context,
                            FieldEventRunner& events, const FieldPoseTables& poses,
                            std::uint32_t idle_entry) {
    if (context.collision.stage != FieldCollision::Stage::detected)
        return false;
    const auto selected = state.field.update_order[0];
    require(selected && *selected <= FieldObjects::camera, "Contact needs a selected actor");
    const auto& before = state.field.objects[*selected];
    require(before.x <= 0xffffff && before.y <= 0xffffff, "Invalid selected contact position");
    if (before.x % 4096 != 0 || before.y % 4096 != 0)
        return false;
    const auto hit = context.collision;
    require(hit.character < 16 && hit.npc >= 16 && hit.npc <= FieldObjects::camera &&
                hit.direction < 4 && state.roster.active_party < 8,
            "Invalid contact identities");
    auto next = state;
    auto world = context;
    auto event = events;
    auto& character = next.field.objects[hit.character];
    auto& npc = next.field.objects[hit.npc];
    valid_actor(character);
    valid_actor(npc);
    valid_actor(next.field.objects[*selected]);
    require(!npc.special_graphics, "Special graphics do not own a decoded contact script");
    world.collision.stage = FieldCollision::Stage::activated;
    // These coordinates are byte-truncated integer positions, not map-wrapped
    // occupancy cells. The selected and contacted actors can be different.
    world.contact.scroll_target = {wrap8(character.x >> 12U), wrap8(character.y >> 12U)};
    const std::array<int, 2> difference{signed8(wrap8(unsigned{world.contact.scroll_target.x} +
                                                      256U - world.scroll.layers[0][0].center)),
                                        signed8(wrap8(unsigned{world.contact.scroll_target.y} +
                                                      256U - world.scroll.layers[0][1].center))};
    if (difference[0] < -1 || difference[0] > 1 || difference[1] < -1 || difference[1] > 1) {
        const auto speed = rates(difference);
        for (auto& layer : world.scroll.layers)
            for (std::size_t axis = 0; axis < 2; ++axis) {
                layer[axis].background_rate = speed[axis];
                layer[axis].follow_rate = 0;
            }
    }
    // A near contact keeps both sets of rates until the later event scroll gate.
    character.saved_facing = character.facing;
    character.facing = static_cast<std::uint8_t>((hit.direction + 2U) & 3U);
    character.pending_pose = poses.standing[character.facing];
    world.contact.default_party = next.roster.active_party;
    next.roster.active_party = character.membership.party;
    character.movement.mode = 4;
    character.saved_movement = character.movement;
    // The first selector is retained despite the original comment saying four.
    for (std::size_t i = 1; i < next.field.party_selectors.size(); ++i)
        next.field.party_selectors[i].reset();
    npc.action_wait = 0;
    npc.facing = hit.direction;
    npc.pending_pose = poses.standing[npc.facing];
    npc.saved_movement = npc.movement;
    npc.movement.mode = 4;
    event.begin_contact_event(npc.interaction_data, idle_entry);
    auto& player = next.field.objects[*selected];
    player.saved_movement = player.movement;
    player.movement.mode = 4;
    next.field.sort_pending = true;
    state = std::move(next);
    context = std::move(world);
    events = std::move(event);
    return true;
}
bool field_contact_scroll_arrived(const FieldActionContext& context) {
    const std::array<std::uint8_t, 2> target{context.contact.scroll_target.x,
                                             context.contact.scroll_target.y};
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const auto center = context.scroll.layers[0][axis].center;
        const auto delta = wrap8(unsigned{target[axis]} + 256U - center);
        if (delta != 0 && delta != 1 && delta != 255)
            return false;
    }
    return true;
}
EventYield advance_field_contact_event(FieldState& state, FieldActionContext& context,
                                       FieldEventRunner& events, EventInputs input,
                                       std::size_t command_budget) {
    input.scroll_arrived = field_contact_scroll_arrived(context);
    auto result = advance_field_event(state, events, input, command_budget);
    if (result != EventYield::operation ||
        !std::holds_alternative<EventStopScroll>(events.pending_request()->operation))
        return result;
    // Sorting, background readiness, input-flag updates and higher-priority waits
    // have already run. Clear only autonomous rates; retained follow rates stay.
    const auto identity = events.pending_request()->identity;
    for (auto& layer : context.scroll.layers)
        for (auto& axis : layer)
            axis.background_rate = 0;
    FieldFlagAccess flags(state);
    events.complete_request(identity, flags);
    return advance_field_event(state, events, input, command_budget);
}
} // namespace ff6
