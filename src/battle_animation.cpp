#include "ff6/battle_animation.hpp"
#include "rom-materials/battle_animation_capture_origin.macros.inc"
#include "rom-materials/battle_animation_magitek_origin.macros.inc"

#include <stdexcept>

namespace ff6 {
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::invalid_argument(why);
}
std::uint16_t add(std::uint16_t value, int delta) {
    return wrap16(static_cast<std::uint32_t>(static_cast<std::int32_t>(value) + delta));
}
std::pair<int, int> displacement(std::uint8_t command, bool flipped) {
    constexpr std::array<int, 8> horizontal{-1, 0, 1, -1, 1, -1, 0, 1};
    constexpr std::array<int, 8> vertical{1, 1, 1, 0, 0, -1, -1, -1};
    const auto direction = unsigned{command} >> 5U;
    const int distance = (command & 31U) + 1;
    return {horizontal[direction] * distance * (flipped ? -1 : 1), vertical[direction] * distance};
}
struct AnimationActors {
    std::array<BattleCharacterPose, 4>* characters = nullptr;
    std::array<BattleMonsterPosition, 6>* monsters = nullptr;
    const std::array<std::uint8_t, 6>* monster_facing = nullptr;
    const BattleCharacterMotionData* motion = nullptr;
    bool magitek = false;
    BattleCharacterPose& character(std::uint8_t slot) const {
        require(characters && slot < 4, "Character animation owner is unavailable");
        return (*characters)[slot];
    }
    BattleMonsterPosition& monster(std::uint8_t participant) const {
        require(monsters && participant >= 4 && participant < 10,
                "Monster animation owner is unavailable");
        return (*monsters)[participant - 4U];
    }
    bool flipped(std::uint8_t participant) const {
        if (participant < 4)
            return (character(participant).facing & 1U) != 0;
        require(monster_facing, "Monster animation facing is unavailable");
        return ((monster(participant).flip ^ (*monster_facing)[participant - 4U] ^ 1U) & 1U) != 0;
    }
};
void move_actor(BattleAnimationTask& task, AnimationActors actors, std::uint8_t command,
                bool target) {
    const auto slot = target ? task.target : task.attacker;
    const auto [dx, dy] = displacement(command, task.flipped);
    if (slot < 4) {
        auto& pose = actors.character(slot);
        pose.animation_x = add(pose.animation_x, dx);
        pose.offset_y = add(pose.offset_y, dy);
    } else {
        auto& pose = actors.monster(slot);
        pose.x = add(pose.x, dx);
        pose.y = add(pose.y, dy);
    }
    auto& point = target ? task.target_position : task.attacker_position;
    point.x = add(point.x, dx);
    // Both character and monster up/right helpers add to the retained task y,
    // while subtracting from the actor y. Preserve this source-backed quirk.
    point.y = add(point.y, dx > 0 && dy < 0 ? -dy : dy);
}
void event_command(std::uint8_t command, BattleAnimationTask& task, BattleAnimationControl& control,
                   AnimationActors actors) {
    require(task.attacker < 4, "Character event command needs a character attacker");
    auto& pose = actors.character(task.attacker);
    if (command == 2) {
        control.saved_positions[task.attacker] = {pose.x, pose.y};
    } else if (command == 3) {
        const auto saved = control.saved_positions[task.attacker];
        pose.x = saved.x;
        pose.y = saved.y;
        pose.offset_x = pose.animation_x = pose.offset_y = pose.jump_y = 0;
    } else if (command == 1) {
        pose.x = wrap16(std::uint32_t{pose.x} + pose.offset_x + pose.animation_x);
        pose.y = wrap16(std::uint32_t{pose.y} + pose.offset_y + pose.jump_y);
        task.attacker_position = {pose.x, pose.y};
        pose.offset_x = pose.animation_x = pose.offset_y = pose.jump_y = 0;
    } else if (command == 4) {
        task.target_position = control.saved_positions[task.attacker];
    } else if (command == 7 || command == 9) {
        require(actors.motion, "Character vector direction needs motion tables");
        const auto angle = battle_vector_angle(actors.motion->angles, task.attacker_position, task.target_position);
        const auto& actions = command == 7 ? actors.motion->walking : actors.motion->jumping;
        pose.action = actions[(angle >> 5U) + (task.flipped ? 8U : 0U)];
    } else {
        throw std::invalid_argument("Unsupported battle character event animation command " +
                                    std::to_string(command));
    }
}
// Return true when this task yields a frame or terminates. No CPU instructions
// are represented: these bytes are the game's documented animation language.
bool command(ByteView programs, BattleAnimationTask& task, BattleAnimationControl& control,
             AnimationActors actors) {
    const auto read = [&]() {
        require(task.instruction < programs.size(), "Truncated battle animation program");
        return programs[task.instruction++];
    };
    const auto op = read();
    if (op < 128) {
        task.effect_frame = op;
        task.resume = task.instruction;
        return true;
    }
    if (op == 255) {
        task.active = false;
        task.effect_frame.reset();
        // Original termination does not publish a new resume pointer.
        return true;
    }
    switch (op) {
    case 0x80: {
        require(read() == 0x1a, "Unsupported extended battle animation command");
        // The original character branch returns before consuming the operand.
        // Reject that unsupported caller rather than silently desynchronizing it.
        require(task.target >= 4 && task.target < 10,
                "Monster entry visibility needs a monster target");
        actors.monster(task.target);
        control.monster_entry_visible[task.target - 4U] = (read() & 1U) != 0;
        break;
    }
    case 0x81:
    case 0x82: {
        const auto left = read(), right = read();
        const auto slot = op == 0x81 ? task.attacker : task.target;
        require(slot < 4, "Character frame command needs its character owner");
        // Animation direction and the actor's current facing are separate.
        actors.character(slot).frame_override = actors.flipped(slot) ? right : left;
        control.force_character_frame = slot;
        break;
    }
    case 0x83: {
        const auto [dx, dy] = displacement(read(), task.flipped);
        task.position.x = add(task.position.x, dx);
        task.position.y = add(task.position.y, dy);
        break;
    }
    case 0x84:
        // Unlike the header, this argument is NOT incremented by the handler.
        task.rate = read();
        break;
    case 0x85:
        task.position = task.attacker_position;
        break;
    case 0x86:
    case 0x87:
        move_actor(task, actors, read(), op == 0x87);
        break;
    case 0x89:
        task.loop_count = read();
        task.loop_body = task.instruction;
        break;
    case 0x8a:
        if (task.loop_count != 0) {
            require(task.loop_body.has_value(), "Battle animation loop has no start");
            --task.loop_count;
            task.instruction = *task.loop_body;
        }
        break;
    case 0x9a:
        task.flipped = actors.flipped(task.target);
        break;
    case 0xbf: {
        const auto low = read(), high = read();
        const auto destination = unsigned{low} | (unsigned{high} << 8U);
        require(destination >= 0x9800 && destination - 0x9800U < programs.size(),
                "Character animation call leaves its data resource");
        task.return_to = task.instruction;
        task.instruction = destination - 0x9800U;
        break;
    }
    case 0xc0:
        require(task.return_to.has_value(), "Character animation return has no call");
        task.instruction = *task.return_to;
        break;
    case 0xc7: {
        const auto event = read();
        if (event == 6) {
            auto& pose = actors.character(task.attacker);
            pose.x = read(); pose.y = read();
            pose.offset_x = pose.offset_y = pose.animation_x = pose.jump_y = 0;
        } else event_command(event, task, control, actors);
        break;
    }
    case 0xd2: {
        const auto x = read(), y = read();
        task.target_position = {static_cast<std::uint16_t>(task.flipped ? 255U - x : x), y};
        break;
    }
    case 0xe7:
        task.line = make_battle_character_line(task.attacker_position, task.target_position, actors.magitek);
        break;
    case 0xe5:
    case 0xe6: {
        const auto speed = read(), backward = read(), height = read();
        require(task.line && actors.motion, "Character vector needs initialized motion");
        if (!advance_battle_character_line(*task.line, actors.character(task.attacker), task.offset,
                                           actors.motion->sine, speed, height, op == 0xe6)) {
            require(backward <= task.instruction - 2, "Character vector branch underflow");
            task.instruction -= 2 + backward;
        }
        break;
    }
    case 0xc8:
        require(task.attacker < 4, "Character action needs a character attacker");
        actors.character(task.attacker).action = read();
        break;
    case 0xd1:
        control.fixed_draw_order = read() != 0;
        break;
    case 0xe1:
    case 0xf1: {
        const auto flags = read();
        const auto slot = op == 0xe1 ? task.attacker : task.target;
        require(slot < 4, "Visibility command needs its character owner");
        if ((flags & 128U) == 0)
            control.visible[slot] = (flags & 1U) != 0;
        break;
    }
    case 0xf9: {
        const auto action = read(), target = read(), override = read();
        const auto slot = (override & 128U) != 0 ? override & 3U
                          : target != 0          ? unsigned{task.target}
                                                 : unsigned{task.attacker};
        require(slot < 4, "Armor action needs its character owner");
        control.armor_actions[slot] = action;
        break;
    }
    default:
        throw std::invalid_argument("Unsupported battle animation command " + std::to_string(op));
    }
    return false;
}
} // namespace
BattleCharacterAnimationContent load_battle_character_animation_content(const ContentPack& pack, bool victory) {
    const auto raw = pack.get("data_d09800_battle_event_scripts_and_pointers");
    require(raw.size() >= 66, "Truncated battle event directory");
    const auto normalize = [&](std::uint16_t address) -> std::size_t {
        require(address >= 0x9800 && address - 0x9800U < raw.size(),
                "Battle animation pointer leaves its owned resource");
        return address - 0x9800U;
    };
    auto at = normalize(read16(raw, victory ? 2 : 0));
    require(at + 52 <= raw.size() && raw[at] == 14 && raw[at + 1] == 18 && raw[at + 50] == 15 &&
                raw[at + 51] == 255,
            "Unsupported character-entry event program");
    BattleCharacterAnimationContent content;
    content.programs.assign(raw.begin(), raw.end());
    content.motion = std::make_shared<BattleCharacterMotionData>(load_battle_character_motion(pack));
    for (unsigned i = 0; i < 24; ++i)
        content.entries[i] = normalize(read16(raw, at + 2 + i * 2));
    return content;
}
std::optional<std::uint8_t> select_battle_character_entry(BattleAnimationParticipant character,
                                                          bool magitek, bool victory) {
    if (!character.present || character.graphics == 255)
        return std::nullopt;
    if (magitek && !victory)
        return 22;
    if (character.ai || (character.status & (status_mask(BattleStatus::dead) |
                                             status_mask(BattleStatus::petrify))) != 0)
        return 23;
    const auto entry = character.graphics & 31U;
    require(entry < 24, "Character entry selects an unowned script table entry");
    return static_cast<std::uint8_t>(entry);
}
BattleCharacterAnimation
make_battle_character_entry_animation(const BattleCharacterAnimationContent& content,
                                      const std::array<BattleAnimationParticipant, 4>& participants,
                                      const std::array<BattleCharacterPose, 4>& poses,
                                      const BattleSceneAnchors& anchors, bool magitek, bool victory,
                                      BattleAnimationControl retained) {
    BattleCharacterAnimation state;
    state.motion = content.motion;
    state.magitek = magitek;
    // Task initialization does not clear caller-owned visibility, armor actions
    // or saved positions. This also permits an explicitly retained scene.
    state.control = std::move(retained);
    for (std::uint8_t slot = 0; slot < 4; ++slot) {
        const auto entry = select_battle_character_entry(participants[slot], magitek, victory);
        if (!entry)
            continue;
        const auto at = content.entries[*entry];
        require(at < content.programs.size() && content.programs.size() - at >= 2,
                "Truncated character animation header");
        const auto alignment = content.programs[at + 1] & 0xe0U;
        require(alignment == 0xe0U || (victory && (alignment == 0x20 || alignment == 0)),
                "Character entry needs unsupported animation alignment");
        BattleAnimationTask task;
        task.attacker = task.target = slot;
        task.flipped = (poses[slot].facing & 1U) != 0;
        task.rate = static_cast<std::uint8_t>((content.programs[at] >> 4U) + 1U);
        task.resume = task.instruction = at + 2;
        task.position = {add(anchors[slot].center_x, -FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0),
                         add(anchors[slot].center_y, magitek ? -FF6_ROM_VALUE_BATTLE_ANIMATION_MAGITEK_ORIGIN_0 : -FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0)};
        task.attacker_position = task.target_position = task.position;
        state.tasks[slot] = task;
    }
    return state;
}
namespace {
template <std::size_t Slots>
BattleAnimationBoundary advance_sequence(ByteView programs, BattleAnimationSequence<Slots>& next,
                                         AnimationActors actors, std::size_t command_budget) {
    require(next.next_task <= Slots, "Invalid animation task continuation");
    while (next.next_task < Slots) {
        auto& selected = next.tasks[next.next_task];
        if (!selected || !selected->active) {
            ++next.next_task;
            next.task_started = false;
            continue;
        }
        auto& task = *selected;
        if (!next.task_started) {
            ++next.active_visits;
            task.counter = wrap8(255U + task.counter);
            if (task.counter != 0) {
                ++next.next_task;
                continue;
            }
            task.counter = task.rate;
            task.instruction = task.resume;
            next.task_started = true;
        }
        if (command_budget == 0)
            break;
        --command_budget;
        if (command(programs, task, next.control, actors)) {
            ++next.next_task;
            next.task_started = false;
        }
    }
    if (next.next_task == Slots)
        next.boundary = BattleAnimationBoundary::frame_wait;
    return next.boundary;
}
template <std::size_t Slots> void resume_sequence(BattleAnimationSequence<Slots>& state) {
    require(state.boundary == BattleAnimationBoundary::frame_wait,
            "Animation resumed without a pending frame wait");
    if (state.active_visits == 0) {
        state.control.fixed_draw_order = false;
        state.boundary = BattleAnimationBoundary::complete;
    } else {
        state.next_task = state.active_visits = 0;
        state.task_started = false;
        state.boundary = BattleAnimationBoundary::work_limit;
    }
}
} // namespace
BattleAnimationBoundary
advance_battle_character_animation(ByteView programs, BattleCharacterAnimation& state,
                                   std::array<BattleCharacterPose, 4>& poses,
                                   std::size_t command_budget) {
    if (state.boundary != BattleAnimationBoundary::work_limit || command_budget == 0)
        return state.boundary;
    auto next = state;
    auto next_poses = poses;
    advance_sequence(programs, next, {&next_poses, nullptr, nullptr, state.motion.get(), state.magitek}, command_budget);
    state = std::move(next);
    poses = std::move(next_poses);
    return state.boundary;
}
void resume_battle_character_animation(BattleCharacterAnimation& state) {
    resume_sequence(state);
}
BattleMonsterSpriteDefinition load_battle_monster_sprite_definition(const ContentPack& pack,
                                                                    std::uint16_t property) {
    const auto properties = pack.get("data_d07fb2_attack_anim_prop");
    const auto at = std::size_t{property} * 14U;
    require(at <= properties.size() && properties.size() - at >= 14,
            "Unowned battle animation property");
    const auto script = read16(properties, at);
    require(script < 0x8000 && read16(properties, at + 2) == 0xffff &&
                read16(properties, at + 4) == 0xffff && read16(properties, at + 11) == 0xffff,
            "Monster sprite animation needs unsupported background/extra tasks");
    const auto pointers = pack.get("ptr_table_d1ead8_attack_animation");
    const auto graphics = pack.get("data_d4d000_attack_gfx_prop");
    require(std::size_t{script} * 2U + 2U <= pointers.size() &&
                std::size_t{script} * 6U + 6U <= graphics.size(),
            "Unowned monster animation script/graphics identity");
    const auto programs = pack.get("data_d00000_attack_animation_scripts");
    const auto entry = read16(pointers, std::size_t{script} * 2U);
    require(std::size_t{entry} + 2U <= programs.size(), "Truncated monster animation header");
    return {Bytes(programs.begin(), programs.end()),
            entry,
            graphics[std::size_t{script} * 6U + 4U],
            graphics[std::size_t{script} * 6U + 5U],
            properties[at + 13],
            properties[at + 10],
            properties[at + 9],
            properties[at + 6]};
}
BattleMonsterAnimationTasks make_battle_monster_animation(
    const BattleMonsterSpriteDefinition& definition, std::bitset<6> targets, std::uint8_t attacker,
    const BattleSceneAnchors& anchors, const std::array<BattleMonsterPosition, 6>& monsters,
    const std::array<std::uint8_t, 6>& retained_facing, BattleAnimationControl retained) {
    require(attacker < 6 && definition.entry <= definition.programs.size() &&
                definition.programs.size() - definition.entry >= 2,
            "Invalid monster animation task request");
    // These initialization functions allocate extra tasks for each target.
    // They must not accidentally take this one-primary-task path.
    require((definition.initialization & 0x7fU) != 2 && (definition.initialization & 0x7fU) != 5,
            "Monster animation requires multiple tasks per target");
    const auto alignment = definition.programs[definition.entry + 1] & 0xe0U;
    require(alignment != 0x80 && alignment != 0xa0 && alignment != 0xc0,
            "Monster animation requires unsupported front/screen alignment");
    const auto point = [&](unsigned slot) -> BattleAnimationPoint {
        const auto& anchor = anchors[slot + 4U];
        auto y = anchor.center_y;
        if (alignment == 0)
            y = add(anchor.bottom_y, 8 - int(wrap8(unsigned{definition.height} * 8U)));
        else if (alignment == 0x40)
            y = monsters[slot].y;
        return {anchor.center_x, y};
    };
    BattleMonsterAnimationTasks state;
    state.control = std::move(retained);
    std::uint8_t delay = 1;
    for (unsigned slot = 0; slot < 6; ++slot) {
        if (!targets[slot])
            continue;
        BattleAnimationTask task;
        task.attacker = static_cast<std::uint8_t>(attacker + 4U);
        task.target = static_cast<std::uint8_t>(slot + 4U);
        task.flipped = ((monsters[attacker].flip ^ retained_facing[attacker] ^ 1U) & 1U) != 0;
        task.rate = static_cast<std::uint8_t>((definition.programs[definition.entry] >> 4U) + 1U);
        task.counter = delay;
        delay = wrap8(unsigned{delay} + definition.target_delay);
        task.resume = task.instruction = definition.entry + 2U;
        task.position = task.target_position = point(slot);
        task.attacker_position = point(attacker);
        state.tasks[slot] = task;
    }
    return state;
}
BattleAnimationBoundary
advance_battle_monster_animation(ByteView programs, BattleMonsterAnimationTasks& state,
                                 std::array<BattleMonsterPosition, 6>& monsters,
                                 const std::array<std::uint8_t, 6>& retained_facing,
                                 std::size_t command_budget) {
    if (state.boundary != BattleAnimationBoundary::work_limit || command_budget == 0)
        return state.boundary;
    auto next = state;
    auto next_monsters = monsters;
    advance_sequence(programs, next, {nullptr, &next_monsters, &retained_facing}, command_budget);
    state = std::move(next);
    monsters = std::move(next_monsters);
    return state.boundary;
}
void resume_battle_monster_animation(BattleMonsterAnimationTasks& state) {
    resume_sequence(state);
}
} // namespace ff6
