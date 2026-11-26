#include "ff6/battle_event.hpp"
#include "rom-materials/battle_dialogue_fixed_wait.macros.inc"
#include "rom-materials/battle_dialogue_scroll_step.macros.inc"
#include "rom-materials/battle_dialogue_last_line.macros.inc"
#include "rom-materials/battle_animation_capture_origin.macros.inc"
#include "rom-materials/battle_animation_magitek_origin.macros.inc"
#include <algorithm>

namespace ff6 {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::invalid_argument(why); }
std::size_t program_offset(ByteView data, unsigned pointer) {
    require(pointer >= 0x9800 && pointer - 0x9800U < data.size(), "Battle event pointer outside resource");
    return pointer - 0x9800U;
}
std::size_t message_offset(ByteView data, ByteView pointers, unsigned identity, unsigned base) {
    require(pointers.size() == 512 && identity < 256, "Invalid battle dialogue directory");
    const auto pointer = read16(pointers, identity * 2);
    require(pointer >= base && pointer - base < data.size(), "Battle dialogue pointer outside resource");
    return pointer - base;
}
}
BattleEventAssets load_battle_event_assets(const ContentPack& pack) {
    const auto own = [&](const char* name) { auto data = pack.get(name); return Bytes(data.begin(), data.end()); };
    BattleEventAssets out{own("data_d09800_battle_event_scripts_and_pointers"), own("text_battle_dlg_bin"),
        own("text_battle_dlg_ptr_bin"), own("text_monster_dlg_bin"), own("text_monster_dlg_ptr_bin"), load_battle_menu_transition_assets(pack), {}};
    const auto properties = pack.get("battle_event_animation_properties");
    require(properties.size() == 40, "Invalid battle event animation directory");
    for (unsigned i = 0; i < 20; ++i) {
        const auto value = read16(properties, i * 2);
        require(value % 14 == 0 && value / 14 < 406, "Invalid event animation property");
        out.attack_properties.push_back(value / 14);
    }
    return out;
}
namespace {
BattleMessage format_dialogue_stream(ByteView data, std::size_t at, const BattleParty& party) {
    BattleMessage message;
    const auto read = [&] {
        require(at < data.size(), "Truncated monster dialogue");
        return data[at++];
    };
    for (unsigned work = 0; work < 4096; ++work) {
        const auto value = read();
        if (!value) return message;
        if (value >= 32)
            message.steps.push_back({BattleMessageStepKind::glyph, value});
        else if (value == 1)
            message.steps.push_back({BattleMessageStepKind::newline});
        else if (value == 5 || value == 6)
            message.steps.push_back(
                {BattleMessageStepKind::wait, value == 5 ? std::uint8_t{FF6_ROM_VALUE_BATTLE_DIALOGUE_FIXED_WAIT_0} : read()});
        else if (value == 7)
            message.steps.push_back({BattleMessageStepKind::confirm});
        else if (value == 2) {
            const auto actor = read();
            if (actor < party.dialogue_names.size() && party.dialogue_names[actor])
                for (auto glyph : *party.dialogue_names[actor]) {
                    if (glyph == 255)
                        break;
                    message.steps.push_back({BattleMessageStepKind::glyph, glyph});
                }
        } else
            throw std::invalid_argument("Unsupported monster dialogue control " +
                                        std::to_string(value));
    }
    throw std::invalid_argument("Monster dialogue exceeds instruction limit");
}
} // namespace
BattleMessage format_monster_message(const BattleEventAssets& assets, std::uint8_t identity,
                                     const BattleParty& party) {
    return format_dialogue_stream(
        assets.monster_dialogue,
        message_offset(assets.monster_dialogue, assets.monster_pointers, identity, 0xe1e0), party);
}

