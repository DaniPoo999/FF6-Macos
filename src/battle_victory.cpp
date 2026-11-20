#include "ff6/battle_victory.hpp"
#include <algorithm>
#include "rom-materials/battle_message_first_wait.macros.inc"
#include "rom-materials/battle_message_wait_steps.macros.inc"
#include <stdexcept>
#include "rom-materials/battle_victory_choices.macros.inc"
namespace ff6 {
// Extraction checks every consecutive table difference before supplying the scale.

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::invalid_argument(message); }
}
BattleVictoryAssets load_battle_victory_assets(const ContentPack& pack) {
    return {load_battle_message_assets(pack), load_battle_character_animation_content(pack, true)};
}
BattleVictoryMusic battle_victory_music(bool victory_enabled, bool formation_continues_music,
                                       bool field_continues_music) {
    // C1/0117: only the field event's continuation flag suppresses fanfare.
    // C1/00AF: either continuation flag preserves the song on return, unless
    // victory actually replaced it with fanfare (the original $6284 marker).
    const bool fanfare = victory_enabled && !field_continues_music;
    return {fanfare, fanfare || !(formation_continues_music || field_continues_music)};
}
BattleVictory::BattleVictory(std::shared_ptr<const BattleVictoryAssets> assets,
                              const BattleRewards& rewards, const BattleParty& party,
                              const BattleActorScene& actors, const BattleActorSceneRules& rules,
                              std::uint8_t message_speed, bool animate, BattleVictoryMusic music)
    : assets_(std::move(assets)), message_wait_(FF6_ROM_VALUE_BATTLE_MESSAGE_FIRST_WAIT_0 +
          (message_speed & 7U) * FF6_ROM_VALUE_BATTLE_MESSAGE_WAIT_STEPS_0),
      music_(music), animate_(animate) {
    require(assets_ != nullptr, "Missing victory assets");
    std::array<BattleAnimationParticipant, 4> participants{};
    for (unsigned i = 0; i < 4; ++i)
        participants[i] = {party.slots[i].graphics.value_or(255), party.slots[i].present,
                           party.slots[i].ai.entry.has_value(), actors.characters.control[i].active_status};
    characters_ = make_battle_character_entry_animation(assets_->characters, participants,
        actors.characters.layout.poses, actors.anchors, rules.magitek, true, actors.animation);
    const auto add = [&](std::uint8_t message, std::uint32_t first, std::uint32_t second) {
        messages_.push_back(format_battle_message(assets_->messages, message, {first, second, 0, 0}, party));
    };
    if (rewards.experience) add(39, rewards.experience, 0);
    if (rewards.announce_magic_points) add(53, rewards.magic_points, 0);
    // C2/5E92: each character's level message precedes their learned spells.
    for (unsigned reverse = 4; reverse; --reverse) {
        const auto slot = static_cast<std::uint8_t>(reverse - 1);
        if (std::ranges::find(rewards.levelled_characters, slot) != rewards.levelled_characters.end())
            add(46, 0, slot);
        for (const auto learned : rewards.learned_spells)
            if (learned.character == slot) add(50, learned.spell, slot);
    }
    for (const auto lore : rewards.learned_lores) add(45, lore, 0);
    if (rewards.learned_dance) add(40, 0, 0);
    if (rewards.dispelled_cursed_shield) add(42, 0, 0); // C2/5F00, before drops/gil.
    for (const auto item : rewards.items) add(item.quantity > 1 ? 33 : 32, item.item, item.quantity);
    if (rewards.gil) add(38, 0, rewards.gil);
}
std::vector<MusicPlayerCommand> BattleVictory::advance(const BattleActorSceneAssets& assets,
                                                       BattleActorScene& actors,
                                                       BattleActorSceneRules& rules, Buttons buttons) {
    const bool held_confirm = held(buttons, Button::a);
    const bool pressed = held_confirm && !confirm_held_;
    confirm_held_ = held_confirm;
    std::vector<MusicPlayerCommand> audio;
    if (finished_) return audio;
    if (waits_) --waits_;
    else if (!started_) {
        started_ = true;
        if (music_.fanfare) audio.push_back(GameSong{FF6_ROM_VALUE_BATTLE_VICTORY_CHOICES_0, 255, false});
    } else if (animate_ && characters_.boundary != BattleAnimationBoundary::complete) {
        characters_.control = actors.animation;
        const auto boundary = advance_battle_character_animation(assets_->characters.programs,
                                    characters_, actors.characters.layout.poses, 4096);
        require(boundary != BattleAnimationBoundary::work_limit, "Victory task exceeded command limit");
        actors.animation = characters_.control;
        resume_battle_character_animation(characters_);
    } else if (!settled_) {
        settled_ = true;
        waits_ = FF6_ROM_VALUE_BATTLE_VICTORY_CHOICES_2;
    } else if (!messages_started_) {
        messages_started_ = true;
        waits_ = messages_.empty() || messages_[0].wait_for_confirm ? 0 : message_wait_;
    } else if (message_ < messages_.size()) {
        if (!messages_[message_].wait_for_confirm || pressed) {
            ++message_;
            waits_ = message_ < messages_.size() && !messages_[message_].wait_for_confirm ? message_wait_ : 0;
        }
    } else {
        if (music_.fade_on_return)
            audio.push_back(PlaybackVolumeRamp{PlaybackGroup::music, FF6_ROM_VALUE_BATTLE_VICTORY_CHOICES_1, 0});
        finished_ = true;
    }
    publish_battle_actor_scene(actors, assets, rules.hide_characters);
    prepare_battle_actor_scene(assets, rules, actors);
    return audio;
}
void BattleVictory::compose(RasterScene& scene, const BattleReadoutAssets& assets,
                            const BattleReadoutView& readout) const {
    if (finished_) { scene.intensity.assign(scene.height, 0); return; }
    if (!messages_started_ || message_ >= messages_.size()) return;
    compose_battle_message(scene, assets, readout, assets_->messages, messages_[message_]);
}
} // namespace ff6
