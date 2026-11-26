#pragma once
#include "ff6/battle_effects.hpp"
#include "ff6/battle_message.hpp"
#include "ff6/battle_menu_transition.hpp"

namespace ff6 {
struct BattleEventAssets {
    Bytes programs, dialogue, dialogue_pointers, monster_dialogue, monster_pointers;
    BattleMenuTransitionAssets windows;
    std::vector<std::uint16_t> attack_properties;
};
BattleEventAssets load_battle_event_assets(const ContentPack& pack);
BattleMessage format_monster_message(const BattleEventAssets& assets, std::uint8_t identity,
                                     const BattleParty& party = {});
// Documented battle-event bytecode is game data. Character animation and text
// remain separate native owners; unknown commands fail at their boundary.
struct BattleEventCharacterChange {
    std::uint8_t character = 0;
    bool menu = false, enabled = false;
};
class BattleScriptedEvent {
  public:
    BattleScriptedEvent(std::shared_ptr<const BattleEventAssets> assets, std::uint8_t identity,
                        std::shared_ptr<const BattleEffectLibrary> effects = {},
                        BattleEffectContext context = {}, std::uint8_t message_speed = 0);
    std::vector<MusicPlayerCommand> advance(BattleActorScene& actors, BattleActorSceneRules& rules,
                 const BattleParty& party, Buttons buttons,
                 const BattleActorSceneAssets& actor_assets = {});
    void compose(RasterScene& scene, const BattleReadoutAssets& windows,
                 const BattleReadoutView& readout, const BattleMessageAssets& font) const;
    BattleActorPaletteOverrides actor_palettes() const;
    void compose_effects(RasterScene& scene, std::span<const BattleActorSpriteGroup> groups) const;
    void publish_suspended(const BattleActorPublication& actors);
    void update_graphics_random_index(std::uint8_t index) {
        animation_->update_graphics_random_index(index);
        if (attack_) attack_->update_graphics_random_index(index);
    }
    std::uint8_t graphics_random_index() const {
        return attack_ ? attack_->context().random_index : animation_->context().random_index;
    }
    BattleEffectDisplay display() const { return attack_ ? attack_->context().display : animation_->context().display; }
    bool menus_disabled() const { return attack_ ? attack_->context().menus_disabled : animation_->context().menus_disabled; }
    std::vector<BattleEventCharacterChange> take_character_changes();
    std::vector<BattleActorAppearanceChange> take_appearance_changes();
    std::uint8_t identity() const {
        return identity_;
    }
    bool complete() const { return complete_; }
    std::uint8_t message_window_uses() const { return message_window_uses_; }
    bool waiting_for_input() const { return key_wait_; }
    const BattleMessage& dialogue() const {
        return text_;
    }

  private:
    std::uint8_t identity_ = 0, message_speed_ = 0;
    std::uint8_t message_window_uses_ = 0;
    std::optional<BattleMessagePresentation> top_message_;
    std::shared_ptr<const BattleEventAssets> assets_;
    std::size_t cursor_ = 0, text_at_ = 0;
    std::shared_ptr<const BattleEffectLibrary> effects_;
    std::optional<BattleAttackAnimation> animation_, attack_;
    std::vector<MusicPlayerCommand> audio_;
    void advance_script(BattleActorScene& actors, BattleActorSceneRules& rules,
                        const BattleParty& party, Buttons buttons);
    std::vector<BattleEventCharacterChange> character_changes_;
    std::vector<BattleActorAppearanceChange> appearance_changes_;
    BattleMessage text_;
    BattleMenuTransition window_motion_;
    bool moving_window_ = false, closing_window_ = false;
    unsigned waits_ = 4;
    bool running_animation_ = false, window_ = false, text_active_ = false,
         key_wait_ = false, confirm_held_ = false, complete_ = false;
    unsigned lines_ = 0, dialogue_scroll_ = 0;
    std::uint8_t dialogue_identity_ = 0;
    Bytes name_glyphs_;
    std::size_t name_at_ = 0;
};
} // namespace ff6