BattleScriptedEvent::BattleScriptedEvent(std::shared_ptr<const BattleEventAssets> assets,
                                         std::uint8_t identity,
                                         std::shared_ptr<const BattleEffectLibrary> effects,
                                         BattleEffectContext context, std::uint8_t message_speed)
    : identity_(identity), message_speed_(message_speed), assets_(std::move(assets)),
      effects_(effects ? std::move(effects) : std::make_shared<BattleEffectLibrary>()) {
    require(assets_ && identity < 33 && assets_->programs.size() >= 66, "Invalid battle event identity");
    cursor_ = program_offset(assets_->programs, read16(assets_->programs, unsigned{identity} * 2));
    message_window_uses_ = context.message_window_uses;
    animation_.emplace(effects_, std::shared_ptr<const Bytes>(assets_, &assets_->programs), std::move(context));
}
std::vector<MusicPlayerCommand> BattleScriptedEvent::advance(BattleActorScene& actors, BattleActorSceneRules& rules,
    const BattleParty& party, Buttons buttons, const BattleActorSceneAssets& actor_assets) {
    audio_.clear();
    // Ordinary palette preparation continues between event-animation visits.
    // Retain it across script waits and batches, including costume reloads.
    const auto colors = battle_character_colors(actors, actor_assets);
    animation_->update_character_colors(colors);
    if (attack_)
        attack_->update_character_colors(colors);
    // Terrain shaking continues during dialogue and between animation batches.
    animation_->update_presentation_tick(actors.clock);
    if (attack_) attack_->update_presentation_tick(actors.clock);
    advance_script(actors, rules, party, buttons);
    return std::move(audio_);
}
std::vector<BattleEventCharacterChange> BattleScriptedEvent::take_character_changes() {
    auto changes = std::move(character_changes_);
    character_changes_.clear();
    return changes;
}
std::vector<BattleActorAppearanceChange> BattleScriptedEvent::take_appearance_changes() {
    auto changes = std::move(appearance_changes_);
    appearance_changes_.clear();
    return changes;
}
void BattleScriptedEvent::advance_script(BattleActorScene& actors, BattleActorSceneRules& rules,
                                  const BattleParty& party, Buttons buttons) {
    const bool confirm = held(buttons, Button::a), pressed = confirm && !confirm_held_;
    confirm_held_ = confirm;
    if (complete_) return;
    if (top_message_) {
        top_message_->advance(buttons);
        if (top_message_->complete())
            top_message_.reset();
        return;
    }
    if (waits_) { --waits_; return; }
    if (moving_window_) {
        advance_battle_menu_transition(window_motion_, false);
        moving_window_ = window_motion_.busy();
        if (!moving_window_ && closing_window_) window_ = false;
        return;
    }
    if (running_animation_) {
        auto& effect = attack_ ? *attack_ : *animation_;
        effect.update_presentation_tick(actors.clock);
        const auto result = effect.advance_presentation();
        effect.apply_actor_state(actors, rules);
        appearance_changes_.insert(appearance_changes_.end(), result.appearance_changes.begin(),
                                    result.appearance_changes.end());
        audio_ = effect.take_event_audio();
        if (result.sound) audio_.push_back(*result.sound);
        running_animation_ = !result.complete;
        return;
    }
    if (dialogue_scroll_) {
        dialogue_scroll_ += FF6_ROM_VALUE_BATTLE_DIALOGUE_SCROLL_STEP_0;
        if (dialogue_scroll_ == 16) {
            dialogue_scroll_ = 0;
            const auto first_line = std::ranges::find(text_.glyphs, std::uint8_t{1});
            require(first_line != text_.glyphs.end(), "Dialogue scroll needs a preceding line");
            text_.glyphs.erase(text_.glyphs.begin(), first_line + 1);
            text_.glyphs.push_back(1);
        }
        return;
    }
    if (text_active_) {
        if (key_wait_) { if (!pressed) return; key_wait_ = false; }
        if (name_at_ < name_glyphs_.size()) {
            text_.glyphs.push_back(name_glyphs_[name_at_++]);
            return;
        }
        require(text_at_ < assets_->dialogue.size(), "Truncated battle dialogue");
        const auto op = assets_->dialogue[text_at_++];
        if (!op) { text_active_ = false; return; }
        if (op == 5 || op == 6) {
            if (op == 6) require(text_at_ < assets_->dialogue.size(), "Missing dialogue wait length");
            waits_ = op == 5 ? unsigned{FF6_ROM_VALUE_BATTLE_DIALOGUE_FIXED_WAIT_0} : assets_->dialogue[text_at_++];
            return;
        }
        if (op == 7) { key_wait_ = true; return; }
        if (op == 1) {
            // DlgTextCmd_01 and DlgScroll select the page limit and pixel step.
            // The completed line still spans two eight-pixel font tiles.
            if (lines_ == FF6_ROM_VALUE_BATTLE_DIALOGUE_LAST_LINE_0)
                dialogue_scroll_ = FF6_ROM_VALUE_BATTLE_DIALOGUE_SCROLL_STEP_0;
            else {
                ++lines_;
                text_.glyphs.push_back(1);
            }
            return;
        }
        if (op == 2) {
            require(text_at_ < assets_->dialogue.size(), "Missing battle dialogue name");
            const auto id = assets_->dialogue[text_at_++];
            name_glyphs_.clear();
            name_at_ = 0;
            // C1/5EAC searches all sixteen persistent characters. An absent
            // property contributes no glyphs; battle participation is irrelevant.
            if (id >= party.dialogue_names.size() || !party.dialogue_names[id]) return;
            for (const auto c : *party.dialogue_names[id]) {
                if (c == 255)
                    break;
                name_glyphs_.push_back(c);
            }
            return;
        }
        require(op >= 32, "Unsupported battle dialogue control");
        require(text_.glyphs.size() < 240, "Battle dialogue page exceeds bounds");
        text_.glyphs.push_back(op);
        return;
    }
    const auto read = [&] { require(cursor_ < assets_->programs.size(), "Truncated battle event"); return assets_->programs[cursor_++]; };
    for (unsigned work = 0; work < 128; ++work) {
        const auto op = read();
        if (op == 255) { complete_ = true; return; }
        if (op == 14) {
            auto context = attack_ ? attack_->context() : animation_->context();
            context.message_window_uses = message_window_uses_;
            context.characters = actors.characters.layout.poses;
            for (unsigned i = 0; i < 4; ++i)
                context.character_identities[i] = party.slots[i].actor;
            context.monsters = actors.monsters.positions;
            context.control = actors.animation;
            context.anchors = actors.anchors;
            animation_->begin_event_batch(std::move(context));
            attack_.reset();
            continue;
        }
        if (op == 13) {
            const auto id = read(), identity = read(), targets = read();
            require(id < assets_->attack_properties.size(), "Invalid event attack selection");
            auto context = animation_->context();
            context.message_window_uses = message_window_uses_;
            auto found = std::ranges::find_if(party.slots, [identity](const auto& slot) {
                return slot.actor == identity;
            });
            // C1/FE99 returns slot zero when no identity matches. Global event
            // effects (including the escaping Espers) intentionally use this
            // default selector; they need not have a participating caster.
            context.attacker = found == party.slots.end() ? std::uint8_t{0}
                : static_cast<std::uint8_t>(found - party.slots.begin());
            context.targets.reset();
            if (targets & 128U) {
                context.targets[context.attacker] = true;
                if (targets == 255) context.attacker = 4;
            } else for (unsigned i = 0; i < 6; ++i) context.targets[i + 4] = (targets & (1U << i)) != 0;
            context.hits = context.targets;
            attack_.emplace(effects_, assets_->attack_properties[id], context, initial_battle_effect_images());
            attack_->hold_completed_frame();
            continue;
        }
        if (op >= 3 && op <= 12) {
            const auto identity = read(), low = read(), high = read();
            require(op <= 6 || identity <= 251, "Event monster selector overflows its identity");
            const auto entry = program_offset(assets_->programs, unsigned{low} | (unsigned{high} << 8U));
            require(entry + 2 <= assets_->programs.size(), "Truncated event animation header");
            for (unsigned slot = 0; slot < 256; ++slot) {
                if (op <= 6) {
                    if (slot >= 4 || party.slots[slot].actor != identity) continue;
                } else if (slot != unsigned{identity} + 4U) continue;
                BattleAnimationTask task;
                task.attacker = task.target = static_cast<std::uint8_t>(slot);
                task.flipped = slot < 4 ? (actors.characters.layout.poses[slot].facing & 1U) != 0
                                       : slot < 10 && ((actors.monsters.positions[slot - 4].flip ^ rules.retained_monster_facing[slot - 4] ^ 1U) & 1U) != 0;
                task.rate = static_cast<std::uint8_t>((assets_->programs[entry] >> 4U) + 1);
                task.resume = task.instruction = entry + 2;
                // These scripts sometimes carry an out-of-range, unused monster
                // selector. Global palette/sound tasks need no actor position;
                // the shared executor rejects actor-dependent commands for them.
                if (slot < 10) task.position = {wrap16(unsigned{actors.anchors[slot].center_x} - unsigned{FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0}),
                    wrap16(unsigned{actors.anchors[slot].center_y} - (slot < 4 && rules.magitek ? unsigned{FF6_ROM_VALUE_BATTLE_ANIMATION_MAGITEK_ORIGIN_0} : unsigned{FF6_ROM_VALUE_BATTLE_ANIMATION_CAPTURE_ORIGIN_0}))};
                task.attacker_position = task.target_position = task.position;
                animation_->add_event_task(op - 3, task);
                break;
            }
            continue;
        }
        if (op == 15) {
            // Event char threads and attack layers belong to the same batch.
            // Selecting just the attack drops Blizzard Fist's knockback scripts.
            if (attack_)
                attack_->inherit_event_tasks(*animation_);
            running_animation_ = true;
            return;
        }
        if (op == 19 || op == 20) {
            const auto value = read();
            for (unsigned i = 0; i < 4; ++i)
                if (party.slots[i].actor == (value & 127U)) {
                    character_changes_.push_back(
                        {static_cast<std::uint8_t>(i), op == 20, (value & 128U) == 0});
                    break;
                }
            continue;
        }
        if (op == 17 || op == 16) {
            window_ = true; closing_window_ = op == 16; text_.glyphs.clear();
            begin_battle_menu_transition(window_motion_, assets_->windows.windows[closing_window_ ? 21 : 20],
                assets_->windows.text[2], closing_window_ ? BattleWindowMotion::closing : BattleWindowMotion::opening);
            moving_window_ = true;
            return;
        }
        if (op == 0) {
            const auto entry =
                message_offset(assets_->dialogue, assets_->dialogue_pointers, read(), 0xd200);
            top_message_.emplace(format_dialogue_stream(assets_->dialogue, entry, party),
                                 message_speed_);
            message_window_uses_ = wrap8(unsigned{message_window_uses_} + 1U);
            return;
        }
        if (op == 1) {
            dialogue_identity_ = read();
            text_at_ = message_offset(assets_->dialogue, assets_->dialogue_pointers, dialogue_identity_, 0xd200);
            text_.glyphs.clear(); lines_ = 0; text_active_ = true; return;
        }
        throw std::invalid_argument("Unsupported battle event command " + std::to_string(op));
    }
    throw std::invalid_argument("Battle event exceeded command limit");
}
BattleActorPaletteOverrides BattleScriptedEvent::actor_palettes() const {
    return attack_ ? attack_->actor_palettes() : animation_->actor_palettes();
}
void BattleScriptedEvent::publish_suspended(const BattleActorPublication& actors) {
    auto& effect = attack_ ? *attack_ : *animation_;
    effect.update_character_frames(actors.character_frames);
    effect.update_status_tiles(actors.status_tiles);
    effect.publish_suspended();
}
void BattleScriptedEvent::compose_effects(RasterScene& scene, std::span<const BattleActorSpriteGroup> groups) const {
    if (attack_) attack_->compose(scene, groups);
    else animation_->compose(scene, groups);
}
void BattleScriptedEvent::compose(RasterScene& scene, const BattleReadoutAssets& windows,
                                  const BattleReadoutView& readout, const BattleMessageAssets& font) const {
    if (top_message_) {
        compose_battle_message(scene, windows, readout, font, top_message_->message(), false,
                               top_message_->scroll());
    }
    if (!window_) return;
    try {
        compose_battle_message(scene, windows, readout, font, text_, true, dialogue_scroll_);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("Battle dialogue " + std::to_string(dialogue_identity_) +
                                    ": " + error.what());
    }
    auto& layer = scene.layers.back();
    for (unsigned y = 0; y < layer.lines.size(); ++y) {
        const auto& binding = window_motion_.windows[y];
        auto& line = layer.lines[y];
        line.enabled = binding.canvas == BattleWindowCanvas::list;
        if (line.enabled) { line.offset_x = binding.offset_x; line.offset_y = binding.offset_y; }
    }
}
} // namespace ff6
